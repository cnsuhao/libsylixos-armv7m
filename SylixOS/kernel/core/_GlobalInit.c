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
** 文   件   名: _GlobalInit.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 13 日
**
** 描        述: 这是系统初始化函数库。

** BUG
2007.03.22  加入系统没有启动时的错误处理机制
2007.04.12  加入中断堆栈区基地址初始化
2007.04.12  清空中断堆栈
2007.06.06  最高中断嵌套层数清零
2008.01.20  取消对线程调度器锁的全局变量初始化.
2008.03.29  初始化分化出来的等待链表和看门狗链表的链表头.
2009.04.29  加入对 SMP 相关内核锁的初始化.
2009.10.12  加入对 CPU ID 的初始化.
2009.11.01  修正注释.
            10.31日, 我国伟大的科学家钱学森逝世, 享年98岁. 作者对钱老无比敬佩! 
            借用<士兵突击>中高诚的一句话追思钱老, "信念这东西, 还真不是说出来的, 是做出来的!". 
            也借此勉励自己.
2010.01.22  加入内核线程扫描链尾的初始化.
2010.08.03  加入 tick spinlock 初始化.
2012.07.04  合并 hook 初始化.
2012.09.11  _GlobalInit() 中加入对 FPU 的初始化.
2013.12.19  去掉 FPU 的初始化, 放在 bsp 内核初始化回调中进行, 用户需要指定 fpu 的名称.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#define  __KERNEL_MAIN_FILE                                             /*  这是系统主文件              */
#define  __LW_BITMAP                                                    /*  这是系统主文件              */
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  中断堆栈定义
*********************************************************************************************************/
LW_STACK    _K_stkInterruptStack[LW_CFG_MAX_PROCESSORS][LW_CFG_INT_STK_SIZE / sizeof(LW_STACK)];
PLW_STACK   _K_pstkInterruptBase[LW_CFG_MAX_PROCESSORS];                /*  中断处理时的堆栈基地址      */
                                                                        /*  通过 CPU_STK_GROWTH 判断    */
/*********************************************************************************************************
** 函数名称: __interStackInit
** 功能描述: 初始化中断堆栈, (SylixOS 在 SMP 中每一个 CPU 都可以接受中断)
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __interStackInit (VOID)
{
    REGISTER INT        i;
    
    for (i = 0; i < LW_CFG_MAX_PROCESSORS; i++) {
#if CPU_STK_GROWTH == 0
        _K_pstkInterruptBase[i] = &_K_stkInterruptStack[i][0];
#else
        _K_pstkInterruptBase[i] = &_K_stkInterruptStack[i][(LW_CFG_INT_STK_SIZE / sizeof(LW_STACK)) - 1];
#endif                                                                  /*  CPU_STK_GROWTH              */
        lib_memset(_K_stkInterruptStack[i], LW_CFG_STK_EMPTY_FLAG, LW_CFG_INT_STK_SIZE);
    }
}
/*********************************************************************************************************
** 函数名称: __cpuInit
** 功能描述: 操作系统 CPU 控制块结构初始化
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __cpuInit (VOID)
{
    REGISTER INT        i;
    
    for (i = 0; i < LW_CFG_MAX_PROCESSORS; i++) {
        LW_CPU_GET(i)->CPU_ulStatus = LW_CPU_STATUS_INACTIVE;           /*  CPU INACTIVE                */
        LW_SPIN_INIT(&_K_tcbDummy[i].TCB_slLock);                       /*  初始化自旋锁                */
    }
}
/*********************************************************************************************************
** 函数名称: __miscSmpInit
** 功能描述: 与 SMP 有关的全局变量初始化
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __miscSmpInit (VOID)
{
    REGISTER INT            i;
             PLW_CLASS_CPU  pcpu;
    
    for (i = 0; i < LW_CFG_MAX_PROCESSORS; i++) {
        pcpu = LW_CPU_GET(i);
        
        LW_CAND_TCB(pcpu) = LW_NULL;                                    /*  候选运行表为空              */
        LW_CAND_ROT(pcpu) = LW_FALSE;                                   /*  没有优先级卷绕              */
        
        pcpu->CPU_ulCPUId        = (ULONG)i;
        pcpu->CPU_iKernelCounter = 1;                                   /*  初始化 1, 当前不允许调度    */
        pcpu->CPU_ulIPIVector    = __ARCH_ULONG_MAX;                    /*  目前不确定核间中断向量      */
        LW_SPIN_INIT(&pcpu->CPU_slIpi);                                 /*  初始化 CPU spinlock         */
    }
}
/*********************************************************************************************************
** 函数名称: __hookInit
** 功能描述: hook 初始化
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块:
*********************************************************************************************************/
static VOID  __hookInit (VOID)
{
    _K_hookKernel.HOOK_ThreadCreate    = LW_NULL;                       /*  线程建立钩子                */
    _K_hookKernel.HOOK_ThreadDelete    = LW_NULL;                       /*  线程删除钩子                */
    _K_hookKernel.HOOK_ThreadSwap      = LW_NULL;                       /*  线程切换钩子                */
    _K_hookKernel.HOOK_ThreadTick      = LW_NULL;                       /*  系统时钟中断钩子            */
    _K_hookKernel.HOOK_ThreadInit      = LW_NULL;                       /*  线程初始化钩子              */
    _K_hookKernel.HOOK_ThreadIdle      = LW_NULL;                       /*  空闲线程钩子                */
    _K_hookKernel.HOOK_KernelInitBegin = LW_NULL;                       /*  内核初始化开始钩子          */
    _K_hookKernel.HOOK_KernelInitEnd   = LW_NULL;                       /*  内核初始化结束钩子          */
    _K_hookKernel.HOOK_KernelReboot    = LW_NULL;                       /*  内核重新启动钩子            */
    _K_hookKernel.HOOK_WatchDogTimer   = LW_NULL;                       /*  看门狗定时器钩子            */

    _K_hookKernel.HOOK_ObjectCreate = LW_NULL;                          /*  创建内核对象钩子            */
    _K_hookKernel.HOOK_ObjectDelete = LW_NULL;                          /*  删除内核对象钩子            */
    _K_hookKernel.HOOK_FdCreate     = LW_NULL;                          /*  文件描述符创建钩子          */
    _K_hookKernel.HOOK_FdDelete     = LW_NULL;                          /*  文件描述符删除钩子          */
    
    _K_hookKernel.HOOK_CpuIdleEnter = LW_NULL;                          /*  CPU 进入空闲模式            */
    _K_hookKernel.HOOK_CpuIdleExit  = LW_NULL;                          /*  CPU 退出空闲模式            */
    _K_hookKernel.HOOK_CpuIntEnter  = LW_NULL;                          /*  CPU 进入中断(异常)模式      */
    _K_hookKernel.HOOK_CpuIntExit   = LW_NULL;                          /*  CPU 退出中断(异常)模式      */
}
/*********************************************************************************************************
** 函数名称: _GlobalInit
** 功能描述: 初始化零散全局变量
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID _GlobalInit (VOID)
{
    LW_SYS_STATUS_SET(LW_SYS_STATUS_INIT);                              /*  系统状态为初始化状态        */

    /*
     *  内核关键性自旋锁初始化
     */
    LW_SPIN_INIT(&_K_slKernel);                                         /*  初始化内核自旋锁            */
    LW_SPIN_INIT(&_K_slAtomic);                                         /*  初始化原子操作自旋锁        */

    /*
     *  内核关键性数据结构初始化
     */
    __cpuInit();                                                        /*  CPU 结构初始化              */
    __interStackInit();                                                 /*  首先初始化中断堆栈          */
    __miscSmpInit();                                                    /*  SMP 相关初始化              */
    __hookInit();                                                       /*  hook 初始化                 */
    
    /*
     *  内核关键性状态变量初始化
     */
    _K_i64KernelTime = 0;
    
#if LW_CFG_THREAD_CPU_USAGE_CHK_EN > 0
    _K_ulCPUUsageTicks       = 1ul;                                     /*  避免除 0 错误               */
    _K_ulCPUUsageKernelTicks = 0ul;
#endif

    _K_usThreadCounter = 0;                                             /*  线程数量                    */
    _K_plineTCBHeader  = LW_NULL;                                       /*  TCB 管理链表头              */
    _K_ulNotRunError   = 0ul;                                           /*  系统未启动时错误存放变量    */
    
    __WAKEUP_INIT(&_K_wuDelay);
#if LW_CFG_SOFTWARE_WATCHDOG_EN > 0
    __WAKEUP_INIT(&_K_wuWatchDog);
#endif                                                                  /*  LW_CFG_SOFTWARE_WATCHDOG_EN */
    
#if LW_CFG_THREAD_CPU_USAGE_CHK_EN > 0
    __LW_TICK_CPUUSAGE_ENABLE();                                        /*  启动 CPU 利用率测试         */
#endif                                                                  /*  LW_CFG_THREAD_CPU_USAGE_... */
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
