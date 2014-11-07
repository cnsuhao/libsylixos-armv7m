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
** 文   件   名: loader_vptimer.c
**
** 创   建   人: Han.hui (韩辉)
**
** 文件创建日期: 2011 年 08 月 03 日
**
** 描        述: 进程定时器支持.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
#include "sys/time.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if (LW_CFG_MODULELOADER_EN > 0) && (LW_CFG_PTIMER_EN > 0)
#include "../include/loader_lib.h"
#include "../include/loader_vppatch.h"
/*********************************************************************************************************
  进程列表
*********************************************************************************************************/
extern LW_LIST_LINE_HEADER      _G_plineVProcHeader;
/*********************************************************************************************************
** 函数名称: vprocItimerHook
** 功能描述: 进程定时器 tick hook
** 输　入  : iWhich        类型, ITIMER_REAL / ITIMER_VIRTUAL / ITIMER_PROF
**           pitValue      定时参数
**           pitOld        当前参数
** 输　出  : PX_ERROR or ERROR_NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  vprocItimerHook (PLW_CLASS_TCB  ptcb, PLW_CLASS_CPU  pcpu)
{
    LW_LD_VPROC     *pvproc;
    LW_LD_VPROC_T   *pvptimer;
    LW_LIST_LINE    *plineTemp;
    struct sigevent  sigeventTimer;
    struct siginfo   siginfoTimer;
    LW_OBJECT_HANDLE ulThreadId;
    
    pvproc = __LW_VP_GET_TCB_PROC(ptcb);
    if (pvproc == LW_NULL) {
        pvproc     = &_G_vprocKernel;
        ulThreadId = ptcb->TCB_ulId;
    } else {
        ulThreadId = pvproc->VP_ulMainThread;
    }
    
    sigeventTimer.sigev_notify          = SIGEV_SIGNAL;
    sigeventTimer.sigev_value.sival_ptr = LW_NULL;
    
    siginfoTimer.si_errno = ERROR_NONE;
    siginfoTimer.si_code  = SI_TIMER;
    
    if (pcpu->CPU_iKernelCounter == 0) {                                /*  ITIMER_VIRTUAL 定时器       */
        pvptimer = &pvproc->VP_vptimer[ITIMER_VIRTUAL];
        if (pvptimer->VPT_ulCounter) {
            pvptimer->VPT_ulCounter--;
            if (pvptimer->VPT_ulCounter == 0ul) {
                sigeventTimer.sigev_signo = SIGVTALRM;
                siginfoTimer.si_signo     = SIGVTALRM;
                siginfoTimer.si_timerid   = ITIMER_VIRTUAL;
                _doSigEventEx(ulThreadId, &sigeventTimer, &siginfoTimer);
                pvptimer->VPT_ulCounter = pvptimer->VPT_ulInterval;
            }
        }
    }
    
    pvptimer = &pvproc->VP_vptimer[ITIMER_PROF];
    if (pvptimer->VPT_ulCounter) {                                      /*  ITIMER_PROF 定时器          */
        pvptimer->VPT_ulCounter--;
        if (pvptimer->VPT_ulCounter == 0ul) {
            sigeventTimer.sigev_signo = SIGPROF;
            siginfoTimer.si_signo     = SIGPROF;
            siginfoTimer.si_timerid   = ITIMER_PROF;
            _doSigEventEx(ulThreadId, &sigeventTimer, &siginfoTimer);
            pvptimer->VPT_ulCounter = pvptimer->VPT_ulInterval;
        }
    }
    
    sigeventTimer.sigev_signo = SIGALRM;
    siginfoTimer.si_signo     = SIGALRM;
    siginfoTimer.si_timerid   = ITIMER_REAL;
    
    for (plineTemp  = _G_plineVProcHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {                 /*  ITIMER_REAL 定时器          */
         
        pvproc   = _LIST_ENTRY(plineTemp, LW_LD_VPROC, VP_lineManage);
        pvptimer = &pvproc->VP_vptimer[ITIMER_REAL];
        if (pvptimer->VPT_ulCounter) {
            pvptimer->VPT_ulCounter--;
            if (pvptimer->VPT_ulCounter == 0ul) {
                if (pvproc == &_G_vprocKernel) {
                    ulThreadId = ptcb->TCB_ulId;
                } else {
                    ulThreadId = pvproc->VP_ulMainThread;
                }
                _doSigEventEx(ulThreadId, &sigeventTimer, &siginfoTimer);
                pvptimer->VPT_ulCounter = pvptimer->VPT_ulInterval;
            }
        }
    }
}
/*********************************************************************************************************
** 函数名称: vprocSetitimer
** 功能描述: 设置进程内部定时器
** 输　入  : iWhich        类型, ITIMER_REAL / ITIMER_VIRTUAL / ITIMER_PROF
**           pitValue      定时参数
**           pitOld        当前参数
** 输　出  : PX_ERROR or ERROR_NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  vprocSetitimer (INT        iWhich, 
                     ULONG      ulCounter,
                     ULONG      ulInterval,
                     ULONG     *pulCounter,
                     ULONG     *pulInterval)
{
    INTREG           iregInterLevel;
    LW_LD_VPROC     *pvproc;
    LW_LD_VPROC_T   *pvptimer;
    PLW_CLASS_TCB    ptcbCur;

    LW_TCB_GET_CUR_SAFE(ptcbCur);
    
    pvproc = __LW_VP_GET_TCB_PROC(ptcbCur);
    if (pvproc == LW_NULL) {
        pvproc = &_G_vprocKernel;
    }
    
    pvptimer = &pvproc->VP_vptimer[iWhich];
    
    LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);
    if (pulCounter) {
        *pulCounter = pvptimer->VPT_ulCounter;
    }
    if (pulInterval) {
        *pulInterval = pvptimer->VPT_ulInterval;
    }
    pvptimer->VPT_ulCounter  = ulCounter;
    pvptimer->VPT_ulInterval = ulInterval;
    LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: vprocGetitimer
** 功能描述: 获取进程内部定时器
** 输　入  : iWhich        类型, ITIMER_REAL / ITIMER_VIRTUAL / ITIMER_PROF
**           pitValue      获取当前定时信息
** 输　出  : PX_ERROR or ERROR_NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  vprocGetitimer (INT        iWhich, 
                     ULONG     *pulCounter,
                     ULONG     *pulInterval)
{
    INTREG           iregInterLevel;
    LW_LD_VPROC     *pvproc;
    LW_LD_VPROC_T   *pvptimer;
    PLW_CLASS_TCB    ptcbCur;

    LW_TCB_GET_CUR_SAFE(ptcbCur);
    
    pvproc = __LW_VP_GET_TCB_PROC(ptcbCur);
    if (pvproc == LW_NULL) {
        pvproc = &_G_vprocKernel;
    }
    
    pvptimer = &pvproc->VP_vptimer[iWhich];
    
    LW_SPIN_LOCK_QUICK(&_K_slKernel, &iregInterLevel);
    if (pulCounter) {
        *pulCounter = pvptimer->VPT_ulCounter;
    }
    if (pulInterval) {
        *pulInterval = pvptimer->VPT_ulInterval;
    }
    LW_SPIN_UNLOCK_QUICK(&_K_slKernel, iregInterLevel);
    
    return  (ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
                                                                        /*  LW_CFG_PTIMER_EN > 0        */
/*********************************************************************************************************
  END
*********************************************************************************************************/
