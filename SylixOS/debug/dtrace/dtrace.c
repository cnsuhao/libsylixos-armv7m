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
** 描        述: SylixOS 调试跟踪器, GDB server 可以使用此调试进程.

** BUG:
2012.09.05  今天凌晨, 重新设计 dtrace 的等待与接口机制, 开始为 GDB server 的编写扫平一切障碍.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
#include "dtrace.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_GDB_EN > 0
#include "../SylixOS/loader/include/loader_vppatch.h"
/*********************************************************************************************************
  dtrace 结构
*********************************************************************************************************/
typedef struct {
    UINT                DMSG_uiIn;
    UINT                DMSG_uiOut;
    UINT                DMSG_uiNum;
    LW_DTRACE_MSG       DMSG_dmsgBuffer[LW_CFG_MAX_THREADS];
} LW_DTRACE_MSG_POOL;

typedef struct {
    LW_LIST_LINE        DTRACE_lineManage;                              /*  管理链表                    */
    pid_t               DTRACE_pid;                                     /*  进程描述符                  */
    UINT                DTRACE_uiType;                                  /*  调试类型                    */
    UINT                DTRACE_uiFlag;                                  /*  调试选项                    */
    LW_DTRACE_MSG_POOL  DTRACE_dmsgpool;                                /*  调试暂停线程信息            */
    LW_OBJECT_HANDLE    DTRACE_ulDbger;                                 /*  调试器                      */
} LW_DTRACE;
typedef LW_DTRACE      *PLW_DTRACE;

#define DPOOL_IN        pdtrace->DTRACE_dmsgpool.DMSG_uiIn
#define DPOOL_OUT       pdtrace->DTRACE_dmsgpool.DMSG_uiOut
#define DPOOL_NUM       pdtrace->DTRACE_dmsgpool.DMSG_uiNum
#define DPOOL_MSG       pdtrace->DTRACE_dmsgpool.DMSG_dmsgBuffer
/*********************************************************************************************************
  dtrace 结构
*********************************************************************************************************/
static LW_LIST_LINE_HEADER  _G_plineDtraceHeader = LW_NULL;
/*********************************************************************************************************
** 函数名称: __dtraceWriteMsg
** 功能描述: 插入一个调试信息
** 输　入  : pdtrace       dtrace 节点
**           dmsg          调试信息
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __dtraceWriteMsg (PLW_DTRACE  pdtrace, const PLW_DTRACE_MSG  pdmsg)
{
    if (DPOOL_NUM == LW_CFG_MAX_THREADS) {
        return  (PX_ERROR);
    }
    
    DPOOL_MSG[DPOOL_IN] = *pdmsg;
    DPOOL_IN++;
    DPOOL_NUM++;
    
    if (DPOOL_IN >= LW_CFG_MAX_THREADS) {
        DPOOL_IN =  0;
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __dtraceReadMsg
** 功能描述: 读取一个调试信息
** 输　入  : pdtrace       dtrace 节点
**           dmsg          调试信息
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __dtraceReadMsg (PLW_DTRACE  pdtrace, PLW_DTRACE_MSG  pdmsg)
{
    if (DPOOL_NUM == 0) {
        return  (PX_ERROR);
    }
    
    while (DPOOL_MSG[DPOOL_OUT].DTM_ulThread == LW_OBJECT_HANDLE_INVALID) {
        DPOOL_OUT++;
        if (DPOOL_OUT >= LW_CFG_MAX_THREADS) {
            DPOOL_OUT =  0;
        }
    }
    
    *pdmsg = DPOOL_MSG[DPOOL_OUT];
    DPOOL_MSG[DPOOL_OUT].DTM_ulThread = LW_OBJECT_HANDLE_INVALID;
    DPOOL_OUT++;
    DPOOL_NUM--;
    
    if (DPOOL_OUT >= LW_CFG_MAX_THREADS) {
        DPOOL_OUT =  0;
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __dtraceReadMsgEx
** 功能描述: 读取一个调试信息
** 输　入  : pdtrace       dtrace 节点
**           dmsg          调试信息
**           ulThread      指定的线程 ID
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __dtraceReadMsgEx (PLW_DTRACE  pdtrace, PLW_DTRACE_MSG  pdmsg, LW_OBJECT_HANDLE  ulThread)
{
    UINT    uiI = DPOOL_OUT;

    if (DPOOL_NUM == 0) {
        return  (PX_ERROR);
    }
    
    while (DPOOL_MSG[uiI].DTM_ulThread != ulThread) {
        uiI++;
        if (uiI >= LW_CFG_MAX_THREADS) {
            uiI =  0;
        }
        if (uiI == DPOOL_IN) {
            return  (PX_ERROR);                                         /*  没有对应线程信息            */
        }
    }
    
    *pdmsg = DPOOL_MSG[uiI];
    DPOOL_MSG[uiI].DTM_ulThread = LW_OBJECT_HANDLE_INVALID;
    DPOOL_NUM--;
    
    if (DPOOL_OUT == uiI) {
        DPOOL_OUT++;
        if (DPOOL_OUT >= LW_CFG_MAX_THREADS) {
            DPOOL_OUT =  0;
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __dtraceThread
** 功能描述: 获取一个线程的句柄
** 输　入  : pdtrace       dtrace 节点
**           dmsg          调试信息
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __dtraceThreadCb (LW_OBJECT_HANDLE  ulId, LW_OBJECT_HANDLE  ulThread[], 
                               UINT  uiTableNum, UINT *uiNum)
{
    if (*uiNum < uiTableNum) {
        ulThread[*uiNum] = ulId;
        (*uiNum)++;
    }
}
/*********************************************************************************************************
** 函数名称: API_DtraceTrap
** 功能描述: 有一个线程触发断点
** 输　入  : ulAddr        触发地址
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_DtraceTrap (addr_t  ulAddr)
{
#if LW_CFG_MODULELOADER_EN > 0
    LW_OBJECT_HANDLE    ulDbger;
    PLW_LIST_LINE       plineTemp;
    LW_LD_VPROC        *pvproc;
    PLW_CLASS_TCB       ptcbCur;
    PLW_DTRACE          pdtrace;
    LW_DTRACE_MSG       dtm;
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);
    
    pvproc = __LW_VP_GET_TCB_PROC(ptcbCur);
    if (!pvproc || (pvproc->VP_pid <= 0)) {
        return  (PX_ERROR);
    }
    
    dtm.DTM_ulAddr   = ulAddr;
    dtm.DTM_ulThread = ptcbCur->TCB_ulId;
    
    __KERNEL_ENTER();                                                   /*  进入内核                    */
    for (plineTemp  = _G_plineDtraceHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {                 /*  查找对应的调试器            */
        pdtrace = _LIST_ENTRY(plineTemp, LW_DTRACE, DTRACE_lineManage);
        if (pdtrace->DTRACE_pid == pvproc->VP_pid) {
            ulDbger = pdtrace->DTRACE_ulDbger;
            __dtraceWriteMsg(pdtrace, &dtm);
            break;
        }
    }
    __KERNEL_EXIT();                                                    /*  退出内核                    */
    
    if (plineTemp == LW_NULL) {                                         /*  此进程不存在调试器          */
        return  (PX_ERROR);
    
    } else {
        _DebugHandle(__LOGMESSAGE_LEVEL, "dtrace trap.\r\n");
        killTrap(ulDbger);                                              /*  通知调试器线程              */
        return  (ERROR_NONE);
    }
#else
    return  (PX_ERROR);
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
}
/*********************************************************************************************************
** 函数名称: API_DtraceCreate
** 功能描述: 创建一个 dtrace 调试节点
** 输　入  : uiType            调试类型
**           uiFlag            调试选项
**           ulDbger           调试器服务线程
** 输　出  : dtrace 节点
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
PVOID  API_DtraceCreate (UINT  uiType, UINT  uiFlag, LW_OBJECT_HANDLE  ulDbger)
{
    PLW_DTRACE  pdtrace;
    
    pdtrace = (PLW_DTRACE)__SHEAP_ALLOC(sizeof(LW_DTRACE));
    if (pdtrace == LW_NULL) {
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (LW_NULL);
    }
    lib_bzero(pdtrace, sizeof(LW_DTRACE));
    
    pdtrace->DTRACE_pid     = (pid_t)PX_ERROR;
    pdtrace->DTRACE_uiType  = uiType;
    pdtrace->DTRACE_uiFlag  = uiFlag;
    pdtrace->DTRACE_ulDbger = ulDbger;
    
    __KERNEL_ENTER();
    _List_Line_Add_Ahead(&pdtrace->DTRACE_lineManage, &_G_plineDtraceHeader);
    __KERNEL_EXIT();
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "dtrace create.\r\n");
    return  ((PVOID)pdtrace);
}
/*********************************************************************************************************
** 函数名称: API_DtraceDelete
** 功能描述: 删除一个 dtrace 调试节点
** 输　入  : pvDtrace      dtrace 节点
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceDelete (PVOID  pvDtrace)
{
    PLW_DTRACE  pdtrace = (PLW_DTRACE)pvDtrace;
    
    if (!pdtrace) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    __KERNEL_ENTER();
    _List_Line_Del(&pdtrace->DTRACE_lineManage, &_G_plineDtraceHeader);
    __KERNEL_EXIT();
    
    __SHEAP_FREE(pdtrace);
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "dtrace delete.\r\n");
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceSetPid
** 功能描述: 设置 dtrace 跟踪进程
** 输　入  : pvDtrace          dtrace 节点
**           pid               被调试进程号
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceSetPid (PVOID  pvDtrace, pid_t  pid)
{
    PLW_DTRACE  pdtrace = (PLW_DTRACE)pvDtrace;
    
    if (!pdtrace) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    pdtrace->DTRACE_pid = pid;
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceGetRegs
** 功能描述: 获取指定调试线程的寄存器上下文
** 输　入  : pvDtrace      dtrace 节点
**           ulThread      线程句柄
**           pregctx       寄存器表
**           pregSp        堆栈指针
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceGetRegs (PVOID  pvDtrace, LW_OBJECT_HANDLE  ulThread, 
                          ARCH_REG_CTX  *pregctx, ARCH_REG_T *pregSp)
{
    REGISTER UINT16         usIndex;
    REGISTER PLW_CLASS_TCB  ptcb;
    REGISTER ARCH_REG_T     regSp;
    
    PLW_DTRACE  pdtrace = (PLW_DTRACE)pvDtrace;
    
    if (!pdtrace || !pregctx) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    usIndex = _ObjectGetIndex(ulThread);
    
    if (!_ObjectClassOK(ulThread, _OBJECT_THREAD)) {                    /*  检查 ID 类型有效性          */
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    
    if (_Thread_Index_Invalid(usIndex)) {                               /*  检查线程有效性              */
        return  (ERROR_THREAD_NULL);
    }
    
    __KERNEL_ENTER();                                                   /*  进入内核                    */
    if (_Thread_Invalid(usIndex)) {
        __KERNEL_EXIT();                                                /*  退出内核                    */
        return  (ERROR_THREAD_NULL);
    }
    
    ptcb = _K_ptcbTCBIdTable[usIndex];
    
    *pregctx = *(ARCH_REG_CTX *)ptcb->TCB_pstkStackNow;
    regSp    =  (ARCH_REG_T)ptcb->TCB_pstkStackNow;
    
#if	CPU_STK_GROWTH == 0
    regSp -= sizeof(ARCH_REG_CTX);
#else
    regSp += sizeof(ARCH_REG_CTX);
#endif

    *pregSp = regSp;
    __KERNEL_EXIT();                                                    /*  退出内核                    */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceSetRegs
** 功能描述: 设置指定调试线程的寄存器上下文
** 输　入  : pvDtrace      dtrace 节点
**           ulThread      线程句柄
**           pregctx       寄存器表
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceSetRegs (PVOID  pvDtrace, LW_OBJECT_HANDLE  ulThread, const ARCH_REG_CTX  *pregctx)
{
    REGISTER UINT16         usIndex;
    REGISTER PLW_CLASS_TCB  ptcb;
    
    PLW_DTRACE  pdtrace = (PLW_DTRACE)pvDtrace;
    
    if (!pdtrace || !pregctx) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    usIndex = _ObjectGetIndex(ulThread);
    
    if (!_ObjectClassOK(ulThread, _OBJECT_THREAD)) {                    /*  检查 ID 类型有效性          */
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    
    if (_Thread_Index_Invalid(usIndex)) {                               /*  检查线程有效性              */
        return  (ERROR_THREAD_NULL);
    }
    
    __KERNEL_ENTER();                                                   /*  进入内核                    */
    if (_Thread_Invalid(usIndex)) {
        __KERNEL_EXIT();                                                /*  退出内核                    */
        return  (ERROR_THREAD_NULL);
    }
    
    ptcb = _K_ptcbTCBIdTable[usIndex];
    
    *(ARCH_REG_CTX *)ptcb->TCB_pstkStackNow = *pregctx;
    
    __KERNEL_EXIT();                                                    /*  退出内核                    */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceGetMems
** 功能描述: 拷贝内存
** 输　入  : pvDtrace      dtrace 节点
**           ulAddr        拷贝内存起始地址
**           pvBuffer      接收缓冲
**           stSize        拷贝的大小
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceGetMems (PVOID  pvDtrace, addr_t  ulAddr, PVOID  pvBuffer, size_t  stSize)
{
    if (!pvBuffer) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    lib_memcpy(pvBuffer, (const PVOID)ulAddr, stSize);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceSetMems
** 功能描述: 写入内存
** 输　入  : pvDtrace      dtrace 节点
**           ulAddr        写入内存起始地址
**           pvBuffer      写入数据
**           stSize        写入的大小
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceSetMems (PVOID  pvDtrace, addr_t  ulAddr, const PVOID  pvBuffer, size_t  stSize)
{
    if (!pvBuffer) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    lib_memcpy((PVOID)ulAddr, pvBuffer, stSize);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceBreakpointInsert
** 功能描述: 插入一个断点
** 输　入  : pvDtrace      dtrace 节点
**           ulAddr        断点地址
**           pulIns        返回断点地址之前的指令
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceBreakpointInsert (PVOID  pvDtrace, addr_t  ulAddr, ULONG  *pulIns)
{
    PLW_DTRACE    pdtrace = (PLW_DTRACE)pvDtrace;
    
    if (!(pdtrace->DTRACE_uiFlag & LW_DTRACE_F_KBP)) {                  /*  内核断点禁能                */
        if ((ulAddr < (LW_CFG_VMM_VIRTUAL_START)) ||
            (ulAddr > (LW_CFG_VMM_VIRTUAL_START + LW_CFG_VMM_VIRTUAL_SIZE))) {
            _ErrorHandle(ERROR_KERNEL_MEMORY);
            return  (ERROR_KERNEL_MEMORY);
        }
    }
    
    archDbgBpInsert(ulAddr, pulIns);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceBreakpointRemove
** 功能描述: 删除一个断点
** 输　入  : pvDtrace      dtrace 节点
**           ulAddr        断点地址
**           ulIns         断电之前的指令
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceBreakpointRemove (PVOID  pvDtrace, addr_t  ulAddr, ULONG  ulIns)
{
    archDbgBpRemove(ulAddr, ulIns);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceWatchpointInsert
** 功能描述: 创建一个数据观察点
** 输　入  : pvDtrace      dtrace 节点
**           ulAddr        数据地址
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceWatchpointInsert (PVOID  pvDtrace, addr_t  ulAddr)
{
    _ErrorHandle(ENOSYS);
    return  (ENOSYS);
}
/*********************************************************************************************************
** 函数名称: API_DtraceWatchpointRemove
** 功能描述: 删除一个数据观察点
** 输　入  : pvDtrace      dtrace 节点
**           ulAddr        数据地址
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceWatchpointRemove (PVOID  pvDtrace, addr_t  ulAddr)
{
    _ErrorHandle(ENOSYS);
    return  (ENOSYS);
}
/*********************************************************************************************************
** 函数名称: API_DtraceStopThread
** 功能描述: 停止一个线程
** 输　入  : pvDtrace      dtrace 节点
**           ulThread      线程句柄
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceStopThread (PVOID  pvDtrace, LW_OBJECT_HANDLE  ulThread)
{
    return  (API_ThreadStop(ulThread));
}
/*********************************************************************************************************
** 函数名称: API_DtraceContinueThread
** 功能描述: 恢复一个已经被停止的线程
** 输　入  : pvDtrace      dtrace 节点
**           ulThread      线程句柄
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceContinueThread (PVOID  pvDtrace, LW_OBJECT_HANDLE  ulThread)
{
    return  (API_ThreadContinue(ulThread));
}
/*********************************************************************************************************
** 函数名称: API_DtraceStopProcess
** 功能描述: 停止 dtrace 对应的调试进程
** 输　入  : pvDtrace      dtrace 节点
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceStopProcess (PVOID  pvDtrace)
{
    PLW_DTRACE    pdtrace = (PLW_DTRACE)pvDtrace;
    LW_LD_VPROC  *pvproc  = vprocGet(pdtrace->DTRACE_pid);
    
    if (pvproc) {
        vprocThreadTraversal(pvproc, (VOIDFUNCPTR)API_ThreadStop, 0, 0, 0, 0, 0, 0);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceContinueProcess
** 功能描述: 恢复 dtrace 对应的调试进程
** 输　入  : pvDtrace      dtrace 节点
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceContinueProcess (PVOID  pvDtrace)
{
    PLW_DTRACE    pdtrace = (PLW_DTRACE)pvDtrace;
    LW_LD_VPROC  *pvproc  = vprocGet(pdtrace->DTRACE_pid);
    
    if (pvproc) {
        vprocThreadTraversal(pvproc, (VOIDFUNCPTR)API_ThreadContinue, 0, 0, 0, 0, 0, 0);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceGetBreakInfo
** 功能描述: 获得当前断点线程信息
** 输　入  : pvDtrace      dtrace 节点
**           pdtm          获取的信息
**           ulThread      指定的线程句柄
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceGetBreakInfo (PVOID  pvDtrace, PLW_DTRACE_MSG  pdtm, LW_OBJECT_HANDLE  ulThread)
{
    INT         iError;
    PLW_DTRACE  pdtrace = (PLW_DTRACE)pvDtrace;
    
    if (!pdtrace || !pdtm) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    __KERNEL_ENTER();                                                   /*  进入内核                    */
    if (ulThread == LW_OBJECT_HANDLE_INVALID) {
        iError = __dtraceReadMsg(pdtrace, pdtm);
    } else {
        iError = __dtraceReadMsgEx(pdtrace, pdtm, ulThread);
    }
    __KERNEL_EXIT();                                                    /*  退出内核                    */
    
    if (iError) {
        _ErrorHandle(ENOMSG);
        return  (ENOMSG);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceProcessThread
** 功能描述: 获得进程内所有线程的句柄
** 输　入  : pvDtrace      dtrace 节点
**           ulThread      线程句柄列表缓冲
**           uiTableNum    列表大小
**           puiThreadNum  实际获取的线程数目
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceProcessThread (PVOID  pvDtrace, LW_OBJECT_HANDLE ulThread[], 
                                UINT   uiTableNum, UINT *puiThreadNum)
{
    PLW_DTRACE    pdtrace = (PLW_DTRACE)pvDtrace;
    LW_LD_VPROC  *pvproc  = vprocGet(pdtrace->DTRACE_pid);
    UINT          uiNum   = 0;
    
    if (!ulThread || !uiTableNum || !puiThreadNum) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    if (pvproc) {
        vprocThreadTraversal(pvproc, __dtraceThreadCb, 
                             (PVOID)ulThread, (PVOID)uiTableNum, (PVOID)&uiNum, 
                             0, 0, 0);
    }
    
    *puiThreadNum = uiNum;
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_DtraceThreadExtraInfo
** 功能描述: 获得线程额外信息
** 输　入  : pvDtrace      dtrace 节点
**           ulThread      线程句柄
**           pcExtraInfo   额外信息缓存
**           siSize        缓存大小
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DtraceThreadExtraInfo (PVOID  pvDtrace, LW_OBJECT_HANDLE  ulThread,
                                  PCHAR  pcExtraInfo, size_t  stSize)
{
    ULONG              ulError;
    LW_CLASS_TCB_DESC  tcbdesc;
    
    PCHAR              pcPendType = LW_NULL;
    PCHAR              pcFpu      = LW_NULL;
    size_t             stFreeByteSize = 0;
    
    if (!pcExtraInfo || !stSize) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    ulError = API_ThreadDesc(ulThread, &tcbdesc);
    if (ulError) {
        return  (ulError);
    }
    
    API_ThreadStackCheck(ulThread, &stFreeByteSize, LW_NULL, LW_NULL);
    
    if (tcbdesc.TCBD_usStatus & LW_THREAD_STATUS_SEM) {                 /*  等待信号量                  */
        pcPendType = "SEM";
    
    } else if (tcbdesc.TCBD_usStatus & LW_THREAD_STATUS_MSGQUEUE) {     /*  等待消息队列                */
        pcPendType = "MSGQ";
    
    } else if (tcbdesc.TCBD_usStatus & LW_THREAD_STATUS_SUSPEND) {      /*  挂起                        */
        pcPendType = "SUSP";
    
    } else if (tcbdesc.TCBD_usStatus & LW_THREAD_STATUS_EVENTSET) {     /*  等待事件组                  */
        pcPendType = "ENTS";
    
    } else if (tcbdesc.TCBD_usStatus & LW_THREAD_STATUS_SIGNAL) {       /*  等待信号                    */
        pcPendType = "WSIG";
    
    } else if (tcbdesc.TCBD_usStatus & LW_THREAD_STATUS_INIT) {         /*  初始化中                    */
        pcPendType = "INIT";
    
    } else if (tcbdesc.TCBD_usStatus & LW_THREAD_STATUS_WDEATH) {       /*  僵死状态                    */
        pcPendType = "WDEA";
    
    } else if (tcbdesc.TCBD_usStatus & LW_THREAD_STATUS_DELAY) {        /*  睡眠                        */
        pcPendType = "SLP";
    
    } else {
        pcPendType = "RDY";                                             /*  就绪态                      */
    }
    
    if (tcbdesc.TCBD_ulOption & LW_OPTION_THREAD_USED_FP) {
        pcFpu = "USE";
    } else {
        pcFpu = "NO";
    }
    
    snprintf(pcExtraInfo, stSize, "%s,prio:%d,stat:%s,errno:%ld,wake:%ld,fpu:%s,cpu:%ld,stackfree:%zd",
             tcbdesc.TCBD_cThreadName,
             tcbdesc.TCBD_ucPriority,
             pcPendType,
             tcbdesc.TCBD_ulLastError,
             tcbdesc.TCBD_ulWakeupLeft,
             pcFpu,
             tcbdesc.TCBD_ulCPUId,
             stFreeByteSize);
    
    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_GDB_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
