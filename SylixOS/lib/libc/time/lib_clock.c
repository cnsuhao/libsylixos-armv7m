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
** 文   件   名: lib_clock.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 08 月 30 日
**
** 描        述: 系统库.

** BUG:
2013.11.28  加入 lib_clock_nanosleep().
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#if LW_CFG_MODULELOADER_EN > 0
#include "../SylixOS/loader/include/loader_vppatch.h"
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
/*********************************************************************************************************
  高分辨率时钟接口
*********************************************************************************************************/
#if LW_CFG_TIME_HIGH_RESOLUTION_EN > 0
#define LW_TIME_HIGH_RESOLUTION(tv)     bspTickHighResolution(tv)
#else
#define LW_TIME_HIGH_RESOLUTION(tv)
#endif                                                                  /*  LW_CFG_TIME_HIGH_RESOLUT... */
/*********************************************************************************************************
** 函数名称: lib_clock
** 功能描述: 
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_RTC_EN > 0

clock_t  lib_clock (VOID)
{
    return  ((clock_t)API_TimeGet());
}
/*********************************************************************************************************
** 函数名称: lib_clock_getres
** 功能描述: 
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
** 注  意  : 这里并没有判断真正的时钟精度.
*********************************************************************************************************/
INT  lib_clock_getres (clockid_t  clockid, struct timespec *res)
{
    if (!res) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    res->tv_sec  = 0;
    res->tv_nsec = 1;
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: lib_clock_gettime
** 功能描述: 
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  lib_clock_gettime (clockid_t  clockid, struct timespec  *tv)
{
    INTREG          iregInterLevel;
    PLW_CLASS_TCB   ptcbCur;

    if (tv == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    switch (clockid) {
    
    case CLOCK_REALTIME:
        LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);
        *tv = _K_tvTODCurrent;
        LW_TIME_HIGH_RESOLUTION(tv);
        LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);
        break;
    
    case CLOCK_MONOTONIC:
        LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);
        *tv = _K_tvTODMono;
        LW_TIME_HIGH_RESOLUTION(tv);
        LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);
        break;
        
    case CLOCK_PROCESS_CPUTIME_ID:
#if LW_CFG_MODULELOADER_EN > 0
        {
            LW_LD_VPROC *pvproc = __LW_VP_GET_CUR_PROC();
            if (pvproc == LW_NULL) {
                _ErrorHandle(ESRCH);
                return  (PX_ERROR);
            }
            LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);
            __tickToTimespec(pvproc->VP_clockUser + pvproc->VP_clockSystem, tv);
            LW_TIME_HIGH_RESOLUTION(tv);
            LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);
        }
#else
        _ErrorHandle(ENOSYS);
        return  (PX_ERROR);
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
        break;
        
    case CLOCK_THREAD_CPUTIME_ID:
        LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);
        LW_TCB_GET_CUR(ptcbCur);
        __tickToTimespec(ptcbCur->TCB_ulCPUTicks, tv);
        LW_TIME_HIGH_RESOLUTION(tv);
        LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);
        break;
        
    default:
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: lib_clock_settime
** 功能描述: 
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  lib_clock_settime (clockid_t  clockid, const struct timespec  *tv)
{
    INTREG      iregInterLevel;

    if (tv == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    if (clockid != CLOCK_REALTIME) {                                    /*  CLOCK_REALTIME              */
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);
    _K_tvTODCurrent = *tv;
    LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: lib_clock_nanosleep
** 功能描述: 
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  lib_clock_nanosleep (clockid_t  clockid, int  iFlags, 
                          const struct timespec  *rqtp, struct timespec  *rmtp)
{
    INTREG           iregInterLevel;
    struct timespec  tvValue;

    if ((clockid != CLOCK_REALTIME) && (clockid != CLOCK_MONOTONIC)) {
        _ErrorHandle(ENOTSUP);
        return  (PX_ERROR);
    }
    
    if (!rqtp) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    tvValue = *rqtp;
    
    if (iFlags == TIMER_ABSTIME) {                                      /*  绝对时间                    */
        struct timespec  tvNow;
        
        LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);
        if (clockid == CLOCK_REALTIME) {
            tvNow = _K_tvTODCurrent;
        } else {
            tvNow = _K_tvTODMono;
        }
        LW_TIME_HIGH_RESOLUTION(&tvNow);
        LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);

        if ((rqtp->tv_sec < tvNow.tv_sec) ||
            ((rqtp->tv_sec == tvNow.tv_sec) &&
             (rqtp->tv_nsec < tvNow.tv_nsec))) {
            _ErrorHandle(EINVAL);
            return  (PX_ERROR);
        }
        
        __timespecSub(&tvValue, &tvNow);
        
        return  (nanosleep(&tvValue, LW_NULL));
    
    } else {
        return  (nanosleep(&tvValue, rmtp));
    }
}

#endif                                                                  /*  LW_CFG_RTC_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
