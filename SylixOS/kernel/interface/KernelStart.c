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
** 文   件   名: KernelStart.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 13 日
**
** 描        述: 这是系统内核入口函数。

** BUG
2007.03.22  去掉了原始的系统未启动时的错误截获机制
2007.07.11  _KernelEntry() 中将 OSInitTickHook() 放在 _SchedSeekPriority() 之前，以保证优先级计算正确
2007.07.13  加入 _DebugHandle() 信息功能。
2008.07.27  加入动态堆地址确定功能.
2008.12.03  将中断初始化提前.
2009.04.17  将 OSInitTickHook() 放在最后.
2011.03.08  在调用 pfuncStartHook 之前, 初始化 c++ 运行时库.
2012.03.19  _KernelEntry() 启动首次调度钱, 初始化所有处理器调动参数.
2012.03.24  加入启动参数分析.
2012.04.06  加入 _KernelSMPStartup() 函数来启动所有 CPU 开始运转.
2012.09.11  加入 kfpu 启动参数
2012.09.12  OSStartHighRdy() 启动多任务前, 需要初始化被运行任务的 FPU 环境.
2013.05.01  内核启动参数加入堆内存越界的相关检查.
2013.07.17  引入新的多核初始化机制.
2014.08.08  将内核启动参数调整放在独立的文件中.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  SMP 同步变量
*********************************************************************************************************/
#if LW_CFG_SMP_EN > 0
static volatile BOOL            _K_bMultiTaskStart = LW_FALSE;          /*  系统进入多任务模式          */
static volatile ULONG           _K_ulHoldingPen    = 0ul;               /*  主核 starthook 完毕         */
#define LW_HOLDING_PEN_START    0xdeadbeef                              /*  启动从核的模数              */
#endif                                                                  /*  LW_CFG_SMP_EN               */
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
extern INT  _cppRtInit(VOID);
/*********************************************************************************************************
** 函数名称: _KernelPrimaryCoreStartup
** 功能描述: 系统的主核 (负责初始化的核) 进入多任务状态, 进入多任务状态后, 所有核均对等不分主从
** 输　入  : pcpuCur   当前 CPU
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static  VOID  _KernelPrimaryCoreStartup (PLW_CLASS_CPU   pcpuCur)
{
    PLW_CLASS_TCB   ptcbOrg;
    
    LW_SPIN_LOCK_IGNIRQ(&_K_slKernel);
    LW_TCB_GET_CUR(ptcbOrg);
    _CpuActive(pcpuCur);                                                /*  CPU 激活                    */
    LW_SPIN_UNLOCK_SCHED(&_K_slKernel, ptcbOrg);
    
#if LW_CFG_SMP_EN > 0
    KN_SMP_MB();                                                        /*  内存屏障, 确保之前操作已处理*/
    _K_bMultiTaskStart = LW_TRUE;                                       /*  通知从核可以进入多任务模式  */
#endif                                                                  /*  LW_CFG_SMP_EN               */

    pcpuCur->CPU_iKernelCounter = 0;                                    /*  允许调度                    */
    KN_SMP_MB();

    _DebugHandle(__LOGMESSAGE_LEVEL, "primary cpu multi-task start...\r\n");
    
#if LW_CFG_CPU_FPU_EN > 0
    _ThreadFpuSwith(LW_FALSE);                                          /*  初始化将要运行任务 FPU 环境 */
#endif

    errno = ERROR_NONE;                                                 /*  清除错误                    */
    archTaskCtxStart(pcpuCur);                                          /*  当前 CPU 进入任务状态       */
}
/*********************************************************************************************************
** 函数名称: _KernelSecondaryCoreStartup
** 功能描述: 系统的从核 (非初始化核) 进入多任务状态, 进入多任务状态后, 所有核均对等不分主从
** 输　入  : pcpuCur   当前 CPU
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_SMP_EN > 0

static  VOID  _KernelSecondaryCoreStartup (PLW_CLASS_CPU   pcpuCur)
{
    ULONG           i;
    PLW_CLASS_TCB   ptcbOrg;
    
    while (_K_bMultiTaskStart == LW_FALSE) {
        LW_SPINLOCK_DELAY();                                            /*  短延迟并释放总线            */
    }

    LW_SPIN_LOCK_IGNIRQ(&_K_slKernel);
    LW_TCB_GET_CUR(ptcbOrg);
    _CpuActive(pcpuCur);                                                /*  CPU 激活                    */
    LW_SPIN_UNLOCK_SCHED(&_K_slKernel, ptcbOrg);
    
    pcpuCur->CPU_iKernelCounter = 0;                                    /*  允许调度                    */
    KN_SMP_MB();
    
    for (i = 0; i < LW_NCPUS; i++) {
        if (!LW_CPU_IS_ACTIVE(LW_CPU_GET(i))) {
            break;
        }
    }
    
    if (i >= LW_NCPUS) {                                                /*  所有的 CPU 都已经运行       */
        _K_ulHoldingPen = 0;                                            /*  防止系统重启时判断出错      */
    }
    KN_SMP_MB();
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "secondary cpu multi-task start...\r\n");
    
#if LW_CFG_CPU_FPU_EN > 0
    _ThreadFpuSwith(LW_FALSE);                                          /*  初始化将要运行任务 FPU 环境 */
#endif

    errno = ERROR_NONE;                                                 /*  清除错误                    */
    archTaskCtxStart(pcpuCur);                                          /*  当前 CPU 进入任务状态       */
}

#endif                                                                  /*  LW_CFG_SMP_EN               */
/*********************************************************************************************************
** 函数名称: _KernelPrimaryEntry
** 功能描述: 主核系统内核内部入口
** 输　入  : pcpuCur   当前 CPU
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static  VOID  _KernelPrimaryEntry (PLW_CLASS_CPU   pcpuCur)
{
    if (LW_SYS_STATUS_IS_RUNNING()) {                                   /*  系统必须没有启动            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "kernel is already run! can not re-enter\r\n");
        _ErrorHandle(ERROR_KERNEL_RUNNING);
        return;
    }
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "kernel ticks initialize...\r\n");
    bspTickInit();                                                      /*  初始化操作系统时钟          */

    LW_SYS_STATUS_SET(LW_SYS_STATUS_RUNNING);                           /*  启动多任务处理              */
    
    _KernelPrimaryCoreStartup(pcpuCur);                                 /*  主核(当前核)运行            */
}
/*********************************************************************************************************
** 函数名称: _KernelBootSecondary
** 功能描述: 系统发出核间中断, 从核启动
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_SMP_EN > 0

static VOID  _KernelBootSecondary (VOID)
{
    KN_SMP_MB();                                                        /*  内存屏障, 确保之前操作已处理*/
    _K_ulHoldingPen = LW_HOLDING_PEN_START;                             /*  通知从核可以进行基本初始化  */
}

#endif                                                                  /*  LW_CFG_SMP_EN               */
/*********************************************************************************************************
** 函数名称: API_KernelPrimaryStart
** 功能描述: 系统内核入口 (只允许系统逻辑主核调用, 一般为 CPU 0, 当前为关中断状态)
** 输　入  : pfuncStartHook     系统启动中的用户回调
**           pvKernelHeapMem    内核堆内存首地址
**           stKernelHeapSize   内核堆大小
**           pvSystemHeapMem    系统堆内存首地址
**           stSystemHeapSize   系统堆大小
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                       API 函数
*********************************************************************************************************/
#if LW_CFG_MEMORY_HEAP_CONFIG_TYPE > 0
LW_API
VOID  API_KernelPrimaryStart (PKERNEL_START_ROUTINE  pfuncStartHook,
                              PVOID                  pvKernelHeapMem,
                              size_t                 stKernelHeapSize,
                              PVOID                  pvSystemHeapMem,
                              size_t                 stSystemHeapSize)
#else
LW_API
VOID  API_KernelPrimaryStart (PKERNEL_START_ROUTINE  pfuncStartHook)
#endif                                                                  /*  LW_CFG_MEMORY_HEAP_...      */
{
    INT             iError;
    PLW_CLASS_CPU   pcpuCur;

    if (LW_SYS_STATUS_IS_RUNNING()) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "kernel is already start.\r\n");
        return;
    }
    
    if (LW_NCPUS > LW_CFG_MAX_PROCESSORS) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "LW_NCPUS > LW_CFG_MAX_PROCESSORS.\r\n");
        return;
    }
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "longwing(TM) kernel initialize...\r\n");
    
#if LW_CFG_MEMORY_HEAP_CONFIG_TYPE > 0
    if (!pvKernelHeapMem || !pvSystemHeapMem) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "heap memory invalidate.\r\n");
        _DebugHandle(__LOGMESSAGE_LEVEL,   "some error occur, kernel is not initialize.\r\n");
        return;
    }
    if (!_Addresses_Is_Aligned(pvKernelHeapMem) ||
        !_Addresses_Is_Aligned(pvSystemHeapMem)) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "heap memory is not align.\r\n");
        _DebugHandle(__LOGMESSAGE_LEVEL,   "some error occur, kernel is not initialize.\r\n");
        return;
    }
    if ((stKernelHeapSize < LW_CFG_KB_SIZE) || (stSystemHeapSize < LW_CFG_KB_SIZE)) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "heap memory is too little.\r\n");
        _DebugHandle(__LOGMESSAGE_LEVEL,   "some error occur, kernel is not initialize.\r\n");
        return;
    }
    _KernelLowLevelInit(pvKernelHeapMem, stKernelHeapSize,
                        pvSystemHeapMem, stSystemHeapSize);             /*  内核底层初始化              */
#else
    _KernelLowLevelInit();                                              /*  内核底层初始化              */
#endif                                                                  /*  LW_CFG_MEMORY_HEAP_...      */
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "kernel interrupt vector initialize...\r\n");
    bspIntInit();                                                       /*  初始化中断系统              */
    
    _KernelHighLevelInit();                                             /*  内核高级初始化              */
    
    iError = _cppRtInit();                                              /*  CPP 运行时库初始化          */
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "c++ run time lib initialized error!\r\n");
    } else {
        _DebugHandle(__LOGMESSAGE_LEVEL, "c++ run time lib initialized.\r\n");
    }
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "kernel primary cpu usrStartup...\r\n");
    if (pfuncStartHook) {                                               /*  用户是否要求需要初始化      */
        pfuncStartHook();                                               /*  用户系统初始化              */
    }
    
#if LW_CFG_MODULELOADER_EN > 0
    _resInit();                                                         /*  从现在开始记录资源情况      */
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
    
#if LW_CFG_SMP_EN > 0
    _KernelBootSecondary();                                             /*  从核可以进行初始化操作      */
#endif                                                                  /*  LW_CFG_SMP_EN               */

    pcpuCur = LW_CPU_GET_CUR();

    _KernelPrimaryEntry(pcpuCur);                                       /*  启动内核                    */
                                                                        /*  多核将在第一次调度被启用    */
}
/*********************************************************************************************************
** 函数名称: API_KernelSecondaryStart
** 功能描述: 从核系统内核入口 (只允许系统主核调用, 也就是非启动 CPU, 当前为关中断状态)
** 输　入  : pfuncStartHook            初始化本 CPU 基本资源, 例如 MMU, CACHE, FPU 等
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                       API 函数
*********************************************************************************************************/
#if LW_CFG_SMP_EN > 0

VOID  API_KernelSecondaryStart (PKERNEL_START_ROUTINE  pfuncStartHook)
{
    PLW_CLASS_CPU   pcpuCur;

    while (_K_ulHoldingPen != LW_HOLDING_PEN_START) {
        LW_SPINLOCK_DELAY();                                            /*  短延迟并释放总线            */
    }
    
    pcpuCur = LW_CPU_GET_CUR();
    pcpuCur->CPU_ptcbTCBCur = &_K_tcbDummy[pcpuCur->CPU_ulCPUId];       /*  伪内核线程                  */
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "kernel secondary cpu usrStartup...\r\n");
    if (pfuncStartHook) {                                               /*  用户是否要求需要初始化      */
        pfuncStartHook();                                               /*  用户系统初始化              */
    }

    _KernelSecondaryCoreStartup(pcpuCur);                               /*  主核初始化完毕直接启动多任务*/
}

#endif                                                                  /*  LW_CFG_SMP_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
