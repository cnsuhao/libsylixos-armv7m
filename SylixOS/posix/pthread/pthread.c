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
** 文   件   名: pthread.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 12 月 30 日
**
** 描        述: pthread 兼容库.

** BUG:
2010.01.13  创建线程使用默认属性时, 优先级应使用 POSIX 默认优先级.
2010.05.07  pthread_create() 应在调用 API_ThreadStart() 之前将线程 ID 写入参数地址.
2010.10.04  修正 pthread_create() 处理 attr 为 NULL 时的线程启动参数错误.
            加入线程并行化相关设置.
2010.10.06  加入 pthread_set_name_np() 函数.
2011.06.24  pthread_create() 当没有 attr 时, 使用 POSIX 默认堆栈大小.
2012.06.18  对 posix 线程的优先级操作, 必须要转换优先级为 sylixos 标准. 
            posix 优先级数字越大, 优先级越高, sylixos 刚好相反.
2013.05.01  If successful, the pthread_*() function shall return zero; 
            otherwise, an error number shall be returned to indicate the error.
2013.05.03  pthread_create() 如果存在 attr 并且 attr 指定的堆栈大小为 0, 则继承创建者的堆栈大小.
2013.05.07  加入 pthread_getname_np() 操作.
2013.09.18  pthread_create() 加入对堆栈警戒区的处理.
2013.12.02  加入 pthread_yield().
2014.01.01  加入 pthread_safe_np() 与 pthread_unsafe_np().
2014.07.04  加入 pthread_setaffinity_np 与 pthread_getaffinity_np();
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../include/px_pthread.h"                                      /*  已包含操作系统头文件        */
#include "../include/posixLib.h"                                        /*  posix 内部公共库            */
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_POSIX_EN > 0
/*********************************************************************************************************
** 函数名称: pthread_atfork
** 功能描述: 设置线程在 fork() 时需要执行的函数.
** 输　入  : prepare       prepare 函数指针
**           parent        父进程执行函数指针
**           child         子进程执行函数指针
**           arg           线程入口参数
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_atfork (void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    errno = ENOSYS;
    return  (ENOSYS);
}
/*********************************************************************************************************
** 函数名称: pthread_create
** 功能描述: 创建一个 posix 线程.
** 输　入  : pthread       线程 id (返回).
**           pattr         创建属性
**           start_routine 线程入口
**           arg           线程入口参数
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_create (pthread_t              *pthread, 
                     const pthread_attr_t   *pattr, 
                     void                   *(*start_routine)(void *),
                     void                   *arg)
{
    LW_OBJECT_HANDLE        ulId;
    LW_CLASS_THREADATTR     lwattr;
    PLW_CLASS_TCB           ptcbCur;
    PCHAR                   pcName = "pthread";
    
    if (start_routine == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);
    
    lwattr = API_ThreadAttrGetDefault();                                /*  获得默认线程属性            */
    
    if (pattr) {
        if (!(pattr->PTHREADATTR_ulOption & LW_OPTION_THREAD_STK_CHK)) {/*  属性块没有初始化            */
            errno = EINVAL;
            return  (EINVAL);
        }
        pcName = pattr->PTHREADATTR_pcName;                             /*  使用 attr 作为创建线程名    */
        
        if (pattr->PTHREADATTR_stStackGurad > pattr->PTHREADATTR_stStackByteSize) {
            lwattr.THREADATTR_stGuardSize = LW_CFG_THREAD_DEFAULT_GUARD_SIZE;
        } else {
            lwattr.THREADATTR_stGuardSize = pattr->PTHREADATTR_stStackGurad;
        }
        
        if (pattr->PTHREADATTR_stStackByteSize == 0) {                  /*  继承创建者                  */
            lwattr.THREADATTR_stStackByteSize = ptcbCur->TCB_stStackSize * sizeof(LW_STACK);
        } else {
            lwattr.THREADATTR_pstkLowAddr     = (PLW_STACK)pattr->PTHREADATTR_pvStackAddr;
            lwattr.THREADATTR_stStackByteSize = pattr->PTHREADATTR_stStackByteSize;
        }
        
        if (pattr->PTHREADATTR_iInherit == PTHREAD_INHERIT_SCHED) {     /*  是否继承优先级              */
            lwattr.THREADATTR_ucPriority =  ptcbCur->TCB_ucPriority;
        } else {
            lwattr.THREADATTR_ucPriority = 
                (UINT8)PX_PRIORITY_CONVERT(pattr->PTHREADATTR_schedparam.sched_priority);
        }
        
        lwattr.THREADATTR_ulOption = pattr->PTHREADATTR_ulOption;
    
    } else {                                                            /*  堆栈大小和优先级全部继承    */
        lwattr.THREADATTR_stStackByteSize = ptcbCur->TCB_stStackSize * sizeof(LW_STACK);
        lwattr.THREADATTR_ucPriority  = ptcbCur->TCB_ucPriority;
    }
    
    lwattr.THREADATTR_pvArg = arg;                                      /*  记录参数                    */
    
    /*
     *  初始化线程.
     */
    ulId = API_ThreadInit(pcName, (PTHREAD_START_ROUTINE)start_routine, &lwattr, LW_NULL);
    if (ulId == 0) {
        return  (errno);
    }
    
    if (pattr) {
        UINT8       ucPolicy        = (UINT8)pattr->PTHREADATTR_iSchedPolicy;
        UINT8       ucActivatedMode = LW_OPTION_RESPOND_AUTO;           /*  默认响应方式                */
        
        if (pattr->PTHREADATTR_iInherit == PTHREAD_INHERIT_SCHED) {     /*  继承调度策略                */
            API_ThreadGetSchedParam(ptcbCur->TCB_ulId, &ucPolicy, &ucActivatedMode);
        }
        
        API_ThreadSetSchedParam(ulId, ucPolicy, ucActivatedMode);       /*  设置调度策略                */
    }
    
    if (pthread) {
        *pthread = ulId;                                                /*  保存线程句柄                */
    }
    
    API_ThreadStart(ulId);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_cancel
** 功能描述: cancel 一个 posix 线程.
** 输　入  : thread       线程 id.
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_cancel (pthread_t  thread)
{
    PX_ID_VERIFY(thread, pthread_t);
    
    if (API_ThreadCancel(&thread)) {
        return  (errno);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_testcancel
** 功能描述: testcancel 当前 posix 线程.
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
void  pthread_testcancel (void)
{
    API_ThreadTestCancel();
}
/*********************************************************************************************************
** 函数名称: pthread_join
** 功能描述: join 一个 posix 线程.
** 输　入  : thread       线程 id.
**           ppstatus     获取线程返回值
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_join (pthread_t  thread, void **ppstatus)
{
    if (API_ThreadJoin(thread, ppstatus)) {
        return  (errno);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_detach
** 功能描述: detach 一个 posix 线程.
** 输　入  : thread       线程 id.
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_detach (pthread_t  thread)
{
    PX_ID_VERIFY(thread, pthread_t);
    
    if (API_ThreadDetach(thread)) {
        return  (errno);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_equal
** 功能描述: 判断两个 posix 线程是否相同.
** 输　入  : thread1       线程 id.
**           thread2       线程 id.
** 输　出  : true or false
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_equal (pthread_t  thread1, pthread_t  thread2)
{
    PX_ID_VERIFY(thread1, pthread_t);
    PX_ID_VERIFY(thread2, pthread_t);

    return  (thread1 == thread2);
}
/*********************************************************************************************************
** 函数名称: pthread_exit
** 功能描述: 删除当前 posix 线程.
** 输　入  : status        exit 代码
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
void  pthread_exit (void *status)
{
    LW_OBJECT_HANDLE    ulId = API_ThreadIdSelf();

    API_ThreadDelete(&ulId, status);
}
/*********************************************************************************************************
** 函数名称: pthread_self
** 功能描述: 获得当前 posix 线程句柄.
** 输　入  : NONE
** 输　出  : 句柄
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
pthread_t  pthread_self (void)
{
    return  (API_ThreadIdSelf());
}
/*********************************************************************************************************
** 函数名称: pthread_yield
** 功能描述: 将当前任务插入到同优先级调度器链表的最后, 主动让出一次 CPU 调度.
** 输　入  : NONE
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_yield (void)
{
    API_ThreadYield(API_ThreadIdSelf());
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_kill
** 功能描述: 向指定 posix 线程发送一个信号.
** 输　入  : thread        线程句柄
**           signo         信号
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_kill (pthread_t  thread, int signo)
{
    int  error;

    PX_ID_VERIFY(thread, pthread_t);
    
    error = kill(thread, signo);
    if (error < ERROR_NONE) {
        return  (errno);
    } else {
        return  (error);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_sigmask
** 功能描述: 修改 posix 线程信号掩码.
** 输　入  : how           方法
**           newmask       新的信号掩码
**           oldmask       旧的信号掩码
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_sigmask (int  how, const sigset_t  *newmask, sigset_t *oldmask)
{
    int  error;
    
    error = sigprocmask(how, newmask, oldmask);
    if (error < ERROR_NONE) {
        return  (errno);
    } else {
        return  (error);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_cleanup_pop
** 功能描述: 将一个压栈函数运行并释放
** 输　入  : iNeedRun          是否执行
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
void  pthread_cleanup_pop (int  iNeedRun)
{
    API_ThreadCleanupPop((BOOL)iNeedRun);
}
/*********************************************************************************************************
** 函数名称: pthread_cleanup_push
** 功能描述: 将一个清除函数压入函数堆栈
** 输　入  : pfunc         函数指针
**           arg           函数参数
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
void  pthread_cleanup_push (void (*pfunc)(void *), void *arg)
{
    API_ThreadCleanupPush(pfunc, arg);
}
/*********************************************************************************************************
** 函数名称: pthread_getschedparam
** 功能描述: 获得调度器参数
** 输　入  : thread        线程句柄
**           piPolicy      调度策略(返回)
**           pschedparam   调度器参数(返回)
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_getschedparam (pthread_t            thread, 
                            int                 *piPolicy, 
                            struct sched_param  *pschedparam)
{
    UINT8       ucPriority;
    UINT8       ucPolicy;
    
    PX_ID_VERIFY(thread, pthread_t);
    
    if (pschedparam) {
        if (API_ThreadGetPriority(thread, &ucPriority)) {
            errno = ESRCH;
            return  (ESRCH);
        }
        pschedparam->sched_priority = PX_PRIORITY_CONVERT(ucPriority);
    }
    
    if (piPolicy) {
        if (API_ThreadGetSchedParam(thread, &ucPolicy, LW_NULL)) {
            errno = ESRCH;
            return  (ESRCH);
        }
        *piPolicy = (int)ucPolicy;
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_getschedparam
** 功能描述: 设置调度器参数
** 输　入  : thread        线程句柄
**           iPolicy       调度策略
**           pschedparam   调度器参数
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_setschedparam (pthread_t                  thread, 
                            int                        iPolicy, 
                            const struct sched_param  *pschedparam)
{
    UINT8       ucPriority;
    UINT8       ucActivatedMode;

    if (pschedparam == LW_NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    if ((iPolicy != LW_OPTION_SCHED_FIFO) &&
        (iPolicy != LW_OPTION_SCHED_RR)) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    if ((pschedparam->sched_priority < __PX_PRIORITY_MIN) ||
        (pschedparam->sched_priority > __PX_PRIORITY_MAX)) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    ucPriority= (UINT8)PX_PRIORITY_CONVERT(pschedparam->sched_priority);
    
    PX_ID_VERIFY(thread, pid_t);
    
    if (API_ThreadGetSchedParam(thread,
                                LW_NULL,
                                &ucActivatedMode)) {
        errno = ESRCH;
        return  (ESRCH);
    }
    
    API_ThreadSetSchedParam(thread, (UINT8)iPolicy, ucActivatedMode);
    
    if (API_ThreadSetPriority(thread, ucPriority)) {
        errno = ESRCH;
        return  (ESRCH);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_onec
** 功能描述: 线程安全的仅执行一次指定函数
** 输　入  : thread        线程句柄
**           once          onec_t参数
**           pfunc         函数指针
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_once (pthread_once_t  *once, void (*pfunc)(void))
{
    int  error;
    
    error = API_ThreadOnce(once, pfunc);
    if (error < ERROR_NONE) {
        return  (errno);
    } else {
        return  (error);
    }
}
/*********************************************************************************************************
** 函数名称: pthread_setschedprio
** 功能描述: 设置线程优先级
** 输　入  : thread        线程句柄
**           prio          优先级
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_setschedprio (pthread_t  thread, int  prio)
{
    UINT8       ucPriority;

    if ((prio < __PX_PRIORITY_MIN) ||
        (prio > __PX_PRIORITY_MAX)) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    PX_ID_VERIFY(thread, pthread_t);
    
    ucPriority= (UINT8)PX_PRIORITY_CONVERT(prio);
    
    if (API_ThreadSetPriority(thread, ucPriority)) {
        errno = ESRCH;
        return  (ESRCH);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_getschedprio
** 功能描述: 获得线程优先级
** 输　入  : thread        线程句柄
**           prio          优先级(返回)
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_getschedprio (pthread_t  thread, int  *prio)
{
    UINT8       ucPriority;

    PX_ID_VERIFY(thread, pthread_t);

    if (prio) {
        if (API_ThreadGetPriority(thread, &ucPriority)) {
            errno = ESRCH;
            return  (ESRCH);
        }
        *prio = PX_PRIORITY_CONVERT(ucPriority);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_setcancelstate
** 功能描述: 设置取消线程是否使能
** 输　入  : newstate      新的状态
**           poldstate     先前的状态(返回)
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_setcancelstate (int  newstate, int  *poldstate)
{
    ULONG   ulError = API_ThreadSetCancelState(newstate, poldstate);
    
    if (ulError) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_setcanceltype
** 功能描述: 设置当前线程被动取消时的动作
** 输　入  : newtype      新的动作
**           poldtype     先前的动作(返回)
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_setcanceltype (int  newtype, int  *poldtype)
{
    ULONG   ulError = API_ThreadSetCancelType(newtype, poldtype);
    
    if (ulError) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_setconcurrency
** 功能描述: 设置线程新的并行级别
** 输　入  : newlevel      新并行级别
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_setconcurrency (int  newlevel)
{
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_getconcurrency
** 功能描述: 获得当前线程的并行级别
** 输　入  : NONE
** 输　出  : 当前线程的并行级别
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_getconcurrency (void)
{
    return  (LW_CFG_MAX_THREADS);
}
/*********************************************************************************************************
** 函数名称: pthread_setname_np
** 功能描述: 设置线程名字
** 输　入  : thread    线程句柄
**           name      线程名字
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if LW_CFG_POSIXEX_EN > 0

LW_API 
int  pthread_setname_np (pthread_t  thread, const char  *name)
{
    ULONG   ulError;
    
    PX_ID_VERIFY(thread, pthread_t);

    ulError = API_ThreadSetName(thread, name);
    if (ulError == ERROR_KERNEL_PNAME_TOO_LONG) {
        errno = ERANGE;
        return  (ERANGE);
    
    } else if (ulError) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_getname_np
** 功能描述: 获得线程名字
** 输　入  : thread    线程句柄
**           name      线程名字
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_getname_np (pthread_t  thread, char  *name, size_t len)
{
    ULONG   ulError;
    CHAR    cNameBuffer[LW_CFG_OBJECT_NAME_SIZE];
    
    PX_ID_VERIFY(thread, pthread_t);
    
    ulError = API_ThreadGetName(thread, cNameBuffer);
    if (ulError) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    if (lib_strlen(cNameBuffer) >= len) {
        errno = ERANGE;
        return  (ERANGE);
    }
    
    lib_strcpy(name, cNameBuffer);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_safe_np
** 功能描述: 线程进入安全模式, 任何对本线程的删除操作都会推迟到线程退出安全模式时进行.
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
void  pthread_safe_np (void)
{
    API_ThreadSafe();
}
/*********************************************************************************************************
** 函数名称: pthread_unsafe_np
** 功能描述: 线程退出安全模式.
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
void  pthread_unsafe_np (void)
{
    API_ThreadUnsafe();
}
/*********************************************************************************************************
** 函数名称: pthread_int_lock_np
** 功能描述: 线程锁定当前所在核中断, 不再响应中断.
** 输　入  : irqctx        体系结构相关中断状态保存结构 (用户不需要关心具体内容)
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_int_lock_np (pthread_int_t *irqctx)
{
    if (!irqctx) {
        errno = EINVAL;
        return  (EINVAL);
    }
    
    *irqctx = KN_INT_DISABLE();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_int_unlock_np
** 功能描述: 线程解锁当前所在核中断, 开始响应中断.
** 输　入  : irqctx        体系结构相关中断状态保存结构 (用户不需要关心具体内容)
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_int_unlock_np (pthread_int_t irqctx)
{
    KN_INT_ENABLE(irqctx);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: pthread_setaffinity_np
** 功能描述: 设置进程调度的 CPU 集合
** 输　入  : pid           进程 / 线程 ID
**           setsize       CPU 集合大小
**           set           CPU 集合
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_setaffinity_np (pthread_t  thread, size_t setsize, const cpu_set_t *set)
{
    _ErrorHandle(ENOSYS);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: pthread_getaffinity_np
** 功能描述: 获取进程调度的 CPU 集合
** 输　入  : pid           进程 / 线程 ID
**           setsize       CPU 集合大小
**           set           CPU 集合
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  pthread_getaffinity_np (pthread_t  thread, size_t setsize, cpu_set_t *set)
{
    ULONG   i;
    
    if (!set) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    for (i = 0; (i < setsize) && (i < LW_NCPUS); i++) {
        CPU_SET(i, set);
    }
    
    return  (ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_POSIXEX_EN > 0       */
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
