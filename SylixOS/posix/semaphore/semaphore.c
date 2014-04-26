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
** 文   件   名: semaphore.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 12 月 30 日
**
** 描        述: posix 信号量管理. 

** BUG:
2010.01.14  判断 O_EXCL 需要配合 O_CREAT.
2010.10.06  加入对 cancel type 的操作, 符合 POSIX 标准.
            sem_open() 当创建了信号量后, 系统打印出调试信息.
2011.02.23  加入 EINTR 的检查.
2011.05.30  sem_init() 不再判断 psem 中信号量的有效性.
2012.12.07  加入资源管理器.
2012.12.13  由于 SylixOS 支持进程资源回收, 这里开始支持静态初始化.
2013.04.01  加入创建 mode 的保存, 为未来权限操作提供基础.
*********************************************************************************************************/
#define  __SYLIXOS_STDARG
#define  __SYLIXOS_KERNEL
#include "../include/px_semaphore.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_POSIX_EN > 0
/*********************************************************************************************************
  macro
*********************************************************************************************************/
#define __PX_UNNAME_SEM_NAME        "?psem_unname"
#define __PX_NAMED_SEM_NAME         "psem_named"
/*********************************************************************************************************
  create option (这里加入 LW_OPTION_OBJECT_GLOBAL 是因为 sem 通过原始资源进行回收)
*********************************************************************************************************/
#if LW_CFG_POSIX_INTER_EN > 0
#define __PX_SEM_OPTION             (LW_OPTION_DEFAULT | LW_OPTION_SIGNAL_INTER | LW_OPTION_OBJECT_GLOBAL)
#else
#define __PX_SEM_OPTION             (LW_OPTION_DEFAULT | LW_OPTION_OBJECT_GLOBAL)
#endif                                                                  /*  LW_CFG_POSIX_INTER_EN > 0   */
/*********************************************************************************************************
  internal sem
*********************************************************************************************************/
typedef struct {
    LW_OBJECT_HANDLE        PSEM_ulSemaphore;                           /*  信号量句柄                  */
    mode_t                  PSEM_mode;                                  /*  创建 mode                   */
    __PX_NAME_NODE          PSEM_pxnode;                                /*  名字节点                    */
} __PX_SEM;
/*********************************************************************************************************
** 函数名称: __sem_init_invisible
** 功能描述: posix 信号量隐形创建. (静态初始化)
** 输　入  : psem          信号量句柄 (返回)
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static void  __sem_init_invisible (sem_t  *psem)
{
    if (psem && (psem->SEM_pvPxSem == LW_NULL)) {
        sem_init(psem, 0, 0);
    }
}
/*********************************************************************************************************
** 函数名称: sem_init
** 功能描述: 创建一个匿名的 posix 信号量.
** 输　入  : psem          信号量句柄 (返回)
**           share         是否进程共享
**           value         初始值
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sem_init (sem_t  *psem, int  pshared, unsigned int  value)
{
    __PX_SEM    *pxsem;

    if ((psem == LW_NULL) || (value > SEM_VALUE_MAX)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pxsem = (__PX_SEM *)__SHEAP_ALLOC(sizeof(__PX_SEM) + sizeof(LW_RESOURCE_RAW));
    if (pxsem == LW_NULL) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        errno = ENOMEM;                                                 /*  缺少系统内存                */
        return  (PX_ERROR);
    }
    lib_bzero(&pxsem->PSEM_pxnode, sizeof(__PX_NAME_NODE));
    
    pxsem->PSEM_ulSemaphore = API_SemaphoreCCreate(__PX_UNNAME_SEM_NAME, (ULONG)value, 
                                 __ARCH_ULONG_MAX, __PX_SEM_OPTION, 
                                 LW_NULL);                              /*  创建信号量                  */
    if (pxsem->PSEM_ulSemaphore == LW_OBJECT_HANDLE_INVALID) {
        __SHEAP_FREE(pxsem);
        errno = ENOSPC;
        return  (PX_ERROR);
    }
    
    pxsem->PSEM_mode = 0600;                                            /*  只有本进程可用              */
    
    psem->SEM_pvPxSem = (PVOID)pxsem;
    psem->SEM_presraw = (PLW_RESOURCE_RAW)((CHAR *)pxsem + sizeof(__PX_SEM));
    
    __resAddRawHook(psem->SEM_presraw, (VOIDFUNCPTR)sem_destroy, 
                    psem, 0, 0, 0, 0, 0);                               /*  加入资源管理器              */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: sem_destroy
** 功能描述: 删除一个匿名的 posix 信号量.
** 输　入  : psem          信号量句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sem_destroy (sem_t  *psem)
{
    __PX_SEM    *pxsem;

    if ((psem == LW_NULL) || (psem->SEM_pvPxSem == LW_NULL)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pxsem = (__PX_SEM *)psem->SEM_pvPxSem;
    
    if (pxsem->PSEM_pxnode.PXNODE_pcName != LW_NULL) {                  /*  是否为匿名信号量            */
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    psem->SEM_pvPxSem = LW_NULL;                                        /*  句柄无效                    */
    
    if (API_SemaphoreCDelete(&pxsem->PSEM_ulSemaphore)) {               /*  删除信号量                  */
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    __resDelRawHook(psem->SEM_presraw);
    
    psem->SEM_presraw = LW_NULL;
    
    __SHEAP_FREE(pxsem);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: sem_open
** 功能描述: 打开一个命名的 posix 信号量.
** 输　入  : name          信号量的名字
**           flag          打开选项 (O_CREAT, O_EXCL...)
**           ...
** 输　出  : 信号量句柄
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
sem_t  *sem_open (const char  *name, int  flag, ...)
{
    sem_t              *psem;
    __PX_SEM           *pxsem;
    
    __PX_NAME_NODE     *pxnode;
    va_list             valist;
    size_t              stNameLen;
    
    if (name == LW_NULL) {
        errno = EINVAL;
        return  (SEM_FAILED);
    }
    
    stNameLen = lib_strnlen(name, (NAME_MAX + 1));
    if (stNameLen > NAME_MAX) {
        errno = ENAMETOOLONG;
        return  (SEM_FAILED);
    }
    
    __PX_LOCK();                                                        /*  锁住 posix                  */
    pxnode = __pxnameSeach(name, -1);
    if (pxnode) {                                                       /*  找到                        */
        if ((flag & O_EXCL) && (flag & O_CREAT)) {
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            errno = EEXIST;
            return  (SEM_FAILED);                                       /*  不能同名新建                */
        
        } else {
            pxsem = (__PX_SEM *)pxnode->PXNODE_pvData;                  /*  获得信号量句柄指针          */
            
            if ((pxnode->PXNODE_iType != __PX_NAMED_OBJECT_SEM) ||
                !_ObjectClassOK(pxsem->PSEM_ulSemaphore, 
                                _OBJECT_SEM_C)) {                       /*  是否为计数信号量            */
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                errno = EEXIST;
                return  (SEM_FAILED);
            }
            
            psem = (sem_t *)__SHEAP_ALLOC(sizeof(sem_t) + sizeof(LW_RESOURCE_RAW));
            if (psem == LW_NULL) {
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                errno = ENOMEM;
                return  (SEM_FAILED);
            }
            
            psem->SEM_pvPxSem = pxnode->PXNODE_pvData;                  /*  获得信号量句柄指针          */
            psem->SEM_presraw = (PLW_RESOURCE_RAW)((CHAR *)psem + sizeof(sem_t));
            
            API_AtomicInc(&pxnode->PXNODE_atomic);
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            
            __resAddRawHook(psem->SEM_presraw, (VOIDFUNCPTR)sem_close, 
                            psem, 0, 0, 0, 0, 0);                       /*  加入资源管理器              */
                            
            return  (psem);                                             /*  返回句柄地址                */
        }
    
    } else {                                                            /*  没有找到                    */
        if (flag & O_CREAT) {                                           /*  新建信号量                  */
            mode_t  mode;
            uint_t  value;
            
            va_start(valist, flag);
            mode  = va_arg(valist, mode_t);
            value = va_arg(valist, uint_t);
            va_end(valist);
            
            if (value > SEM_VALUE_MAX) {                                /*  非法值                      */
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                errno = EINVAL;
                return  (SEM_FAILED);
            }
            
            pxsem = (__PX_SEM *)__SHEAP_ALLOC(sizeof(__PX_SEM) + stNameLen + 1);
            if (pxsem == LW_NULL) {
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
                errno = ENOMEM;                                         /*  缺少系统内存                */
                return  (SEM_FAILED);
            }
            
            psem = (sem_t *)__SHEAP_ALLOC(sizeof(sem_t) + sizeof(LW_RESOURCE_RAW));
            if (psem == LW_NULL) {
                __SHEAP_FREE(pxsem);
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
                errno = ENOMEM;                                         /*  缺少系统内存                */
                return  (SEM_FAILED);
            }
            
            pxsem->PSEM_ulSemaphore = API_SemaphoreCCreate(__PX_NAMED_SEM_NAME,
                                        (ULONG)value, 
                                        __ARCH_ULONG_MAX, 
                                        __PX_SEM_OPTION, 
                                        LW_NULL);                       /*  创建信号量                  */
            if (pxsem->PSEM_ulSemaphore == LW_OBJECT_HANDLE_INVALID) {
                __SHEAP_FREE(psem);
                __SHEAP_FREE(pxsem);
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                errno = ENOSPC;
                return  (SEM_FAILED);
            }
            
            pxsem->PSEM_mode = mode;
            
            pxsem->PSEM_pxnode.PXNODE_pcName = (char *)pxsem + sizeof(__PX_SEM);
            pxsem->PSEM_pxnode.PXNODE_iType  = __PX_NAMED_OBJECT_SEM;
            
            lib_strcpy(pxsem->PSEM_pxnode.PXNODE_pcName, name);
            pxsem->PSEM_pxnode.PXNODE_pvData = (void *)pxsem;           /*  保存句柄信息                */
            __pxnameAdd(&pxsem->PSEM_pxnode);                           /*  加入名字节点表              */
            
            psem->SEM_pvPxSem = (void *)pxsem;                          /*  获得信号量句柄指针          */
            psem->SEM_presraw = (PLW_RESOURCE_RAW)((CHAR *)psem + sizeof(sem_t));
            
            API_AtomicInc(&pxsem->PSEM_pxnode.PXNODE_atomic);
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            
            __resAddRawHook(psem->SEM_presraw, (VOIDFUNCPTR)sem_close, 
                            psem, 0, 0, 0, 0, 0);                       /*  加入资源管理器              */
            
            _DebugHandle(__LOGMESSAGE_LEVEL, "posix semaphore \"");
            _DebugHandle(__LOGMESSAGE_LEVEL, name);
            _DebugHandle(__LOGMESSAGE_LEVEL, "\" has been create.\r\n");
            
            return  (psem);                                             /*  返回句柄地址                */
        
        } else {
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            errno = ENOENT;
            return  (SEM_FAILED);
        }
    }
}
/*********************************************************************************************************
** 函数名称: sem_close
** 功能描述: 关闭一个命名的 posix 信号量.
** 输　入  : psem          信号量句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sem_close (sem_t  *psem)
{
    __PX_SEM    *pxsem;

    if ((psem == LW_NULL) || (psem->SEM_pvPxSem == LW_NULL)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pxsem = (__PX_SEM *)psem->SEM_pvPxSem;
    
    if (pxsem->PSEM_pxnode.PXNODE_pcName == LW_NULL) {                  /*  是否为匿名信号量            */
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if (!_ObjectClassOK(pxsem->PSEM_ulSemaphore, _OBJECT_SEM_C)) {      /*  检查是否为计数信号量        */
        errno = EINVAL;
        return  (PX_ERROR);
    }

    __PX_LOCK();                                                        /*  锁住 posix                  */
    if (API_AtomicGet(&pxsem->PSEM_pxnode.PXNODE_atomic) > 0) {
        API_AtomicDec(&pxsem->PSEM_pxnode.PXNODE_atomic);               /*  仅仅减少使用计数            */
    }
    __PX_UNLOCK();                                                      /*  解锁 posix                  */

    __resDelRawHook(psem->SEM_presraw);

    __SHEAP_FREE(psem);                                                 /*  释放句柄                    */

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: sem_unlink
** 功能描述: 删除一个命名的 posix 信号量.
** 输　入  : psem          信号量句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sem_unlink (const char *name)
{
    __PX_NAME_NODE *pxnode;
    __PX_SEM       *pxsem;
    
    if (name == LW_NULL) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    __PX_LOCK();                                                        /*  锁住 posix                  */
    pxnode = __pxnameSeach(name, -1);
    if (pxnode) {
        pxsem = (__PX_SEM *)pxnode->PXNODE_pvData;
        if (!_ObjectClassOK(pxsem->PSEM_ulSemaphore, _OBJECT_SEM_C)) {  /*  检查是否为计数信号量        */
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            errno = EINVAL;
            return  (PX_ERROR);
        }
        if (API_AtomicGet(&pxnode->PXNODE_atomic) > 0) {
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            errno = EBUSY;
            return  (PX_ERROR);
        }
        if (API_SemaphoreDelete(&pxsem->PSEM_ulSemaphore)) {
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            return  (PX_ERROR);                                         /*  errno set by SemaphoreDelete*/
        }
        __pxnameDel(name);
        __SHEAP_FREE(pxsem);                                            /*  释放缓存                    */
        
        __PX_UNLOCK();                                                  /*  解锁 posix                  */
        return  (ERROR_NONE);
    
    } else {
        __PX_UNLOCK();                                                  /*  解锁 posix                  */
        errno = ENOENT;
        return  (PX_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: sem_wait
** 功能描述: 等待一个 posix 信号量.
** 输　入  : psem          信号量句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sem_wait (sem_t  *psem)
{
    __PX_SEM   *pxsem;
    
    __sem_init_invisible(psem);

    if ((psem == LW_NULL) || (psem->SEM_pvPxSem == LW_NULL)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pxsem = (__PX_SEM *)psem->SEM_pvPxSem;
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */

    if (API_SemaphoreCPend(pxsem->PSEM_ulSemaphore, LW_OPTION_WAIT_INFINITE)) {
        if (errno != EINTR) { 
            errno = EINVAL;
        }
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: sem_trywait
** 功能描述: 等待一个 posix 信号量 (不阻塞).
** 输　入  : psem          信号量句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sem_trywait (sem_t  *psem)
{
    ULONG       ulError;
    __PX_SEM   *pxsem;
    
    __sem_init_invisible(psem);
    
    if ((psem == LW_NULL) || (psem->SEM_pvPxSem == LW_NULL)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pxsem = (__PX_SEM *)psem->SEM_pvPxSem;

    ulError = API_SemaphoreCTryPend(pxsem->PSEM_ulSemaphore);
    if (ulError == ERROR_THREAD_WAIT_TIMEOUT) {
        errno = EAGAIN;
        return  (PX_ERROR);
    
    } else if (ulError) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: sem_timedwait
** 功能描述: 等待一个 posix 信号量 (带有超时的阻塞).
** 输　入  : psem          信号量句柄
**           abs_timeout   超时时间 (注意: 这里是绝对时间, 即一个确定的历史时间例如: 2009.12.31 15:36:04)
                           笔者觉得这个不是很爽, 应该再加一个函数可以等待相对时间.
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sem_timedwait (sem_t  *psem, const struct timespec *abs_timeout)
{
    ULONG               ulTimeout;
    ULONG               ulError;
    struct timespec     tvNow;
    struct timespec     tvWait = {0, 0};
    __PX_SEM           *pxsem;
    
    __sem_init_invisible(psem);
    
    if ((psem == LW_NULL) || (psem->SEM_pvPxSem == LW_NULL)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if ((abs_timeout == LW_NULL) || (abs_timeout->tv_nsec >= __TIMEVAL_NSEC_MAX)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pxsem = (__PX_SEM *)psem->SEM_pvPxSem;
    
    lib_clock_gettime(CLOCK_REALTIME, &tvNow);                          /*  获得当前系统时间            */
    if (__timespecLeftTime(&tvNow, abs_timeout)) {
        tvWait = *abs_timeout;
        __timespecSub(&tvWait, &tvNow);                                 /*  计算与当前等待的时间间隔    */
    }
    /*
     *  注意: 当 tvWait 超过ulong tick范围时, 将自然产生溢出, 导致超时时间不准确.
     */
    ulTimeout = __timespecToTick(&tvWait);                              /*  变换为 tick                 */
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    ulError = API_SemaphoreCPend(pxsem->PSEM_ulSemaphore, ulTimeout);   /*  等待信号量                  */
    if (ulError == ERROR_THREAD_WAIT_TIMEOUT) {
        errno = ETIMEDOUT;
        return  (PX_ERROR);
        
    } else if (ulError == EINTR) {
        return  (PX_ERROR);
        
    } else if (ulError) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: sem_reltimedwait_np
** 功能描述: 等待一个 posix 信号量 (带有超时的阻塞).
** 输　入  : psem          信号量句柄
**           rel_timeout   相对超时时间
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if LW_CFG_POSIXEX_EN > 0

LW_API  
int  sem_reltimedwait_np (sem_t  *psem, const struct timespec *rel_timeout)
{
    ULONG               ulTimeout;
    ULONG               ulError;
    __PX_SEM           *pxsem;
    
    __sem_init_invisible(psem);
    
    if ((psem == LW_NULL) || (psem->SEM_pvPxSem == LW_NULL)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if ((rel_timeout == LW_NULL) || (rel_timeout->tv_nsec >= __TIMEVAL_NSEC_MAX)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pxsem = (__PX_SEM *)psem->SEM_pvPxSem;
    
    /*
     *  注意: 当 rel_timeout 超过ulong tick范围时, 将自然产生溢出, 导致超时时间不准确.
     */
    ulTimeout = __timespecToTick(rel_timeout);                          /*  变换为 tick                 */
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    ulError = API_SemaphoreCPend(pxsem->PSEM_ulSemaphore, ulTimeout);   /*  等待信号量                  */
    if (ulError == ERROR_THREAD_WAIT_TIMEOUT) {
        errno = ETIMEDOUT;
        return  (PX_ERROR);
        
    } else if (ulError == EINTR) {
        return  (PX_ERROR);
    
    } else if (ulError) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_POSIXEX_EN > 0       */
/*********************************************************************************************************
** 函数名称: sem_post
** 功能描述: 释放一个 posix 信号量.
** 输　入  : psem          信号量句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sem_post (sem_t  *psem)
{
    __PX_SEM    *pxsem;
    
    __sem_init_invisible(psem);

    if ((psem == LW_NULL) || (psem->SEM_pvPxSem == LW_NULL)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pxsem = (__PX_SEM *)psem->SEM_pvPxSem;

    if (API_SemaphoreCPost(pxsem->PSEM_ulSemaphore)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: sem_getvalue
** 功能描述: 获得一个 posix 信号量当前计数值.
** 输　入  : psem          信号量句柄
**           pivalue       当前计数值
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  sem_getvalue (sem_t  *psem, int  *pivalue)
{
    ULONG       ulValue;
    __PX_SEM   *pxsem;
    
    __sem_init_invisible(psem);
    
    if ((psem == LW_NULL) || (psem->SEM_pvPxSem == LW_NULL)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if (pivalue == LW_NULL) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pxsem = (__PX_SEM *)psem->SEM_pvPxSem;
    
    if (API_SemaphoreCStatus(pxsem->PSEM_ulSemaphore, &ulValue, LW_NULL, LW_NULL)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    *pivalue = (int)ulValue;
    
    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
