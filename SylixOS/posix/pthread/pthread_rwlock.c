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
** 文   件   名: pthread_rwlock.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 12 月 30 日
**
** 描        述: pthread 读写锁兼容库. (使用了双信号量的算法, 写者优先) (rwlock 从来不会发生 EINTR)
** 注        意: SylixOS pthread_rwlock_rdlock() 不支持同一线程连续调用!!!

** BUG:
2010.01.12  加锁时阻塞时, 只需将 pend 计数器一次自增一次即可.
2010.01.13  使用计数信号量充当同步锁, 每次状态的改变激活所有的线程, 他们将自己进行抢占.
2012.12.13  由于 SylixOS 支持进程资源回收, 这里开始支持静态初始化.
2013.05.01  If successful, the pthread_rwlockattr_*() and pthread_rwlock_*() functions shall return zero;
            otherwise, an error number shall be returned to indicate the error.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../include/px_pthread.h"                                      /*  已包含操作系统头文件        */
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_POSIX_EN > 0
/*********************************************************************************************************
  宏
*********************************************************************************************************/
#define __PX_RWLOCKATTR_INIT            0x8000

#define __PX_RWLOCK_LOCK(prwlock)       \
        API_SemaphoreMPend(prwlock->PRWLOCK_ulMutex, LW_OPTION_WAIT_INFINITE)
#define __PX_RWLOCK_UNLOCK(prwlock)     \
        API_SemaphoreMPost(prwlock->PRWLOCK_ulMutex)
        
#define __PX_RWLOCK_STATUS_READING      0x0000
#define __PX_RWLOCK_STATUS_WRITING      0x0001
/*********************************************************************************************************
** 函数名称: __pthread_rwlock_init_invisible
** 功能描述: 读写锁隐形创建. (静态初始化)
** 输　入  : prwlock        句柄
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static void  __pthread_rwlock_init_invisible (pthread_rwlock_t  *prwlock)
{
    if (prwlock && 
        !prwlock->PRWLOCK_ulMutex &&
        !prwlock->PRWLOCK_ulRSemaphore &&
        !prwlock->PRWLOCK_ulWSemaphore &&
        !prwlock->PRWLOCK_ulOwner) {
        pthread_rwlock_init(prwlock, LW_NULL);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_rwlockattr_init
** 功能描述: 初始化一个读写锁属性块.
** 输　入  : prwlockattr    属性块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlockattr_init (pthread_rwlockattr_t  *prwlockattr)
{
    if (prwlockattr == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }

    *prwlockattr = __PX_RWLOCKATTR_INIT;

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_rwlockattr_destroy
** 功能描述: 销毁一个读写锁属性块.
** 输　入  : prwlockattr    属性块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlockattr_destroy (pthread_rwlockattr_t  *prwlockattr)
{
    if (prwlockattr == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    *prwlockattr = 0;
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_rwlockattr_setpshared
** 功能描述: 设置一个读写锁属性块是否进程共享.
** 输　入  : prwlockattr    属性块
**           pshared        共享
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlockattr_setpshared (pthread_rwlockattr_t *prwlockattr, int  pshared)
{
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_rwlockattr_getpshared
** 功能描述: 获取一个读写锁属性块是否进程共享.
** 输　入  : prwlockattr    属性块
**           pshared        共享(返回)
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlockattr_getpshared (const pthread_rwlockattr_t *prwlockattr, int  *pshared)
{
    if (pshared) {
        *pshared = PTHREAD_PROCESS_PRIVATE;
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_rwlock_init
** 功能描述: 创建一个读写锁.
** 输　入  : prwlock        句柄
**           prwlockattr    属性块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlock_init (pthread_rwlock_t  *prwlock, const pthread_rwlockattr_t  *prwlockattr)
{
    if (prwlock == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    prwlock->PRWLOCK_ulRSemaphore = API_SemaphoreCCreate("prwrlock", 0, __ARCH_ULONG_MAX, 
                                                        LW_OPTION_WAIT_PRIORITY, LW_NULL);
    if (prwlock->PRWLOCK_ulRSemaphore == LW_OBJECT_HANDLE_INVALID) {
        errno = ENOSPC;
        return  (ENOSPC);
    }
    prwlock->PRWLOCK_ulWSemaphore = API_SemaphoreCCreate("prwwlock", 0, __ARCH_ULONG_MAX, 
                                                        LW_OPTION_WAIT_PRIORITY, LW_NULL);
    if (prwlock->PRWLOCK_ulWSemaphore == LW_OBJECT_HANDLE_INVALID) {
        API_SemaphoreCDelete(&prwlock->PRWLOCK_ulRSemaphore);
        errno = ENOSPC;
        return  (ENOSPC);
    }
    prwlock->PRWLOCK_ulMutex = API_SemaphoreMCreate("prwlock_m", LW_PRIO_DEF_CEILING, 
                                                    (LW_OPTION_INHERIT_PRIORITY |
                                                     LW_OPTION_DELETE_SAFE |
                                                     LW_OPTION_WAIT_PRIORITY), LW_NULL);
    if (prwlock->PRWLOCK_ulMutex == LW_OBJECT_HANDLE_INVALID) {
        API_SemaphoreCDelete(&prwlock->PRWLOCK_ulRSemaphore);
        API_SemaphoreCDelete(&prwlock->PRWLOCK_ulWSemaphore);
        errno = ENOSPC;
        return  (ENOSPC);
    }
    
    prwlock->PRWLOCK_uiOpCounter    = 0;
    prwlock->PRWLOCK_uiRPendCounter = 0;
    prwlock->PRWLOCK_uiWPendCounter = 0;
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_rwlock_destroy
** 功能描述: 销毁一个读写锁.
** 输　入  : prwlock        句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlock_destroy (pthread_rwlock_t  *prwlock)
{
    if (prwlock == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    if (API_SemaphoreCDelete(&prwlock->PRWLOCK_ulRSemaphore)) {
        errno = EINVAL;
        return  (EINVAL);
    }
    API_SemaphoreCDelete(&prwlock->PRWLOCK_ulWSemaphore);
    API_SemaphoreMDelete(&prwlock->PRWLOCK_ulMutex);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_rwlock_rdlock
** 功能描述: 等待一个读写锁可读.
** 输　入  : prwlock        句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlock_rdlock (pthread_rwlock_t  *prwlock)
{
    if (prwlock == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_rwlock_init_invisible(prwlock);

    __PX_RWLOCK_LOCK(prwlock);                                          /*  锁定读写锁                  */
    prwlock->PRWLOCK_uiRPendCounter++;                                  /*  等待读的数量++              */
    do {
        if ((prwlock->PRWLOCK_uiStatus == __PX_RWLOCK_STATUS_READING) &&
            (prwlock->PRWLOCK_uiWPendCounter == 0)) {
            /*
             *  如果写入器未持有读锁, 并且没有任何写入器基于改锁阻塞. 则调用线程立即获取读锁.
             */
            prwlock->PRWLOCK_uiOpCounter++;                             /*  正在操作数量++              */
            prwlock->PRWLOCK_uiRPendCounter--;                          /*  退出等待模式                */
            break;                                                      /*  直接跳出                    */
        
        } else {
            /*
             *  如果写入器正在操作或者, 或者有多个写入器正在等待该锁, 将阻塞.
             */
            __PX_RWLOCK_UNLOCK(prwlock);                                /*  解锁读写锁                  */
            API_SemaphoreCPend(prwlock->PRWLOCK_ulRSemaphore,
                               LW_OPTION_WAIT_INFINITE);                /*  等待读写锁状态变换          */
            __PX_RWLOCK_LOCK(prwlock);                                  /*  锁定读写锁                  */
        }
        
    } while (1);
    __PX_RWLOCK_UNLOCK(prwlock);                                        /*  解锁读写锁                  */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_rwlock_tryrdlock
** 功能描述: 非阻塞等待一个读写锁可读.
** 输　入  : prwlock        句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlock_tryrdlock (pthread_rwlock_t  *prwlock)
{
    if (prwlock == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_rwlock_init_invisible(prwlock);

    __PX_RWLOCK_LOCK(prwlock);                                          /*  锁定读写锁                  */
    if ((prwlock->PRWLOCK_uiStatus == __PX_RWLOCK_STATUS_READING) &&
        (prwlock->PRWLOCK_uiWPendCounter == 0)) {
        prwlock->PRWLOCK_uiOpCounter++;                                 /*  正在操作数量++              */
        __PX_RWLOCK_UNLOCK(prwlock);                                    /*  解锁读写锁                  */
        return  (ERROR_NONE);
    
    } else {
        __PX_RWLOCK_UNLOCK(prwlock);                                    /*  解锁读写锁                  */
        errno = EBUSY;
        return  (EBUSY);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_rwlock_wrlock
** 功能描述: 等待一个读写锁可写.
** 输　入  : prwlock        句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlock_wrlock (pthread_rwlock_t  *prwlock)
{
    if (prwlock == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_rwlock_init_invisible(prwlock);

    __PX_RWLOCK_LOCK(prwlock);                                          /*  锁定读写锁                  */
    prwlock->PRWLOCK_uiWPendCounter++;                                  /*  等待写的数量++              */
    do {
        if (prwlock->PRWLOCK_uiOpCounter == 0) {
            /*
             *  没有正在操作的线程
             */
            prwlock->PRWLOCK_uiOpCounter++;                             /*  正在操作的线程++            */
            prwlock->PRWLOCK_uiWPendCounter--;                          /*  退出等待状态                */
            prwlock->PRWLOCK_uiStatus = __PX_RWLOCK_STATUS_WRITING;     /*  转换为写模式                */
            break;
            
        } else {
            /*
             *  如果有等待写入的线程或者存在正在读取的线程.
             */
            __PX_RWLOCK_UNLOCK(prwlock);                                /*  解锁读写锁                  */
            API_SemaphoreCPend(prwlock->PRWLOCK_ulWSemaphore,
                               LW_OPTION_WAIT_INFINITE);                /*  等待读写锁状态变换          */
            __PX_RWLOCK_LOCK(prwlock);                                  /*  锁定读写锁                  */
        }
    } while (1);
    prwlock->PRWLOCK_ulOwner = API_ThreadIdSelf();                      /*  读写锁拥有者                */
    __PX_RWLOCK_UNLOCK(prwlock);                                        /*  解锁读写锁                  */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_rwlock_trywrlock
** 功能描述: 非阻塞等待一个读写锁可写.
** 输　入  : prwlock        句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlock_trywrlock (pthread_rwlock_t  *prwlock)
{
    if (prwlock == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_rwlock_init_invisible(prwlock);

    __PX_RWLOCK_LOCK(prwlock);                                          /*  锁定读写锁                  */
    if (prwlock->PRWLOCK_uiOpCounter == 0) {
        prwlock->PRWLOCK_uiOpCounter++;                                 /*  正在操作的线程++            */
        prwlock->PRWLOCK_uiStatus = __PX_RWLOCK_STATUS_WRITING;         /*  转换为写模式                */
        prwlock->PRWLOCK_ulOwner  = API_ThreadIdSelf();                 /*  读写锁拥有者                */
        __PX_RWLOCK_UNLOCK(prwlock);                                    /*  解锁读写锁                  */
        return  (ERROR_NONE);
    
    } else {
        __PX_RWLOCK_UNLOCK(prwlock);                                    /*  解锁读写锁                  */
        errno = EBUSY;
        return  (EBUSY);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_rwlock_unlock
** 功能描述: 释放一个读写锁.
** 输　入  : prwlock        句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_rwlock_unlock (pthread_rwlock_t  *prwlock)
{
    ULONG       ulReleasNum;

    if (prwlock == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    __pthread_rwlock_init_invisible(prwlock);

    __PX_RWLOCK_LOCK(prwlock);                                          /*  锁定读写锁                  */
    if (prwlock->PRWLOCK_uiStatus == __PX_RWLOCK_STATUS_WRITING) {
        /*
         *  写状态下释放.
         */
        if (prwlock->PRWLOCK_ulOwner != API_ThreadIdSelf()) {           /*  不是读写锁拥有者            */
            __PX_RWLOCK_UNLOCK(prwlock);                                /*  解锁读写锁                  */
            errno = EPERM;
            return  (EPERM);
        }
        
        prwlock->PRWLOCK_uiStatus = __PX_RWLOCK_STATUS_READING;         /*  重新进入可读模式            */
        prwlock->PRWLOCK_ulOwner  = LW_OBJECT_HANDLE_INVALID;           /*  没有写拥有者                */
        
    }
    
    prwlock->PRWLOCK_uiOpCounter--;                                     /*  操作数量--                  */
    
    /*
     *  SylixOS 使用写优先原则(其优点详见 www.ibm.com/developerworks/cn/linux/l-rwlock_writing/index.html)
     */
    if (prwlock->PRWLOCK_uiOpCounter) {                                 /*  还有占用的线程              */
        if (prwlock->PRWLOCK_uiWPendCounter == 0) {                     /*  没有等待写的线程            */
            ulReleasNum = (ULONG)prwlock->PRWLOCK_uiRPendCounter;
            if (ulReleasNum) {
                API_SemaphoreCRelease(prwlock->PRWLOCK_ulRSemaphore,
                                      ulReleasNum,
                                      LW_NULL);                         /*  将所有的等待读线程解锁      */
            }
        }
    
    } else {                                                            /*  没有占用锁                  */
        if (prwlock->PRWLOCK_uiWPendCounter) {                          /*  如果有写等待的线程          */
            ulReleasNum = (ULONG)prwlock->PRWLOCK_uiWPendCounter;
            API_SemaphoreCRelease(prwlock->PRWLOCK_ulWSemaphore,
                                  ulReleasNum,
                                  LW_NULL);                             /*  将所有的等待写线程解锁      */
        
        } else if (prwlock->PRWLOCK_uiRPendCounter) {                   /*  如果有等待读的线程          */
            ulReleasNum = (ULONG)prwlock->PRWLOCK_uiRPendCounter;
            API_SemaphoreCRelease(prwlock->PRWLOCK_ulRSemaphore,
                                  ulReleasNum,
                                  LW_NULL);                             /*  将所有的等待读线程解锁      */
        }
    }
    __PX_RWLOCK_UNLOCK(prwlock);                                        /*  解锁读写锁                  */
    
    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
