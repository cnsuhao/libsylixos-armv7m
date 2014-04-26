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
** 文   件   名: pthread_cond.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 12 月 30 日
**
** 描        述: pthread 条件变量兼容库. (cond 从来不会发生 EINTR)

** BUG:
2012.12.13  由于 SylixOS 支持进程资源回收, 这里开始支持静态初始化.
2013.05.01  If successful, the pthread_cond_*() functions shall return zero; 
            otherwise, an error number shall be returned to indicate the error.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../include/px_pthread.h"                                      /*  已包含操作系统头文件        */
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_POSIX_EN > 0
/*********************************************************************************************************
  mutex 隐形创建
*********************************************************************************************************/
extern void  __pthread_mutex_init_invisible(pthread_mutex_t  *pmutex);
/*********************************************************************************************************
** 函数名称: __pthread_cond_init_invisible
** 功能描述: 条件变量隐形创建. (静态初始化)
** 输　入  : pcond         条件变量控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static void  __pthread_cond_init_invisible (pthread_cond_t  *pcond)
{
    if (pcond && 
        !pcond->TCD_ulSignal &&
        !pcond->TCD_ulMutxe) {
        pthread_cond_init(pcond, LW_NULL);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_condattr_init
** 功能描述: 初始化条件变量属性块.
** 输　入  : pcondattr     属性
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_condattr_init (pthread_condattr_t  *pcondattr)
{
    API_ThreadCondAttrInit(pcondattr);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_condattr_destroy
** 功能描述: 销毁条件变量属性块.
** 输　入  : pcondattr     属性
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_condattr_destroy (pthread_condattr_t  *pcondattr)
{
    API_ThreadCondAttrDestroy(pcondattr);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_condattr_setpshared
** 功能描述: 设置条件变量属性块是否为进程.
** 输　入  : pcondattr     属性
**           ishare        是否为进程共享
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_condattr_setpshared (pthread_condattr_t  *pcondattr, int  ishare)
{
    API_ThreadCondAttrSetPshared(pcondattr, ishare);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_condattr_getpshared
** 功能描述: 获得条件变量属性块是否为进程.
** 输　入  : pcondattr     属性
**           pishare       是否为进程共享
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_condattr_getpshared (const pthread_condattr_t  *pcondattr, int  *pishare)
{
    API_ThreadCondAttrGetPshared(pcondattr, pishare);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_cond_init
** 功能描述: 初始化条件变量.
** 输　入  : pcond         条件变量控制块
**           pcondattr     属性
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_cond_init (pthread_cond_t  *pcond, const pthread_condattr_t  *pcondattr)
{
    ULONG       ulAttr = LW_THREAD_PROCESS_SHARED;
    
    if (pcond == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    if (pcondattr) {
        ulAttr = *pcondattr;
    }

    if (API_ThreadCondInit(pcond, ulAttr)) {                            /*  初始化条件变量              */
        return  (errno);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_cond_destroy
** 功能描述: 销毁条件变量.
** 输　入  : pcond         条件变量控制块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_cond_destroy (pthread_cond_t  *pcond)
{
    if (pcond == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    if (API_ThreadCondDestroy(pcond)) {                                 /*  销毁条件变量                */
        return  (errno);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_cond_signal
** 功能描述: 发送一个条件变量有效信号.
** 输　入  : pcond         条件变量控制块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_cond_signal (pthread_cond_t  *pcond)
{
    if (pcond == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_cond_init_invisible(pcond);
    
    if (API_ThreadCondSignal(pcond)) {
        return  (errno);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_cond_broadcast
** 功能描述: 发送一个条件变量广播信号.
** 输　入  : pcond         条件变量控制块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_cond_broadcast (pthread_cond_t  *pcond)
{
    if (pcond == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_cond_init_invisible(pcond);
    
    if (API_ThreadCondBroadcast(pcond)) {
        return  (errno);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_cond_wait
** 功能描述: 等待一个条件变量广播信号.
** 输　入  : pcond         条件变量控制块
**           pmutex        互斥信号量
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_cond_wait (pthread_cond_t  *pcond, pthread_mutex_t  *pmutex)
{
    if ((pcond == LW_NULL) || (pmutex == LW_NULL)) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_cond_init_invisible(pcond);
    __pthread_mutex_init_invisible(pmutex);
    
    if (API_ThreadCondWait(pcond, pmutex->PMUTEX_ulMutex, LW_OPTION_WAIT_INFINITE)) {
        errno = EINVAL;
        return  (EINVAL);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_cond_timedwait
** 功能描述: 等待一个条件变量广播信号(带有超时).
** 输　入  : pcond         条件变量控制块
**           pmutex        互斥信号量
**           abs_timeout   超时时间 (注意: 这里是绝对时间, 即一个确定的历史时间例如: 2009.12.31 15:36:04)
                           笔者觉得这个不是很爽, 应该再加一个函数可以等待相对时间.
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_cond_timedwait (pthread_cond_t         *pcond, 
                             pthread_mutex_t        *pmutex,
                             const struct timespec  *abs_timeout)
{
    ULONG               ulTimeout;
    ULONG               ulError;
    struct timespec     tvNow;
    struct timespec     tvWait = {0, 0};
    
    if ((pcond == LW_NULL) || (pmutex == LW_NULL)) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    if ((abs_timeout == LW_NULL) || (abs_timeout->tv_nsec >= __TIMEVAL_NSEC_MAX)) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_cond_init_invisible(pcond);
    __pthread_mutex_init_invisible(pmutex);
    
    lib_clock_gettime(CLOCK_REALTIME, &tvNow);                          /*  获得当前系统时间            */
    if (__timespecLeftTime(&tvNow, abs_timeout)) {
        tvWait = *abs_timeout;
        __timespecSub(&tvWait, &tvNow);                                 /*  计算与当前等待的时间间隔    */
    }
    /*
     *  注意: 当 tvWait 超过ulong tick范围时, 将自然产生溢出, 导致超时时间不准确.
     */
    ulTimeout = __timespecToTick(&tvWait);                              /*  变换为 tick                 */
    
    ulError = API_ThreadCondWait(pcond, pmutex->PMUTEX_ulMutex, ulTimeout);
    if (ulError == ERROR_THREAD_WAIT_TIMEOUT) {
        errno = ETIMEDOUT;
        return  (ETIMEDOUT);
        
    } else if (ulError) {
        errno = EINVAL;
        return  (EINVAL);
        
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_cond_reltimedwait_np
** 功能描述: 等待一个条件变量广播信号(带有超时).
** 输　入  : pcond         条件变量控制块
**           pmutex        互斥信号量
**           rel_timeout   相对超时时间.
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if LW_CFG_POSIXEX_EN > 0

LW_API 
int  pthread_cond_reltimedwait_np (pthread_cond_t         *pcond, 
                                   pthread_mutex_t        *pmutex,
                                   const struct timespec  *rel_timeout)
{
    ULONG               ulTimeout;
    ULONG               ulError;
    
    if ((pcond == LW_NULL) || (pmutex == LW_NULL)) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    if ((rel_timeout == LW_NULL) || (rel_timeout->tv_nsec >= __TIMEVAL_NSEC_MAX)) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_cond_init_invisible(pcond);
    __pthread_mutex_init_invisible(pmutex);
    
    /*
     *  注意: 当 rel_timeout 超过ulong tick范围时, 将自然产生溢出, 导致超时时间不准确.
     */
    ulTimeout = __timespecToTick(rel_timeout);                          /*  变换为 tick                 */
    
    ulError = API_ThreadCondWait(pcond, pmutex->PMUTEX_ulMutex, ulTimeout);
    if (ulError == ERROR_THREAD_WAIT_TIMEOUT) {
        errno = ETIMEDOUT;
        return  (ETIMEDOUT);
        
    } else if (ulError) {
        errno = EINVAL;
        return  (EINVAL);
        
    } else {
        return  (ERROR_NONE);
    }
}

#endif                                                                  /*  LW_CFG_POSIXEX_EN > 0       */
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
