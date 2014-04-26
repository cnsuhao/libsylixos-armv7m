/*
 * SylixOS(TM)  LW : long wing
 * Copyright All Rights Reserved
 *
 * MODULE PRIVATE DYNAMIC MEMORY IN VPROCESS PATCH
 * this file is support current module malloc/free use his own heap in a vprocess.
 * 
 * Author: Han.hui <sylixos@gmail.com>
 */

#include <unistd.h>
#include <sys/time.h>

#if LW_CFG_PTIMER_EN > 0

static LW_OBJECT_HANDLE intimer = LW_OBJECT_HANDLE_INVALID;

#define __ALARM_TIMER_CREATE() \
        if (intimer == 0) { \
            if (timer_create(CLOCK_REALTIME, LW_NULL, &intimer) != ERROR_NONE) { \
                return  (-1); \
            } \
        }

/*
 *  setitimer
 */
int  setitimer (int which, const struct itimerval *pitValue, struct itimerval *pitOld)
{
    struct itimerspec   tvNew;
    struct itimerspec   tvOld;
    
    __ALARM_TIMER_CREATE();
    
    if (which != ITIMER_REAL) {
        errno = ENOSYS;
        return  (PX_ERROR);
    }
    
    if (pitValue) {
        tvNew.it_interval.tv_sec  = pitValue->it_interval.tv_sec;
        tvNew.it_interval.tv_nsec = pitValue->it_interval.tv_usec * 1000;
        
        tvNew.it_value.tv_sec  = pitValue->it_value.tv_sec;
        tvNew.it_value.tv_nsec = pitValue->it_value.tv_usec * 1000;
        
        timer_settime(intimer, 0, &tvNew, &tvOld);
        
    } else if (pitOld) {
        timer_gettime(intimer, &tvOld);
    }
    
    if (pitOld) {
        pitOld->it_interval.tv_sec  = tvOld.it_interval.tv_sec;
        pitOld->it_interval.tv_usec = tvOld.it_interval.tv_nsec / 1000;
        
        pitOld->it_value.tv_sec  = tvOld.it_value.tv_sec;
        pitOld->it_value.tv_usec = tvOld.it_value.tv_nsec / 1000;
    }

    return  (ERROR_NONE);
}

/*
 *  getitimer
 */
int  getitimer (int which, struct itimerval *pitValue)
{
    struct itimerspec   tvOld;

    __ALARM_TIMER_CREATE();
    
    if (which != ITIMER_REAL) {
        errno = ENOSYS;
        return  (PX_ERROR);
    }
    
    if (pitValue) {
        timer_gettime(intimer, &tvOld);
        
        pitValue->it_interval.tv_sec  = tvOld.it_interval.tv_sec;
        pitValue->it_interval.tv_usec = tvOld.it_interval.tv_nsec / 1000;
        
        pitValue->it_value.tv_sec  = tvOld.it_value.tv_sec;
        pitValue->it_value.tv_usec = tvOld.it_value.tv_nsec / 1000;
    }
    
    return  (ERROR_NONE);
}

/*
 *  alarm
 */
unsigned int  alarm (unsigned int  uiSeconds)
{
    struct itimerval tvValue, tvOld;
    
    tvValue.it_value.tv_sec  = uiSeconds;
    tvValue.it_value.tv_usec = 0;
    tvValue.it_interval.tv_sec  = 0;
    tvValue.it_interval.tv_usec = 0;
    
    if (setitimer(ITIMER_REAL, &tvValue, &tvOld) < ERROR_NONE) {
        return  (0);
    }
    
    return  ((unsigned int)tvOld.it_value.tv_sec);
}

/*
 *  ualarm
 */
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

#endif /* LW_CFG_PTIMER_EN > 0 */