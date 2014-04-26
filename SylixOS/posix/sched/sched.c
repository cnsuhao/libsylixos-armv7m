/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                       SylixOS(TM)
**
**                               Copyright  All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: sched.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 12 月 30 日
**
** 描        述: posix 调度兼容库.

** BUG:
2011.03.26  Higher numerical values for the priority represent higher priorities.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../include/px_sched.h"                                        /*  已包含操作系统头文件        */
#include "../include/posixLib.h"                                        /*  posix 内部公共库            */
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_POSIX_EN > 0
/*********************************************************************************************************
** 函数名称: sched_get_priority_max
** 功能描述: 获得调度器允许的最大优先级 (pthread 线程不能与 idle 同优先级!)
** 输　入  : iPolicy       调度策略 (目前无用)
** 输　出  : 最大优先级
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sched_get_priority_max (int  iPolicy)
{
    (void)iPolicy;

    return  (__PX_PRIORITY_MAX);                                        /*  254                         */
}
/*********************************************************************************************************
** 函数名称: sched_get_priority_min
** 功能描述: 获得调度器允许的最小优先级
** 输　入  : iPolicy       调度策略 (目前无用)
** 输　出  : 最小优先级
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sched_get_priority_min (int  iPolicy)
{
    (void)iPolicy;
    
    return  (__PX_PRIORITY_MIN);                                        /*  1                           */
}
/*********************************************************************************************************
** 函数名称: sched_yield
** 功能描述: 将当前任务插入到同优先级调度器链表的最后, 主动让出一次 CPU 调度.
** 输　入  : NONE
** 输　出  : ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sched_yield (void)
{
    API_ThreadYield(API_ThreadIdSelf());
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: sched_setparam
** 功能描述: 设置指定任务调度器参数
** 输　入  : lThreadId     线程 ID
**           pschedparam   调度器参数
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sched_setparam (pid_t  lThreadId, const struct sched_param  *pschedparam)
{
    UINT8       ucPriority;
    ULONG       ulError;
    
    if (pschedparam == LW_NULL) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    if ((pschedparam->sched_priority < __PX_PRIORITY_MIN) ||
        (pschedparam->sched_priority > __PX_PRIORITY_MAX)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    ucPriority= (UINT8)__PX_PRIORITY_CONVERT(pschedparam->sched_priority);
    
    PX_ID_VERIFY(lThreadId, pid_t);
    
    ulError = API_ThreadSetPriority((LW_OBJECT_HANDLE)lThreadId, ucPriority);
    if (ulError) {
        errno = ESRCH;
        return  (PX_ERROR);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: sched_getparam
** 功能描述: 获得指定任务调度器参数
** 输　入  : lThreadId     线程 ID
**           pschedparam   调度器参数
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sched_getparam (pid_t  lThreadId, struct sched_param  *pschedparam)
{
    UINT8       ucPriority;
    ULONG       ulError;
    
    if (pschedparam == LW_NULL) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    PX_ID_VERIFY(lThreadId, pid_t);

    ulError = API_ThreadGetPriority((LW_OBJECT_HANDLE)lThreadId, &ucPriority);
    if (ulError) {
        errno = ESRCH;
        return  (PX_ERROR);
    } else {
        pschedparam->sched_priority = __PX_PRIORITY_CONVERT(ucPriority);
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: sched_setscheduler
** 功能描述: 设置指定任务调度器
** 输　入  : lThreadId     线程 ID
**           iPolicy       调度策略
**           pschedparam   调度器参数
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sched_setscheduler (pid_t                      lThreadId, 
                         int                        iPolicy, 
                         const struct sched_param  *pschedparam)
{
    UINT8       ucPriority;
    UINT8       ucActivatedMode;

    if (pschedparam == LW_NULL) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if ((iPolicy != LW_OPTION_SCHED_FIFO) &&
        (iPolicy != LW_OPTION_SCHED_RR)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if ((pschedparam->sched_priority < __PX_PRIORITY_MIN) ||
        (pschedparam->sched_priority > __PX_PRIORITY_MAX)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    ucPriority= (UINT8)__PX_PRIORITY_CONVERT(pschedparam->sched_priority);
    
    PX_ID_VERIFY(lThreadId, pid_t);
    
    if (API_ThreadGetSchedParam((LW_OBJECT_HANDLE)lThreadId,
                                LW_NULL,
                                &ucActivatedMode)) {
        errno = ESRCH;
        return  (PX_ERROR);
    }
    
    API_ThreadSetSchedParam((LW_OBJECT_HANDLE)lThreadId, (UINT8)iPolicy, ucActivatedMode);
    
    if (API_ThreadSetPriority((LW_OBJECT_HANDLE)lThreadId, ucPriority)) {
        errno = ESRCH;
        return  (PX_ERROR);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: sched_getscheduler
** 功能描述: 获得指定任务调度器
** 输　入  : lThreadId     线程 ID
** 输　出  : 调度策略
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sched_getscheduler (pid_t  lThreadId)
{
    UINT8   ucPolicy;
    
    PX_ID_VERIFY(lThreadId, pid_t);
    
    if (API_ThreadGetSchedParam((LW_OBJECT_HANDLE)lThreadId,
                                &ucPolicy,
                                LW_NULL)) {
        errno = ESRCH;
        return  (PX_ERROR);
    } else {
        return  ((int)ucPolicy);
    }
}
/*********************************************************************************************************
** 函数名称: sched_rr_get_interval
** 功能描述: 获得指定任务调度器
** 输　入  : lThreadId     线程 ID
**           interval      current execution time limit.
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sched_rr_get_interval (pid_t  lThreadId, struct timespec  *interval)
{
    UINT8               ucPolicy;
    UINT16              usCounter = 0;

    if (interval) {
        errno = EINVAL;
        return  (PX_ERROR);
    }

    if (API_ThreadGetSchedParam((LW_OBJECT_HANDLE)lThreadId,
                                &ucPolicy,
                                LW_NULL)) {
        errno = ESRCH;
        return  (PX_ERROR);
    }
    if (ucPolicy != LW_OPTION_SCHED_RR) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if (API_ThreadGetSliceEx((LW_OBJECT_HANDLE)lThreadId, LW_NULL, &usCounter)) {
        errno = ESRCH;
        return  (PX_ERROR);
    }
    
    __tickToTimespec((ULONG)usCounter, interval);

    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
