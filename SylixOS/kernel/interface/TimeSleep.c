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
  s_internal.h 中也有相关定义
*********************************************************************************************************/
#if LW_CFG_THREAD_CANCEL_EN > 0
#define __THREAD_CANCEL_POINT()         API_ThreadTestCancel()
#else
#define __THREAD_CANCEL_POINT()
#endif                                                                  /*  LW_CFG_THREAD_CANCEL_EN     */
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
        return;
    }

    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */

    ppcb = _GetPcb(ptcbCur);
    __DEL_FROM_READY_RING(ptcbCur, ppcb);                               /*  从就绪表中删除              */
    
    ptcbCur->TCB_ulDelay = ulTick;
    __ADD_TO_WAKEUP_LINE(ptcbCur);                                      /*  加入等待扫描链              */
    
    __KERNEL_TIME_GET_NO_SPINLOCK(ulKernelTime, ULONG);                 /*  记录系统时间                */
    
    if (__KERNEL_EXIT_IRQ(iregInterLevel)) {                            /*  被信号激活                  */
        ulTick = _sigTimeOutRecalc(ulKernelTime, ulTick);               /*  重新计算等待时间            */
        goto __wait_again;                                              /*  继续等待                    */
    }
}
/*********************************************************************************************************
** 函数名称: API_TimeSleepEx
** 功能描述: 线程睡眠函数
** 输　入  : ulTick            睡眠的时间
**           bSigRet           是否允许信号唤醒
** 输　出  : ERROR_NONE or EINTR
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API
ULONG  API_TimeSleepEx (ULONG  ulTick, BOOL  bSigRet)
{
             INTREG                iregInterLevel;
             
             PLW_CLASS_TCB         ptcbCur;
	REGISTER PLW_CLASS_PCB         ppcb;
	REGISTER ULONG                 ulKernelTime;
    
    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (ERROR_KERNEL_IN_ISR);
    }
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);                                       /*  当前任务控制块              */
    
    MONITOR_EVT_LONG2(MONITOR_EVENT_ID_THREAD, MONITOR_EVENT_THREAD_SLEEP, 
                      ptcbCur->TCB_ulId, ulTick, LW_NULL);
    
__wait_again:
    if (!ulTick) {                                                      /*  不进行延迟                  */
        return  (ERROR_NONE);
    }

    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */

    ppcb = _GetPcb(ptcbCur);
    __DEL_FROM_READY_RING(ptcbCur, ppcb);                               /*  从就绪表中删除              */
    
    ptcbCur->TCB_ulDelay = ulTick;
    __ADD_TO_WAKEUP_LINE(ptcbCur);                                      /*  加入等待扫描链              */
    
    __KERNEL_TIME_GET_NO_SPINLOCK(ulKernelTime, ULONG);                 /*  记录系统时间                */
    
    if (__KERNEL_EXIT_IRQ(iregInterLevel)) {                            /*  被信号激活                  */
        if (bSigRet) {
            _ErrorHandle(EINTR);
            return  (EINTR);
        }
        ulTick = _sigTimeOutRecalc(ulKernelTime, ulTick);               /*  重新计算等待时间            */
        goto __wait_again;                                              /*  继续等待                    */
    }
    
    return  (ERROR_NONE);
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
** 函数名称: sleep 
** 功能描述: sleep()会令目前的线程暂停，直到达到参数 uiSeconds 所指定的时间，或是被信号所中断。(POSIX)
** 输　入  : uiSeconds         睡眠的秒数
** 输　出  : 若进程暂停到参数 uiSeconds 所指定的时间则返回 0，若有信号中断则返回剩余秒数。

             error == EINTR    表示被信号激活.
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
UINT  sleep (UINT    uiSeconds)
{
             INTREG                iregInterLevel;
             
             PLW_CLASS_TCB         ptcbCur;
    REGISTER PLW_CLASS_PCB         ppcb;
	REGISTER ULONG                 ulKernelTime;
	         INT                   iSchedRet;
	         
	         ULONG                 ulTick = (ULONG)(uiSeconds * LW_CFG_TICKS_PER_SEC);
    
    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (0);
    }
    
__wait_again:
    if (!ulTick) {                                                      /*  不进行延迟                  */
        return  (0);
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);                                       /*  当前任务控制块              */
    
    MONITOR_EVT_LONG2(MONITOR_EVENT_ID_THREAD, MONITOR_EVENT_THREAD_SLEEP, 
                      ptcbCur->TCB_ulId, ulTick, LW_NULL);
    
    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */
    
    ppcb = _GetPcb(ptcbCur);
    __DEL_FROM_READY_RING(ptcbCur, ppcb);                               /*  从就绪表中删除              */
    
    ptcbCur->TCB_ulDelay = ulTick;
    __ADD_TO_WAKEUP_LINE(ptcbCur);                                      /*  加入等待扫描链              */
    
    __KERNEL_TIME_GET_NO_SPINLOCK(ulKernelTime, ULONG);                 /*  记录系统时间                */
    
    iSchedRet = __KERNEL_EXIT_IRQ(iregInterLevel);                      /*  调度器解锁                  */
    if (iSchedRet == LW_SIGNAL_EINTR) {
        ulTick = _sigTimeOutRecalc(ulKernelTime, ulTick);               /*  重新计算等待时间            */
        uiSeconds = (UINT)(ulTick / LW_CFG_TICKS_PER_SEC);
        
        _ErrorHandle(EINTR);                                            /*  被信号激活                  */
        return  (uiSeconds);                                            /*  剩余秒数                    */
        
    } else if (iSchedRet == LW_SIGNAL_RESTART) {
        ulTick = _sigTimeOutRecalc(ulKernelTime, ulTick);               /*  重新计算等待时间            */
        goto    __wait_again;
    }
    
    return  (0);
}
/*********************************************************************************************************
** 函数名称: nanosleep 
** 功能描述: 使调用此函数的线程睡眠一个指定的时间, 睡眠过程中可能被信号惊醒!!! (POSIX)
** 输　入  : rqtp         睡眠的时间
**           rmtp         保存剩余时间的结构.
** 输　出  : ERROR_NONE  or  PX_ERROR

             error == EINTR    表示被信号激活.
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
INT  nanosleep (const struct timespec  *rqtp, struct timespec  *rmtp)
{
             INTREG                iregInterLevel;
             
             PLW_CLASS_TCB         ptcbCur;
    REGISTER PLW_CLASS_PCB         ppcb;
	REGISTER ULONG                 ulKernelTime;
	REGISTER INT                   iRetVal;
	         INT                   iSchedRet;
	
	REGISTER ULONG                 ulError;
             ULONG                 ulTick;
             
    if (!rqtp) {                                                        /*  指定时间为空                */
        _ErrorHandle(EINVAL);
        return (PX_ERROR);
    }
    if (rqtp->tv_nsec >= __TIMEVAL_NSEC_MAX) {                          /*  时间格式错误                */
        _ErrorHandle(EINVAL);
        return (PX_ERROR);
    }
    
    ulTick = __timespecToTick((struct timespec *)rqtp);
    if (!ulTick) {
        ulTick = 1;                                                     /*  至少延迟一个 tick           */
    }
    
__wait_again:
    if (!ulTick) {                                                      /*  不进行延迟                  */
        return  (ERROR_NONE);
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);                                       /*  当前任务控制块              */
    
    MONITOR_EVT_LONG2(MONITOR_EVENT_ID_THREAD, MONITOR_EVENT_THREAD_SLEEP, 
                      ptcbCur->TCB_ulId, ulTick, LW_NULL);
    
    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */
    
    ppcb = _GetPcb(ptcbCur);
    __DEL_FROM_READY_RING(ptcbCur, ppcb);                               /*  从就绪表中删除              */
    
    ptcbCur->TCB_ulDelay = ulTick;
    __ADD_TO_WAKEUP_LINE(ptcbCur);                                      /*  加入等待扫描链              */
    
    __KERNEL_TIME_GET_NO_SPINLOCK(ulKernelTime, ULONG);                 /*  记录系统时间                */
    
    iSchedRet = __KERNEL_EXIT_IRQ(iregInterLevel);                      /*  调度器解锁                  */
    if (iSchedRet == LW_SIGNAL_EINTR) {
        iRetVal =  PX_ERROR;                                            /*  被信号激活                  */
        ulError =  EINTR;
    
    } else if (iSchedRet == LW_SIGNAL_RESTART) {
        ulTick = _sigTimeOutRecalc(ulKernelTime, ulTick);               /*  重新计算等待时间            */
        goto    __wait_again;
    
    } else {
        iRetVal =  ERROR_NONE;                                          /*  自然唤醒                    */
        ulError =  ERROR_NONE;
    }
    
    if (rmtp) {
        ULONG           ulTimeTemp;
        
        struct timespec tvNow;
        struct timespec tvThen;
        
        __KERNEL_TIME_GET(ulTimeTemp, ULONG);                           /*  记录系统时间                */
        
        __tickToTimespec(ulKernelTime, &tvThen);                        /*  等待前的时间                */
        __tickToTimespec(ulTimeTemp,   &tvNow);                         /*  等待后的时间                */
    
        __timespecSub(&tvNow, &tvThen);                                 /*  计算时间差                  */
        
        if (__timespecLeftTime(&tvNow, (struct timespec *)rqtp)) {      /*  是否存在差别                */
            *rmtp = *rqtp;
            __timespecSub(rmtp, &tvNow);                                /*  计算时间偏差                */
        } else {
            rmtp->tv_sec  = 0;                                          /*  不存在时间差别              */
            rmtp->tv_nsec = 0;
        }
    }
             
    _ErrorHandle(ulError);                                              /*  设置 errno 值               */
    return  (iRetVal);
}
/*********************************************************************************************************
** 函数名称: usleep 
** 功能描述: 使调用此函数的线程睡眠一个指定的时间(us 为单位), 睡眠过程中可能被信号惊醒!!! (POSIX)
** 输　入  : usecondTime       时间 (us)
** 输　出  : ERROR_NONE  or  PX_ERROR
             error == EINTR    表示被信号激活.
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
INT  usleep (usecond_t   usecondTime)
{
    struct timespec ts;

    ts.tv_sec  = (time_t)(usecondTime / 1000000);
    ts.tv_nsec = (LONG)(usecondTime % 1000000) * 1000ul;
    
    return  (nanosleep(&ts, LW_NULL));
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
