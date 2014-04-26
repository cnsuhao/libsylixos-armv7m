/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                SylixOS(TM)  LW : long wing
**
**                               Copyright All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: dtrace.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2011 年 11 月 18 日
**
** 描        述: SylixOS 调试跟踪器, GDB server 可以使用此调试跟踪器调试装载的模块或者进程.

** BUG:
2012.09.05  今天凌晨, 重新设计 dtrace 的等待与接口机制, 开始为 GDB server 的编写扫平一切障碍.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
#include "signal.h"
/*********************************************************************************************************
  dtrace 调试原理
  
  每一个 DTRACE_NODE 都保存一个 ptrace 节点相关信息. GDB server 首先创建一个 dtrace 节点, 然后可以创建断点
  
  创建一个断点就是让程序运行到指定的位置(一个地址)时产生异常, 例如, 通过 dtrace_read 先将原始的指令读出来,
  
  然后再用 dtrace_write 写入一条可以导致异常的指令, 当程序运行到这里, 就会产生异常. 操作系统会逐一查找每个
  
  创建了的节点, 然后调用 DTRACE_pfuncTrap() 来判断是哪一个 dtrace 节点创建的节点 (由 GDB server 实现), 
  
  如果找到, GDB server 应该判断此断点是否有效, 如果有效, 则应记录相关信息, 然后返回 0, DTRACE_pfuncTrap() 
  
  返回 0 说明这是一个正常的断点, 此时, dtrace_trap() 会将 "断点" 的线程停止, 并且向 GDB server 发送 SIGTRAP
  
  信号, 然后 GDB server 读取刚才 DTRACE_pfuncTrap() 记录的信息, 并通知 gdb 有事件产生. 
  
  注意: 如果异常类型是其他错误 (定义在 vmm.h 中), GDB server 则需要向 gdb 做适当的信息反馈!
  
  当断点执行完毕需要继续执行时, GDB server 首先需要还原断点的指令, 进行一次汇编单步, 然后再在刚才的位置设置
  
  断点并清除当前断点, 然后再执行, 如果体系结构支持硬件单步, 例如: x86 PowerPC 等处理器, 则使用硬件单步即可, 
  
  如果不支持如 ARM MIPS 则需要 GDB server 有能力进行软件单步, (需要进行软件的分支预测)
  
  注意: 如果统一断点打断了多个线程, DTRACE_pfuncTrap() 将会被多次调用! 而只要 GDB server 调用 
  
  dtrace_continue() 会使所有的线程继续执行.
*********************************************************************************************************/

/*********************************************************************************************************
  dtrace 结构
  
  以下信息由操作系统异常处理管理函数填写
  DTRACE_ulTraceThread
  DTRACE_pvFrame
  DTRACE_ulAddress
*********************************************************************************************************/
typedef struct {
    LW_LIST_LINE        DTRACE_lineManage;                              /*  管理链表                    */
    LW_OBJECT_HANDLE    DTRACE_ulHostThread;                            /*  主控线程                    */
    
    BOOL              (*DTRACE_pfuncTrap)(PVOID  pvArg, 
                                          PVOID  pvFrame, 
                                          addr_t ulAddress, 
                                          ULONG  ulType,
                                          LW_OBJECT_HANDLE ulThread);   /*  硬件异常陷入                */
    PVOID               DTRACE_pvArg;                                   /*  DTRACE_pfuncIsTrap 参数     */
    
    LW_OBJECT_HANDLE    DTRACE_ulStopSem;                               /*  所有被断点暂停的线程信号量  */
} DTRACE_NODE;
typedef DTRACE_NODE    *PDTRACE_NODE;
/*********************************************************************************************************
  dtrace 全局变量
*********************************************************************************************************/
static LW_LIST_LINE_HEADER      _G_plineDtraceHeader;
static LW_OBJECT_HANDLE         _G_ulDtraceLock;

#define __DTRACE_LOCK()         API_SemaphoreMPend(_G_ulDtraceLock, LW_OPTION_WAIT_INFINITE)
#define __DTRACE_UNLOCK()       API_SemaphoreMPost(_G_ulDtraceLock)
/*********************************************************************************************************
** 函数名称: dtrace_create
** 功能描述: 创建一个 dtrace 调试节点
** 输　入  : pfuncTrap         断点陷入函数
**           pvArg             断点陷入函数参数
** 输　出  : dtrace 节点
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
PVOID dtrace_create (BOOL (*pfuncTrap)(), PVOID pvArg)
{
    PDTRACE_NODE    pdtrace;

    if (pfuncTrap == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (LW_NULL);
    }
    
    if (_G_ulDtraceLock == LW_OBJECT_HANDLE_INVALID) {
        _G_ulDtraceLock =  API_SemaphoreMCreate("dtrace_lock", 
                                                LW_PRIO_DEF_CEILING, 
                                                LW_OPTION_INHERIT_PRIORITY | 
                                                LW_OPTION_DELETE_SAFE |
                                                LW_OPTION_OBJECT_GLOBAL, 
                                                LW_NULL);
    }
    
    pdtrace = (PDTRACE_NODE)__SHEAP_ALLOC(sizeof(DTRACE_NODE));
    if (pdtrace == LW_NULL) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system heap is low memory.\r\n");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (LW_NULL);
    }
    lib_bzero(pdtrace, sizeof(DTRACE_NODE));
    
    pdtrace->DTRACE_ulHostThread = API_ThreadIdSelf();
    pdtrace->DTRACE_pfuncTrap    = pfuncTrap;
    pdtrace->DTRACE_pvArg        = pvArg;
    
    /*
     *  DTRACE_ulStopSem 信号量可被信号打断恢复执行 (必须使用 FIFO 型等待)
     */
    pdtrace->DTRACE_ulStopSem = API_SemaphoreBCreate("dtrace_stopsem", LW_FALSE, 
                                                     LW_OPTION_SIGNAL_INTER | 
                                                     LW_OPTION_WAIT_FIFO |
                                                     LW_OPTION_OBJECT_GLOBAL, 
                                                     LW_NULL);
    if (pdtrace->DTRACE_ulStopSem == LW_OBJECT_HANDLE_INVALID) {
        __SHEAP_FREE(pdtrace);
        return  (LW_NULL);
    }
    
    __DTRACE_LOCK();
    _List_Line_Add_Ahead(&pdtrace->DTRACE_lineManage, &_G_plineDtraceHeader);
    __DTRACE_UNLOCK();
    
    return  ((PVOID)pdtrace);
}
/*********************************************************************************************************
** 函数名称: dtrace_delete
** 功能描述: 删除一个 dtrace 节点
** 输　入  : pvDtrace      dtrace 节点
** 输　出  : ERROR or NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT dtrace_delete (PVOID pvDtrace)
{
    PDTRACE_NODE    pdtrace;

    if (pvDtrace == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    pdtrace = (PDTRACE_NODE)pvDtrace;
    
    __DTRACE_LOCK();
    _List_Line_Del(&pdtrace->DTRACE_lineManage, &_G_plineDtraceHeader);
    __DTRACE_UNLOCK();
    
    API_SemaphoreBDelete(&pdtrace->DTRACE_ulStopSem);
    
    __SHEAP_FREE(pdtrace);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: dtrace_read
** 功能描述: dtrace 读取被跟踪线程指定内存数据
** 输　入  : pvDtrace      dtrace 节点
**           ulAddress     地址
**           pvBuffer      读取缓冲
**           stNbytes      大小
** 输　出  : ERROR or NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT dtrace_read (PVOID pvDtrace, addr_t ulAddress, PVOID pvBuffer, size_t  stNbytes)
{
    /*
     *  由于 SylixOS 共享统一的地址空间, 所以, 这里直接拷贝即可
     */
    if (pvBuffer == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    lib_memcpy(pvBuffer, (const PVOID)ulAddress, stNbytes);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: dtrace_write
** 功能描述: dtrace 设置被跟踪线程指定内存数据
** 输　入  : pvDtrace      dtrace 节点
**           ulAddress     地址
**           pvBuffer      输出缓冲
**           stNbytes      大小
** 输　出  : ERROR or NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT dtrace_write (PVOID pvDtrace, addr_t ulAddress, const PVOID pvBuffer, size_t  stNbytes)
{
    if (pvBuffer == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    lib_memcpy((PVOID)ulAddress, pvBuffer, stNbytes);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: dtrace_continue
** 功能描述: dtrace 使被跟踪线程继续执行
** 输　入  : pvDtrace      dtrace 节点
** 输　出  : ERROR or NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT dtrace_continue (PVOID pvDtrace)
{
    PDTRACE_NODE    pdtrace;

    if (pvDtrace == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    pdtrace = (PDTRACE_NODE)pvDtrace;
    
    API_SemaphoreBFlush(pdtrace->DTRACE_ulStopSem, LW_NULL);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: dtrace_continue_one
** 功能描述: dtrace 使最早停止的线程继续执行
** 输　入  : pvDtrace      dtrace 节点
** 输　出  : ERROR or NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT dtrace_continue_one (PVOID pvDtrace)
{
    PDTRACE_NODE    pdtrace;

    if (pvDtrace == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    pdtrace = (PDTRACE_NODE)pvDtrace;
    
    API_SemaphoreBPost(pdtrace->DTRACE_ulStopSem);
    API_SemaphoreBPend(pdtrace->DTRACE_ulStopSem, LW_OPTION_NOT_WAIT);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
  一下函数一般为操作系统使用, 操作系统在异常时会调用这个函数.
*********************************************************************************************************/
/*********************************************************************************************************
** 函数名称: dtrace_trap
** 功能描述: 操作系统异常产生时, 通知 dtrace. (此函数运行于被异常中断线程的异常处理当中)
** 输　入  : pvFrame       trap 异常线程句柄栈区指针, 体系结构相关
**           ulAddress     trap 地址
**           ulType        trap 类型
**           ulThread      trap 异常线程句柄
** 输　出  :  0 表示是断点, 
             -1 表示不是断点.
              1 表示断点已经恢复, 继续执行
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT dtrace_trap (PVOID              pvFrame,
                 addr_t             ulAddress, 
                 ULONG              ulType,
                 LW_OBJECT_HANDLE   ulThread)
{
    INT                 iRet = PX_ERROR;
    PLW_LIST_LINE       plineTemp;
    PDTRACE_NODE        pdtrace;
    LW_OBJECT_HANDLE    ulHostThread;
    
    if (_G_ulDtraceLock == LW_OBJECT_HANDLE_INVALID) {
        _G_ulDtraceLock =  API_SemaphoreMCreate("dtrace_lock", 
                                                LW_PRIO_DEF_CEILING, 
                                                LW_OPTION_INHERIT_PRIORITY | 
                                                LW_OPTION_DELETE_SAFE |
                                                LW_OPTION_OBJECT_GLOBAL, 
                                                LW_NULL);
    }

    __DTRACE_LOCK();
    for (plineTemp  = _G_plineDtraceHeader; 
         plineTemp != LW_NULL; 
         plineTemp  = _list_line_get_next(plineTemp)) {                 /*  遍历所有控制块              */
        
        pdtrace = _LIST_ENTRY(plineTemp, DTRACE_NODE, DTRACE_lineManage);
        
        iRet = pdtrace->DTRACE_pfuncTrap(pdtrace->DTRACE_pvArg, 
                                         pvFrame,
                                         ulAddress, 
                                         ulType, 
                                         ulThread);
        if (iRet > 0) {                                                 /*  此断点已恢复, 可以继续执行  */
            plineTemp = LW_NULL;
            break;
        
        } else if (iRet == 0) {                                         /*  断点有效, 需要暂停当前线程  */
            ulHostThread = pdtrace->DTRACE_ulHostThread;                /*  主控线程                    */
            break;
        }
    }
    __DTRACE_UNLOCK();
    
    if (plineTemp == LW_NULL) {
        return  (iRet);
    }
    
    /*
     *  发送 SIGTRAP 信号.
     */
    kill(ulHostThread, SIGTRAP);                                        /*  通知 GDB Server             */
    
    API_SemaphoreBPend(pdtrace->DTRACE_ulStopSem, 
                       LW_OPTION_WAIT_INFINITE);                        /*  等待继续执行                */
    
    return  (iRet);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
