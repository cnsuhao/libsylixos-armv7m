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
** 文   件   名: KernelTicks.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 18 日
**
** 描        述: 这是系统内核实时时钟函数。

** 注意：
            如果是线程中调用，线程必须是最高优先级的
          
** BUG
2007.04.10  动态 HOOK 函数加入到 LW_CFG_TIME_TICK_HOOK_EN 判断中
2007.05.07  在扫描 tcb 管理链表时，关闭调度器，这样可以防止在任务情况下，调用 kernelTick 时，链表不被破坏
2007.05.24  在线程模式调用时，无法获取时钟中断发生时，运行的线程，从而无法计算时间片消耗情况。
2007.05.27  这里不需要对时间片的减法操作负责，全部由 API_KernelTicksContext() 完成。
2007.05.27  操作系统是否启动的判断放在最开始。
2007.05.27  CPU利用率刷新也不再这里进行，全部由 API_KernelTicksContext() 完成。
2007.05.27  在这里不用保存当前被中断的线程的线程控制块，直接将当前线程时间片减一就行了
            CPU利用率处理在这里进行
2008.03.29  使用全新的 wake up 与 watch dog 机制.
2009.04.14  为了支持 SMP 多核系统, 使用 _SchedSliceTick 进行时间片处理.
2009.09.18  加入新的低中断屏蔽 CPU 利用率测量方法.
2009.10.12  在时钟中断时, 需要遍历所有的 SMP CPU, 处理所有正在运行的线程.
2010.01.04  使用新的 TOD 时钟机制.
2012.07.07  合并 API_KernelTicksContext 到这里.
2013.08.28  加入内核事件监控器.
2014.01.01  API_KernelTicksContext() 需要锁定调度器.
2014.07.04  系统内核支持 tod 时钟微调.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  虚拟进程时间片服务
*********************************************************************************************************/
#if LW_CFG_MODULELOADER_EN > 0
#include "../SylixOS/loader/include/loader_vppatch.h"
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
/*********************************************************************************************************
** 函数名称: __kernelTODTicks
** 功能描述: 通知一个时钟到达, 更新 TOD 时间
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
** 注  意  : _K_i64TODDelta 为 adjtime 所设置, 用来修正系统 tod 时钟.

                                           API 函数
*********************************************************************************************************/
#if LW_CFG_RTC_EN > 0

VOID  __kernelTODTicks (VOID)
{
    static LONG lNsecStd = (1000 * 1000 * 1000) / LW_CFG_TICKS_PER_SEC; /*  一个 tick 有多少 nsec       */
           LONG lNsec;
           
    INTREG      iregInterLevel;
    
    LW_SPIN_LOCK_QUICK(&_K_slKernelRtc, &iregInterLevel);
    if (_K_iTODDelta > 0) {                                             /*  需要加快系统时钟一个 TICK   */
        _K_iTODDelta--;
        lNsec = lNsecStd << 1;                                          /*  加快一个 tick               */
    } else if (_K_iTODDelta < 0) {
        _K_iTODDelta++;
        goto    __tod_tick_over;                                        /*  系统 tod 时间停止一个 tick  */
    } else {
        lNsec = lNsecStd;
    }
    
    _K_tvTODCurrent.tv_nsec += lNsec;                                   /*  CLOCK_REALTIME              */
    if (_K_tvTODCurrent.tv_nsec >= __TIMEVAL_NSEC_MAX) {                /*  是否产生秒进位              */
        _K_tvTODCurrent.tv_nsec -= __TIMEVAL_NSEC_MAX;
        _K_tvTODCurrent.tv_sec  += 1;                                   /*  产生秒进位                  */
    }

    _K_tvTODMono.tv_nsec += lNsec;                                      /*  CLOCK_MONOTONIC             */
    if (_K_tvTODMono.tv_nsec >= __TIMEVAL_NSEC_MAX) {                   /*  是否产生秒进位              */
        _K_tvTODMono.tv_nsec -= __TIMEVAL_NSEC_MAX;
        _K_tvTODMono.tv_sec  += 1;                                      /*  产生秒进位                  */
    }
    
__tod_tick_over:
    LW_SPIN_UNLOCK_QUICK(&_K_slKernelRtc, iregInterLevel);
}

#endif                                                                  /*  LW_CFG_RTC_EN > 0           */
/*********************************************************************************************************
** 函数名称: API_KernelTicks
** 功能描述: 通知系统到达一个实时时钟 TICK 可以在中断中调用，也可以在线程中调用
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API
VOID  API_KernelTicks (VOID)
{
             INTREG  iregInterLevel;
    REGISTER INT64   i64Tick;

    if (!LW_SYS_STATUS_IS_RUNNING()) {                                  /*  系统没有启动                */
        return;
    }

#if LW_CFG_RTC_EN > 0
    __kernelTODTicks();                                                 /*  更新 TOD 时间               */
#endif

    LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);                  /*  关闭中断同时锁住 spinlock   */
    _K_i64KernelTime++;                                                 /*  tick++                      */
    i64Tick = _K_i64KernelTime;
    LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);                 /*  打开中断, 同时打开 spinlock */
    
#if LW_CFG_TIME_TICK_HOOK_EN > 0
    bspTickHook(i64Tick);                                               /*  调用系统时钟钩子函数        */
    __LW_THREAD_TICK_HOOK(i64Tick);
#endif

#if LW_CFG_SOFTWARE_WATCHDOG_EN > 0
    _WatchDogTick();                                                    /*  处理看门狗链表              */
#endif                                                                  /*  LW_CFG_SOFTWARE_WATCHDOG... */
    _ThreadTick();                                                      /*  处理等待链表                */
    
    MONITOR_EVT(MONITOR_EVENT_ID_KERNEL, MONITOR_EVENT_KERNEL_TICK, LW_NULL);
}
/*********************************************************************************************************
** 函数名称: API_KernelTicksContext
** 功能描述: 处理系统时钟中断, 此函数必须在中断中被调用! 否则 _SchedSliceTick 将不能准确的计算时间片.
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API
VOID  API_KernelTicksContext (VOID)
{
             INTREG         iregInterLevel;
    REGISTER INT            i;
             PLW_CLASS_CPU  pcpu;
             PLW_CLASS_TCB  ptcb;
    
    LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);                  /*  关闭中断同时锁住 spinlock   */
    
#if LW_CFG_SMP_EN > 0
    for (i = 0; i < LW_NCPUS; i++) {                                    /*  遍历所有的核                */
#else
    i = 0;
#endif                                                                  /*  LW_CFG_SMP_EN               */

        pcpu = LW_CPU_GET(i);
        if (LW_CPU_IS_ACTIVE(pcpu)) {                                   /*  CPU 必须被激活              */
            ptcb = pcpu->CPU_ptcbTCBCur;
            ptcb->TCB_ulCPUTicks++;
            if (pcpu->CPU_iKernelCounter) {
                ptcb->TCB_ulCPUKernelTicks++;
            }
            __LW_TICK_CPUUSAGE_UPDATE(ptcb, pcpu);                      /*  更新所有 CPU 利用率         */
#if LW_CFG_MODULELOADER_EN > 0
            vprocTickHook(ptcb, pcpu);                                  /*  测算进程执行时间            */
#endif
        }
        
#if LW_CFG_SMP_EN > 0
    }
#endif                                                                  /*  LW_CFG_SMP_EN               */
    
    LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);                 /*  打开中断, 同时打开 spinlock */

    _SchedTick();                                                       /*  处理所有 CPU 线程的时间片   */
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
