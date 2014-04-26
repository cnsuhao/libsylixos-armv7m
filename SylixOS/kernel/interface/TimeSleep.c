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
** 文   件   名: TimeSleep.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 14 日
**
** 描        述: 线程睡眠函数。

** BUG
2008.03.29  使用新的等待机制.
2008.03.30  使用新的就绪环操作.
2008.04.12  加入对信号的支持.
2008.04.13  由于很多内核机制需要精确的延迟, 所以这里区别 API_TimeSleep 和 posix sleep.
            API_TimeSleep 是不能被信号唤醒的, 而 posix sleep 是可以被信号唤醒的.
2009.10.12  加入对 SMP 多核的支持.
2012.03.20  减少对 _K_ptcbTCBCur 的引用, 尽量采用局部变量, 减少对当前 CPU ID 获取的次数.
2013.07.10  去掉 API_TimeUSleep API.
            API_TimeMSleep 至少保证一个 tick 延迟.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: API_TimeSleep
** 功能描述: 线程睡眠函数 (此 API 不能被信号唤醒)
** 输　入  : ulTick            睡眠的时间
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API
VOID  API_TimeSleep (ULONG    ulTick)
{
             INTREG                iregInterLevel;
             
             PLW_CLASS_TCB         ptcbCur;
	REGISTER PLW_CLASS_PCB         ppcb;
	REGISTER ULONG                 ulKernelTime;
    
    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return;
    }
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);                                       /*  当前任务控制块              */
    
    MONITOR_EVT_LONG2(MONITOR_EVENT_ID_THREAD, MONITOR_EVENT_THREAD_SLEEP, 
                      ptcbCur->TCB_ulId, ulTick, LW_NULL);
    
__wait_again:
    if (!ulTick) {                                                      /*  不进行延迟                  */
        _ErrorHandle(ERROR_NONE);
        return;
    }

    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */

    ppcb = _GetPcb(ptcbCur);
    __DEL_FROM_READY_RING(ptcbCur, ppcb);                               /*  从就绪表中删除              */
    
    ptcbCur->TCB_ulDelay = ulTick;
    __ADD_TO_WAKEUP_LINE(ptcbCur);                                      /*  加入等待扫描链              */
    
    __KERNEL_TIME_GET(ulKernelTime, ULONG);                             /*  记录系统时间                */
    
    if (__KERNEL_EXIT_IRQ(iregInterLevel)) {                            /*  被信号激活                  */
        ulTick = _sigTimeOutRecalc(ulKernelTime, ulTick);               /*  重新计算等待时间            */
        goto __wait_again;                                              /*  继续等待                    */
    }
    
    _ErrorHandle(ERROR_NONE);
    return;
}
/*********************************************************************************************************
** 函数名称: API_TimeSSleep
** 功能描述: 线程睡眠函数
** 输　入  : ulSeconds            睡眠的时间 (秒)
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
VOID    API_TimeSSleep (ULONG   ulSeconds)
{
    API_TimeSleep(ulSeconds * LW_CFG_TICKS_PER_SEC);
}
/*********************************************************************************************************
** 函数名称: API_TimeMSleep
** 功能描述: 线程睡眠函数
** 输　入  : ulMSeconds            睡眠的时间 (毫秒)
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
VOID    API_TimeMSleep (ULONG   ulMSeconds)
{
    REGISTER ULONG      ulTicks;
    
    if (ulMSeconds == 0) {
        return;
    }
    
    ulTicks = LW_MSECOND_TO_TICK_1(ulMSeconds);
    
    API_TimeSleep(ulTicks);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
