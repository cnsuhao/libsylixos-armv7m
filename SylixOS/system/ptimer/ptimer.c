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
** 文   件   名: ptimer.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 04 月 13 日
**
** 描        述: posix 兼容时间库.

** BUG
2008.06.06  API_Nanosleep() errno 设置错误, 当被信号激活时, errno = EINTR, 正常时 errno = 0;
2008.09.04  POSIX 定时器使用应用定时器而非, posix 内核定时器.
2009.07.01  加入 usleep 函数.
2009.10.12  加入对 SMP 多核的支持.
2009.12.31  修正了绝对时间等待的 bug.
2010.01.21  alarm() 不使用 abs time.
2010.08.03  使用新的获取系统时钟方式.
2010.10.06  加入对 cancel type 的操作, 符合 POSIX 标准.
2011.02.26  使用 sigevent 替代 sigpend 操作.
2011.06.14  nanosleep() usleep() 至少延迟 1 个 tick.
2011.08.15  重大决定: 将所有 posix 定义的函数以函数方式(非宏)引出.
2012.03.20  减少对 _K_ptcbTCBCur 的引用, 尽量采用局部变量, 减少对当前 CPU ID 获取的次数.
2013.05.05  sleep 与 nanosleep 判断调度器返回值, 决定是重启调用还是退出.
2013.10.08  加入 setitimer 与 getitimer 函数.
2013.11.20  支持 timerfd 功能.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁减控制
*********************************************************************************************************/
#if LW_CFG_PTIMER_EN > 0
#include "sys/time.h"
/*********************************************************************************************************
  内部函数声明
*********************************************************************************************************/
static VOID  __ptimerCallback(LW_OBJECT_ID  ulTimer);
#if LW_CFG_PTIMER_AUTO_DEL_EN > 0
static VOID  __ptimerThreadDeleteHook(LW_OBJECT_HANDLE  ulId);
#endif                                                                  /*  LW_CFG_PTIMER_AUTO_DEL_EN   */
/*********************************************************************************************************
  内部变量
*********************************************************************************************************/
static LW_OBJECT_HANDLE     _G_ulAlarmTimerHandle = LW_OBJECT_HANDLE_INVALID;
/*********************************************************************************************************
  创建内部定时器
*********************************************************************************************************/
#define __ALARM_TIMER_CREATE() \
        if (_G_ulAlarmTimerHandle == 0) { \
            if (timer_create(CLOCK_REALTIME, LW_NULL, \
                             &_G_ulAlarmTimerHandle) != ERROR_NONE) { \
                return  (PX_ERROR); \
            } \
        }
#endif                                                                  /*  LW_CFG_PTIMER_EN            */
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
        _ErrorHandle(ERROR_NONE);
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
    
    __KERNEL_TIME_GET(ulKernelTime, ULONG);                             /*  记录系统时间                */
    
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
    
    _ErrorHandle(ERROR_NONE);
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
        _ErrorHandle(ERROR_NONE);
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
    
    __KERNEL_TIME_GET(ulKernelTime, ULONG);                             /*  记录系统时间                */
    
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
** 函数名称: __ptimerHookInstall
** 功能描述: 定时器任务删除回调安装
** 输　入  : clockid      时间基准
**           sigeventT    信号时间
**           ptimer       定时器句柄
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_PTIMER_AUTO_DEL_EN > 0

static VOID __ptimerHookInstall (VOID)
{
    API_SystemHookAdd(__ptimerThreadDeleteHook, 
                      LW_OPTION_THREAD_DELETE_HOOK);                    /*  使用删除回调                */
}

#endif                                                                  /*  LW_CFG_PTIMER_AUTO_DEL_EN   */
/*********************************************************************************************************
** 函数名称: timer_create
** 功能描述: 创建一个定时器 (POSIX)
** 输　入  : clockid      时间基准
**           sigeventT    信号时间
**           ptimer       定时器句柄
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
#if LW_CFG_PTIMER_EN > 0

LW_API  
INT  timer_create (clockid_t  clockid, struct sigevent *sigeventT, timer_t *ptimer)
{
    return  (timer_create_internal(clockid, sigeventT, ptimer, LW_OPTION_NONE));
}
/*********************************************************************************************************
** 函数名称: timer_create_internal
** 功能描述: 创建一个定时器 (内部接口)
** 输　入  : clockid      时间基准
**           sigeventT    信号时间
**           ptimer       定时器句柄
**           ulOption     SylixOS 定时器选项
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  timer_create_internal (clockid_t  clockid, struct sigevent *sigeventT, 
                            timer_t *ptimer, ULONG  ulOption)
{
#if LW_CFG_PTIMER_AUTO_DEL_EN > 0
    static BOOL         bIsInstallHook = LW_FALSE;
#endif                                                                  /*  LW_CFG_PTIMER_AUTO_DEL_EN   */

    LW_OBJECT_HANDLE    ulTimer;
    PLW_CLASS_TIMER     ptmr;
    
    if (!ptimer) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    if (sigeventT) {
        if (!__issig(sigeventT->sigev_signo)) {                         /*  检查信号有效性              */
            _ErrorHandle(EINVAL);
            return  (PX_ERROR);
        }
    }
                                                                        /*  开辟信号相关内存            */
    ulTimer = API_TimerCreate("posix_tmr", 
                              LW_OPTION_ITIMER | ulOption, LW_NULL);    /*  创建普通定时器              */
    if (ulTimer == LW_OBJECT_HANDLE_INVALID) {
        return  (PX_ERROR);
    }
    
#if LW_CFG_PTIMER_AUTO_DEL_EN > 0
    API_ThreadOnce(&bIsInstallHook, __ptimerHookInstall);               /*  安装回调                    */
#endif                                                                  /*  LW_CFG_PTIMER_AUTO_DEL_EN   */
    
    __KERNEL_ENTER();                                                   /*  进入内核                    */
    ptmr = &_K_tmrBuffer[_ObjectGetIndex(ulTimer)];
    ptmr->TIMER_ulThreadId = API_ThreadIdSelf();
    ptmr->TIMER_ulTimer    = ulTimer;                                   /*  定义为 posix 定时器         */
    __KERNEL_EXIT();                                                    /*  退出内核                    */
    
    if (sigeventT == LW_NULL) {
        ptmr->TIMER_sigevent.sigev_signo  = SIGALRM;                    /*  默认信号事件                */
        ptmr->TIMER_sigevent.sigev_notify = SIGEV_SIGNAL;
        ptmr->TIMER_sigevent.sigev_value.sival_ptr = (PVOID)&ptmr->TIMER_ulTimer;
    } else {
        ptmr->TIMER_sigevent = *sigeventT;                              /*  记录信息                    */
    }
    
    *ptimer = ptmr->TIMER_ulTimer;                                      /*  记录句柄                    */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: timer_delete
** 功能描述: 删除一个定时器 (POSIX)
** 输　入  : timer        定时器句柄
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
INT  timer_delete (timer_t  timer)
{
             LW_OBJECT_HANDLE   ulTimer = timer;
    REGISTER ULONG              ulError;
    
    if (!timer) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    ulError = API_TimerDelete(&ulTimer);
    if (ulError) {
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: timer_gettime
** 功能描述: 获得一个定时器的时间参数 (POSIX)
** 输　入  : timer        定时器句柄
**           ptvTime      时间
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
INT  timer_gettime (timer_t  timer, struct itimerspec  *ptvTime)
{
             BOOL        bIsRunning;
             ULONG       ulCounter;
             ULONG       ulInterval;
    REGISTER ULONG       ulError;

    if ((!timer) || ptvTime == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    ulError = API_TimerStatus((LW_OBJECT_HANDLE)timer, &bIsRunning,
                              LW_NULL, &ulCounter, &ulInterval);        /*  获得定时器状态              */
    if (ulError) {
        return  (PX_ERROR);
    }
                    
    if (bIsRunning == LW_FALSE) {                                       /*  定时器没有运行              */
        ptvTime->it_interval.tv_sec  = 0;
        ptvTime->it_interval.tv_nsec = 0;
        ptvTime->it_value.tv_sec     = 0;
        ptvTime->it_value.tv_nsec    = 0;
        return  (ERROR_NONE);
    }
    
    __tickToTimespec(ulInterval, &ptvTime->it_interval);                /*  定时器重装时间              */
    __tickToTimespec(ulCounter, &ptvTime->it_value);                    /*  据下一次定时到剩余时间      */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: timer_getoverrun
** 功能描述: 获得一个定时器句柄被搁置的次数 (POSIX)
** 输　入  : timer        定时器句柄
** 输　出  : 柄被搁置的次数
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
INT  timer_getoverrun (timer_t  timer)
{
             INTREG     iregInterLevel;
    REGISTER UINT16     usIndex;
    REGISTER INT        iOverrun;
    LW_OBJECT_HANDLE    ulTimer = timer;
    PLW_CLASS_TIMER     ptmr;
    
    usIndex = _ObjectGetIndex(ulTimer);
    
    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!_ObjectClassOK(ulTimer, _OBJECT_TIMER)) {                      /*  对象类型检查                */
        __KERNEL_EXIT_IRQ(iregInterLevel);                              /*  退出内核                    */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "timer handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (PX_ERROR);
    }
    if (_Timer_Index_Invalid(usIndex)) {                                /*  缓冲区索引检查              */
        __KERNEL_EXIT_IRQ(iregInterLevel);                              /*  退出内核                    */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "timer handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (PX_ERROR);
    }
#endif                                                                  /*  LW_CFG_ARG_CHK_EN > 0       */
    
    ptmr = &_K_tmrBuffer[usIndex];
    if (ptmr->TIMER_u64Overrun > DELAYTIMER_MAX) {
        iOverrun = DELAYTIMER_MAX;
    } else {
        iOverrun = (INT)ptmr->TIMER_u64Overrun;
    }
    
    __KERNEL_EXIT_IRQ(iregInterLevel);                                  /*  退出内核                    */
    _ErrorHandle(ERROR_NONE);
    return  (iOverrun);
}
/*********************************************************************************************************
** 函数名称: timer_getoverrun_64
** 功能描述: 获得一个定时器句柄被搁置的次数
** 输　入  : timer        定时器句柄
**           pu64Overruns overrun 计数
**           bClear       是否清除
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_TIMERFD_EN > 0

INT  timer_getoverrun_64 (timer_t  timer, UINT64  *pu64Overruns, BOOL  bClear)
{
             INTREG     iregInterLevel;
    REGISTER UINT16     usIndex;
    LW_OBJECT_HANDLE    ulTimer = timer;
    PLW_CLASS_TIMER     ptmr;
    
    usIndex = _ObjectGetIndex(ulTimer);
    
    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!_ObjectClassOK(ulTimer, _OBJECT_TIMER)) {                      /*  对象类型检查                */
        __KERNEL_EXIT_IRQ(iregInterLevel);                              /*  退出内核                    */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "timer handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (PX_ERROR);
    }
    if (_Timer_Index_Invalid(usIndex)) {                                /*  缓冲区索引检查              */
        __KERNEL_EXIT_IRQ(iregInterLevel);                              /*  退出内核                    */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "timer handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (PX_ERROR);
    }
#endif                                                                  /*  LW_CFG_ARG_CHK_EN > 0       */
    
    ptmr = &_K_tmrBuffer[usIndex];
    if (pu64Overruns) {
        *pu64Overruns = ptmr->TIMER_u64Overrun;
    }
    if (bClear) {
        ptmr->TIMER_u64Overrun = 0ull;
    }
    
    __KERNEL_EXIT_IRQ(iregInterLevel);                                  /*  退出内核                    */
    _ErrorHandle(ERROR_NONE);
    return  (ERROR_NONE);
}

#endif                                                                  /*  #if LW_CFG_TIMERFD_EN > 0   */
/*********************************************************************************************************
** 函数名称: timer_settime
** 功能描述: 设定一个定时器开始定时 (POSIX)
** 输　入  : timer        定时器句柄
**           iFlag        标志
**           ptvNew       新的时间信息
**           ptvOld       先早的时间信息
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
INT  timer_settime (timer_t                  timer, 
                    INT                      iFlag, 
                    const struct itimerspec *ptvNew,
                    struct itimerspec       *ptvOld)
{
             BOOL        bIsRunning;
             ULONG       ulCounter;
             ULONG       ulInterval;
             
    REGISTER ULONG       ulError;
    REGISTER ULONG       ulOption;
    
    LW_OBJECT_HANDLE     ulTimer = timer;
    
    if (!timer) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    if (ptvOld) {
        ulError = API_TimerStatus((LW_OBJECT_HANDLE)timer, &bIsRunning,
                                  LW_NULL, &ulCounter, &ulInterval);    /*  获得定时器状态              */
        if (ulError) {
            return  (PX_ERROR);
        }
        if (bIsRunning == LW_FALSE) {
            ptvOld->it_interval.tv_sec  = 0;
            ptvOld->it_interval.tv_nsec = 0;
            ptvOld->it_value.tv_sec     = 0;
            ptvOld->it_value.tv_nsec    = 0;
        
        } else {
            __tickToTimespec(ulInterval, &ptvOld->it_interval);         /*  定时器重装时间              */
            __tickToTimespec(ulCounter, &ptvOld->it_value);             /*  据下一次定时到剩余时间      */
        }
    }
    
    if (ptvNew == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    if (ptvNew->it_value.tv_sec  == 0 &&
        ptvNew->it_value.tv_nsec == 0) {                                /*  停止定时器                  */
        API_TimerCancel(ulTimer);
        return  (ERROR_NONE);
    }
    
    if (ptvNew->it_interval.tv_sec  == 0 &&
        ptvNew->it_interval.tv_nsec == 0) {                             /*  ONE SHOT                    */
        ulOption = LW_OPTION_MANUAL_RESTART;
    } else {
        ulOption = LW_OPTION_AUTO_RESTART;                              /*  AUTO RELOAD                 */
    }
    
    if (iFlag == TIMER_ABSTIME) {                                       /*  绝对时钟                    */
        struct timespec  tvNow;
        struct timespec  tvValue = ptvNew->it_value;
        
        lib_clock_gettime(CLOCK_REALTIME, &tvNow);                      /*  使用 realtime               */
        
        if ((ptvNew->it_value.tv_sec < tvNow.tv_sec) ||
            ((ptvNew->it_value.tv_sec == tvNow.tv_sec) &&
             (ptvNew->it_value.tv_nsec < tvNow.tv_nsec))) {
            _ErrorHandle(EINVAL);
            return  (PX_ERROR);
        
        } else {
            __timespecSub(&tvValue, &tvNow);                            /*  计算时间差                  */
        }
        
        ulCounter  = __timespecToTick(&tvValue);
        ulInterval = __timespecToTick(&ptvNew->it_interval);
                  
    } else {                                                            /*  相对时间                    */
        ulCounter  = __timespecToTick(&ptvNew->it_value);
        ulInterval = __timespecToTick(&ptvNew->it_interval);
    }
    
    ulCounter  = (ulCounter  == 0) ? 1 : ulCounter;
    ulInterval = (ulInterval == 0) ? 1 : ulInterval;
    
    ulError = API_TimerStartEx(ulTimer, 
                               ulCounter,
                               ulInterval,
                               ulOption,
                               (PTIMER_CALLBACK_ROUTINE)__ptimerCallback,
                               (PVOID)ulTimer);
    if (ulError) {
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: timer_setfile
** 功能描述: 设定一个定时器对应的 timerfd 文件结构
** 输　入  : timer        定时器句柄
**           pvFile       文件结构
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_TIMERFD_EN > 0

INT  timer_setfile (timer_t  timer, PVOID  pvFile)
{
             INTREG     iregInterLevel;
    REGISTER UINT16     usIndex;
    LW_OBJECT_HANDLE    ulTimer = timer;
    PLW_CLASS_TIMER     ptmr;
    
    usIndex = _ObjectGetIndex(ulTimer);
    
    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!_ObjectClassOK(ulTimer, _OBJECT_TIMER)) {                      /*  对象类型检查                */
        __KERNEL_EXIT_IRQ(iregInterLevel);                              /*  退出内核                    */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "timer handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (PX_ERROR);
    }
    if (_Timer_Index_Invalid(usIndex)) {                                /*  缓冲区索引检查              */
        __KERNEL_EXIT_IRQ(iregInterLevel);                              /*  退出内核                    */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "timer handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (PX_ERROR);
    }
#endif                                                                  /*  LW_CFG_ARG_CHK_EN > 0       */
    
    ptmr = &_K_tmrBuffer[usIndex];
    ptmr->TIMER_pvTimerfd = pvFile;
    
    __KERNEL_EXIT_IRQ(iregInterLevel);                                  /*  退出内核                    */
    _ErrorHandle(ERROR_NONE);
    return  (ERROR_NONE);
}

#endif                                                                  /*  #if LW_CFG_TIMERFD_EN > 0   */
/*********************************************************************************************************
** 函数名称: __ptimerCallback
** 功能描述: 定时器的回调函数
** 输　入  : timer        定时器句柄
**           iFlag        标志
**           ptvNew       新的时间信息
**           ptvOld       先早的时间信息
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __ptimerCallback (LW_OBJECT_ID  ulTimer)
{
#if LW_CFG_TIMERFD_EN > 0
    VOID  _tmrfdCallback(PLW_CLASS_TIMER ptmr);
#endif                                                                  /*  LW_CFG_TIMERFD_EN > 0       */

             INTREG     iregInterLevel;
    REGISTER UINT16     usIndex;
    
    LW_OBJECT_HANDLE    ulThreadId;
    struct siginfo      siginfoTimer;
    struct sigevent     sigeventTimer;
    PLW_CLASS_TIMER     ptmr;
    
    usIndex = _ObjectGetIndex(ulTimer);
    
    LW_SPIN_LOCK_QUICK(&_K_slKernelTime, &iregInterLevel);
    ptmr          = &_K_tmrBuffer[usIndex];
    sigeventTimer = ptmr->TIMER_sigevent;
    ulThreadId    = ptmr->TIMER_ulThreadId;
    ptmr->TIMER_u64Overrun++;
    LW_SPIN_UNLOCK_QUICK(&_K_slKernelTime, iregInterLevel);
    
#if LW_CFG_TIMERFD_EN > 0
    if (ptmr->TIMER_pvTimerfd) {
        _tmrfdCallback(ptmr);
        return;
    }
#endif                                                                  /*  LW_CFG_TIMERFD_EN > 0       */

    siginfoTimer.si_signo   = ptmr->TIMER_sigevent.sigev_signo;
    siginfoTimer.si_code    = SI_TIMER;
    siginfoTimer.si_timerid = (INT)ulTimer;
    
    if (ptmr->TIMER_u64Overrun > DELAYTIMER_MAX) {
        siginfoTimer.si_overrun = DELAYTIMER_MAX;
    } else {
        siginfoTimer.si_overrun = (INT)ptmr->TIMER_u64Overrun;
    }
     
    _doSigEventEx(ulThreadId, &sigeventTimer, &siginfoTimer);           /*  发送定时信号                */
}
/*********************************************************************************************************
** 函数名称: __ptimerThreadDeleteHook
** 功能描述: 线程删除时, 需要将自己创建的 posix 定时器一并删除, 因为他不能再次接收到相关的信号了
** 输　入  : timer        定时器句柄
**           iFlag        标志
**           ptvNew       新的时间信息
**           ptvOld       先早的时间信息
** 输　出  : ERROR_NONE  or  PX_ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_PTIMER_AUTO_DEL_EN > 0

static VOID  __ptimerThreadDeleteHook (LW_OBJECT_HANDLE  ulId)
{
    REGISTER INT                 i;
    REGISTER PLW_CLASS_TIMER     ptmr;
    
    for (i = 0; i < LW_CFG_MAX_TIMERS; i++) {                           /*  扫描所有定时器              */
        ptmr = &_K_tmrBuffer[i];
        if (ptmr->TIMER_ucType != LW_TYPE_TIMER_UNUSED) {
            if (ptmr->TIMER_ulThreadId == ulId) {                       /*  是这个线程创建的            */
                if (ptmr->TIMER_ulTimer != _G_ulAlarmTimerHandle) {     /*  不是全局 alarm 定时器       */
                    API_TimerDelete(&ptmr->TIMER_ulTimer);              /*  删除这个定时器              */
                    ptmr->TIMER_ulTimer = 0ul;                          /*  防止重启误操作              */
                }
            }
        }
    }
}

#endif                                                                  /*  LW_CFG_PTIMER_AUTO_DEL_EN   */
/*********************************************************************************************************
** 函数名称: setitimer
** 功能描述: 设置内部定时器
** 输　入  : iWhich        类型, 仅支持 ITIMER_REAL 
**           pitValue      定时参数
**           pitOld        当前参数
** 输　出  : PX_ERROR or ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
INT  setitimer (INT                     iWhich, 
                const struct itimerval *pitValue,
                struct itimerval       *pitOld)
{
    struct itimerspec   tvNew;
    struct itimerspec   tvOld;
    
    __ALARM_TIMER_CREATE();
    
    if (iWhich != ITIMER_REAL) {
        _ErrorHandle(ENOSYS);
        return  (PX_ERROR);
    }
    
    if (pitValue) {
        tvNew.it_interval.tv_sec  = pitValue->it_interval.tv_sec;
        tvNew.it_interval.tv_nsec = pitValue->it_interval.tv_usec * 1000;
        
        tvNew.it_value.tv_sec  = pitValue->it_value.tv_sec;
        tvNew.it_value.tv_nsec = pitValue->it_value.tv_usec * 1000;
        
        timer_settime(_G_ulAlarmTimerHandle, 0, &tvNew, &tvOld);
        
    } else if (pitOld) {
        timer_gettime(_G_ulAlarmTimerHandle, &tvOld);
    }
    
    if (pitOld) {
        pitOld->it_interval.tv_sec  = tvOld.it_interval.tv_sec;
        pitOld->it_interval.tv_usec = tvOld.it_interval.tv_nsec / 1000;
        
        pitOld->it_value.tv_sec  = tvOld.it_value.tv_sec;
        pitOld->it_value.tv_usec = tvOld.it_value.tv_nsec / 1000;
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: setitimer
** 功能描述: 获取内部定时器
** 输　入  : iWhich        类型, 仅支持 ITIMER_REAL 
**           pitValue      获取当前定时信息
** 输　出  : PX_ERROR or ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
INT  getitimer (INT iWhich, struct itimerval *pitValue)
{
    struct itimerspec   tvOld;

    __ALARM_TIMER_CREATE();
    
    if (iWhich != ITIMER_REAL) {
        _ErrorHandle(ENOSYS);
        return  (PX_ERROR);
    }
    
    if (pitValue) {
        timer_gettime(_G_ulAlarmTimerHandle, &tvOld);
        
        pitValue->it_interval.tv_sec  = tvOld.it_interval.tv_sec;
        pitValue->it_interval.tv_usec = tvOld.it_interval.tv_nsec / 1000;
        
        pitValue->it_value.tv_sec  = tvOld.it_value.tv_sec;
        pitValue->it_value.tv_usec = tvOld.it_value.tv_nsec / 1000;
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: alarm
** 功能描述: 创建一个闹铃, 在指定时间后产生闹铃信号 (POSIX)
** 输　入  : uiSeconds    秒数
** 输　出  : PX_ERROR 或者前次闹铃剩余秒数
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
UINT  alarm (UINT  uiSeconds)
{
    struct itimerval tvValue, tvOld;
    
    tvValue.it_value.tv_sec  = uiSeconds;
    tvValue.it_value.tv_usec = 0;
    tvValue.it_interval.tv_sec  = 0;
    tvValue.it_interval.tv_usec = 0;
    
    if (setitimer(ITIMER_REAL, &tvValue, &tvOld) < ERROR_NONE) {
        return  (0);
    }
    
    return  ((UINT)tvOld.it_value.tv_sec);
}
/*********************************************************************************************************
** 函数名称: ualarm
** 功能描述: 创建一个闹铃, 在指定时间后产生闹铃信号 (POSIX)
** 输　入  : usec          微秒数
**           usecInterval  间隔微秒数
** 输　出  : PX_ERROR 或者前次闹铃剩余微秒数
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
useconds_t ualarm (useconds_t usec, useconds_t usecInterval)
{
    struct itimerval tvValue, tvOld;
    
    tvValue.it_value.tv_sec  = (time_t)(usec / __TIMEVAL_USEC_MAX);
    tvValue.it_value.tv_usec = (LONG)(usec % __TIMEVAL_USEC_MAX);
    tvValue.it_interval.tv_sec  = (time_t)(usecInterval / __TIMEVAL_USEC_MAX);
    tvValue.it_interval.tv_usec = (LONG)(usecInterval % __TIMEVAL_USEC_MAX);
    
    if (setitimer(ITIMER_REAL, &tvValue, &tvOld) < ERROR_NONE) {
        return  (0);
    }
    
    return  ((useconds_t)(tvOld.it_value.tv_sec * __TIMEVAL_USEC_MAX) + 
             (useconds_t)tvOld.it_value.tv_usec);
}
#endif                                                                  /*  LW_CFG_PTIMER_EN > 0        */
/*********************************************************************************************************
  END
*********************************************************************************************************/

