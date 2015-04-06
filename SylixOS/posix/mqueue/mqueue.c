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
** 文   件   名: mqueue.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 12 月 30 日
**
** 描        述: posix 消息队列管理. 

** BUG:
2010.01.13  使用计数信号量控制读写同步.
2010.01.14  判断 O_EXCL 需要配合 O_CREAT.
2010.10.06  加入对 cancel type 的操作, 符合 POSIX 标准.
            mq_open() 当创建了消息队列后, 系统打印出调试信息.
2011.02.17  修改注释.
2011.02.22  mq_notify() 加入 _sigPendInit() 初始化.
2011.02.23  加入 EINTR 的检查.
2011.02.26  使用 sigevent 作为发送信号参数.
2012.12.07  将 mqueue 加入资源管理器.
2013.03.18  改进 mqueue 的原子操作性.
2013.04.01  加入创建 mode 的保存, 为未来权限操作提供基础.
2013.12.12  使用 archFindLsb() 来确定消息优先级.
2014.05.30  使用 ROUND_UP 代替除法.
*********************************************************************************************************/
#define  __SYLIXOS_STDARG
#define  __SYLIXOS_KERNEL
#include "../include/px_mqueue.h"
#include "../include/posixLib.h"                                        /*  posix 内部公共库            */
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_POSIX_EN > 0
/*********************************************************************************************************
  注意: message queue 不同于信号量, 信号量 __PX_NAME_NODE 中直接保存的是信号量句柄, 因为 open 时的 flag 对
        正常的信号量操作没有任何影响, (用户句柄既是信号量控制块!)
        
        但是 message queue 有区别! open 时的 flag 会影响之后的操作, 所以, 在 message queue 中, 将用户句柄
        mqt_t 和消息队列控制块分离, mqt_t 独立保存了 flag 也就是, 不同的句柄可以指向同一个消息队列, 即:
        操作时的 flag 可以不同! 
        
        以上的区别造成了 message queue 和 semaphore 对于句柄和控制块的管理方法略有不同.
*********************************************************************************************************/
/*********************************************************************************************************
  create option (这里加入 LW_OPTION_OBJECT_GLOBAL 因为 mqueue 的回收通过原始资源进行回收)
*********************************************************************************************************/
#if LW_CFG_POSIX_INTER_EN > 0
#define __PX_MQUEUE_OPTION          (LW_OPTION_DEFAULT | LW_OPTION_SIGNAL_INTER | LW_OPTION_OBJECT_GLOBAL)
#else
#define __PX_MQUEUE_OPTION          (LW_OPTION_DEFAULT | LW_OPTION_OBJECT_GLOBAL)
#endif                                                                  /*  LW_CFG_POSIX_INTER_EN > 0   */
/*********************************************************************************************************
  一个消息节点
*********************************************************************************************************/

typedef struct {
    LW_LIST_RING        PMSGN_ringManage;                               /*  消息环形缓冲                */
    size_t              PMSGN_stMsgLen;                                 /*  消息长度                    */
} __PX_MSG_NODE;

/*********************************************************************************************************
  消息缓冲内存控制块
*********************************************************************************************************/

typedef struct {
    caddr_t             PMSGM_pcMem;                                    /*  消息缓冲内存                */
    size_t              PMSGM_stBytesPerMsg;                            /*  每一个消息缓冲所占的字节数  */
    LW_LIST_RING_HEADER PMSGM_pringFreeList;                            /*  空闲消息内存                */
} __PX_MSG_MEM;

/*********************************************************************************************************
  消息队列 notify 
*********************************************************************************************************/

typedef struct {
    LW_OBJECT_HANDLE    PMSGNTF_ulThreadId;                             /*  线程 id                     */
    struct sigevent     PMSGNTF_sigevent;                               /*  notify 参数                 */
} __PX_MSG_NOTIFY;

/*********************************************************************************************************
  消息队列控制块
*********************************************************************************************************/

typedef struct {
    mq_attr_t           PMSG_mqattr;                                    /*  消息队列属性                */
    mode_t              PMSG_mode;                                      /*  创建 mode                   */
    LW_OBJECT_HANDLE    PMSG_ulReadSync;                                /*  读同步                      */
    LW_OBJECT_HANDLE    PMSG_ulWriteSync;                               /*  写同步                      */
    LW_OBJECT_HANDLE    PMSG_ulMutex;                                   /*  互斥量                      */
    __PX_MSG_NOTIFY     PMSG_pmsgntf;                                   /*  notify 信号相关             */
    __PX_MSG_MEM        PMSG_pmsgmem;                                   /*  消息队列内存缓存            */
    uint32_t            PMSG_u32Map;                                    /*  位图表 (与优先级算法相同)   */
    LW_LIST_RING_HEADER PMSG_pringMsg[MQ_PRIO_MAX];                     /*  每个优先级的消息队列        */
    __PX_NAME_NODE      PMSG_pxnode;                                    /*  名字节点                    */
} __PX_MSG;

/*********************************************************************************************************
  mqueue file
*********************************************************************************************************/

typedef struct {
    __PX_MSG           *PMSGF_pmg;                                      /*  消息队列                    */
    int                 PMSGF_iFlag;                                    /*  mq_open() 时的 flag         */
    LW_RESOURCE_RAW     PMSGF_resraw;                                   /*  资源管理节点                */
} __PX_MSG_FILE;

/*********************************************************************************************************
  参数
*********************************************************************************************************/
mq_attr_t  mq_attr_default = {O_RDWR, 128, 64, 0};

#define __PX_MQ_LOCK(pmq)           API_SemaphoreMPend(pmq->PMSG_ulMutex, LW_OPTION_WAIT_INFINITE)
#define __PX_MQ_UNLOCK(pmq)         API_SemaphoreMPost(pmq->PMSG_ulMutex)

/*********************************************************************************************************
  读写同步
*********************************************************************************************************/

#define __PX_MQ_RWAIT(pmq, to)      API_SemaphoreBPend(pmq->PMSG_ulReadSync, to)
#define __PX_MQ_RPOST(pmq)          API_SemaphoreBPost(pmq->PMSG_ulReadSync)

#define __PX_MQ_WWAIT(pmq, to)      API_SemaphoreBPend(pmq->PMSG_ulWriteSync, to)
#define __PX_MQ_WPOST(pmq)          API_SemaphoreBPost(pmq->PMSG_ulWriteSync)

/*********************************************************************************************************
** 函数名称: __mqueueSignalNotify
** 功能描述: 消息队列发送一个信号通知可读.
** 输　入  : pmq           消息队列控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __mqueueSignalNotify (__PX_MSG  *pmq)
{
    if (pmq->PMSG_pmsgntf.PMSGNTF_ulThreadId == LW_OBJECT_HANDLE_INVALID) {
        return;                                                         /*  没有任务接收                */
    }
    
    _doSigEvent(pmq->PMSG_pmsgntf.PMSGNTF_ulThreadId,
                &pmq->PMSG_pmsgntf.PMSGNTF_sigevent, SI_MESGQ);         /*  发送 sigevent               */
}
/*********************************************************************************************************
** 函数名称: __mqueueSeekPrio
** 功能描述: 确定消息队列中最高优先级消息的优先级.
** 输　入  : pmq           消息队列控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static uint_t  __mqueueSeekPrio (__PX_MSG  *pmq)
{
    REGISTER uint32_t    u32Map = pmq->PMSG_u32Map;

    archAssert(u32Map, __func__, __FILE__, __LINE__);                   /*  这里 u32Map 绝对不为 0      */

    return  ((uint_t)archFindLsb(u32Map) - 1);
}
/*********************************************************************************************************
** 函数名称: __mqueueSend
** 功能描述: 发送一个消息.
** 输　入  : pmq           消息队列控制块
**           msg           消息指针
**           msgsize       消息大小
**           uiRealPrio    转换后的优先级
**           ulTimeout     超时时间
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __mqueueSend (__PX_MSG  *pmq, const char  *msg, size_t  msglen, 
                          unsigned uiRealPrio, ULONG  ulTimeout)
{
    ULONG               ulError;
    __PX_MSG_NODE      *pmqn;
    PLW_LIST_RING       pringMsg;
    caddr_t             pcBuffer;                                       /*  消息缓冲                    */
    ULONG               ulOrgKernelTime;
    
    /*
     *  获取发送缓冲并填入数据
     */
    do {
        __KERNEL_TIME_GET(ulOrgKernelTime, ULONG);                      /*  记录系统时间                */
        
        ulError = __PX_MQ_WWAIT(pmq, ulTimeout);                        /*  等待写同步                  */
        if (ulError == ERROR_THREAD_WAIT_TIMEOUT) {
            errno = ETIMEDOUT;
            return  (PX_ERROR);
        
        } else if (ulError == EINTR) {
            errno = EINTR;
            return  (PX_ERROR);
        
        } else if (ulError) {
            return  (PX_ERROR);
        }
        
        if (__PX_MQ_LOCK(pmq)) {                                        /*  锁定消息队列                */
            errno = ENOENT;
            return  (PX_ERROR);
        }
        
        if (pmq->PMSG_pmsgmem.PMSGM_pringFreeList) {                    /*  可以发送                    */
            break;
        }
        __PX_MQ_UNLOCK(pmq);                                            /*  解锁消息队列                */
    
        ulTimeout = _sigTimeOutRecalc(ulOrgKernelTime, ulTimeout);      /*  重新计算等待时间            */
    } while (1);
    
    pringMsg = pmq->PMSG_pmsgmem.PMSGM_pringFreeList;
    pmqn     = (__PX_MSG_NODE *)pringMsg;
    pcBuffer = ((char *)pmqn + sizeof(__PX_MSG_NODE));                  /*  定位消息缓冲                */
    
    _List_Ring_Del(&pmqn->PMSGN_ringManage,
                   &pmq->PMSG_pmsgmem.PMSGM_pringFreeList);             /*  从空闲队列中删除            */
                   
    lib_memcpy(pcBuffer, msg, msglen);                                  /*  保存消息                    */
    pmqn->PMSGN_stMsgLen = msglen;                                      /*  记录消息长度                */
    
    /*
     *  处理消息及位图
     */
    _List_Ring_Add_Last(&pmqn->PMSGN_ringManage,
                        &pmq->PMSG_pringMsg[uiRealPrio]);               /*  加入消息散列表              */
    pmq->PMSG_u32Map |= (1 << uiRealPrio);                              /*  位图表处理                  */
    
    /*
     *  更新消息队列状态
     */
    pmq->PMSG_mqattr.mq_curmsgs++;                                      /*  缓存的消息数量++            */
    __PX_MQ_RPOST(pmq);                                                 /*  消息队列可读                */
    
    if (pmq->PMSG_mqattr.mq_curmsgs == 1) {                             /*  信号异步通知可读            */
        __mqueueSignalNotify(pmq);
    }
    
    if (pmq->PMSG_pmsgmem.PMSGM_pringFreeList) {
        __PX_MQ_WPOST(pmq);                                             /*  可写                        */
    }
    __PX_MQ_UNLOCK(pmq);                                                /*  解锁消息队列                */

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __mqueueRecv
** 功能描述: 接收一个消息.
** 输　入  : pmq           消息队列控制块
**           msg           消息指针
**           msglen        缓冲大小
**           pmsgprio      接收的消息优先级 (返回)
**           ulTimeout     超时时间
** 输　出  : 接收消息的大小
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static ssize_t  __mqueueRecv (__PX_MSG  *pmq, char  *msg, size_t  msglen, 
                              unsigned *pmsgprio, ULONG  ulTimeout)
{
    __PX_MSG_NODE      *pmqn;
    PLW_LIST_RING       pringMsg;
    caddr_t             pcBuffer;                                       /*  消息缓冲                    */
    
    uint_t              uiRealPrio;
    ULONG               ulError;
    ULONG               ulOrgKernelTime;

    /*
     *  开始接收消息
     */
    do {
        __KERNEL_TIME_GET(ulOrgKernelTime, ULONG);                      /*  记录系统时间                */
        
        ulError = __PX_MQ_RWAIT(pmq, ulTimeout);                        /*  等待读同步                  */
        if (ulError == ERROR_THREAD_WAIT_TIMEOUT) {
            errno = ETIMEDOUT;
            return  (PX_ERROR);
        
        } else if (ulError == EINTR) {
            errno = EINTR;
            return  (PX_ERROR);
        
        } else if (ulError) {
            return  (PX_ERROR);
        }
        
        if (__PX_MQ_LOCK(pmq)) {                                        /*  锁定消息队列                */
            errno = ENOENT;
            return  (PX_ERROR);
        }
    
        if (pmq->PMSG_mqattr.mq_curmsgs) {                              /*  有数据可读                  */
            break;
        }
        __PX_MQ_UNLOCK(pmq);                                            /*  解锁消息队列                */
    
        ulTimeout = _sigTimeOutRecalc(ulOrgKernelTime, ulTimeout);      /*  重新计算等待时间            */
    } while (1);
    
    uiRealPrio = __mqueueSeekPrio(pmq);                                 /*  确定优先级                  */
    pringMsg   = pmq->PMSG_pringMsg[uiRealPrio];
    pmqn       = (__PX_MSG_NODE *)pringMsg;
    pcBuffer   = ((char *)pmqn + sizeof(__PX_MSG_NODE));                /*  定位消息缓冲                */
    
    if (pmqn->PMSGN_stMsgLen > msglen) {                                /*  消息太长无法接收            */
        __PX_MQ_RPOST(pmq);
        errno = EMSGSIZE;
        __PX_MQ_UNLOCK(pmq);                                            /*  解锁消息队列                */
        return  (PX_ERROR);
    }
    
    /*
     *  处理消息及位图
     */
    _List_Ring_Del(&pmqn->PMSGN_ringManage,
                   &pmq->PMSG_pringMsg[uiRealPrio]);                    /*  从消息散列表中删除          */
    if (pmq->PMSG_pringMsg[uiRealPrio] == LW_NULL) {                    /*  此优先级已经没有消息        */
        pmq->PMSG_u32Map &= ~(1 << uiRealPrio);                         /*  位图表处理                  */
    }
    
    if (pmsgprio) {
        *pmsgprio = MQ_PRIO_MAX - uiRealPrio;                           /*  回写消息优先级              */
    }
    
    /*
     *  保存消息
     */
    msglen = pmqn->PMSGN_stMsgLen;                                      /*  记录消息长度                */
    lib_memcpy(msg, pcBuffer, msglen);                                  /*  保存消息                    */
    
    _List_Ring_Add_Last(&pmqn->PMSGN_ringManage,
                        &pmq->PMSG_pmsgmem.PMSGM_pringFreeList);        /*  重新加入空闲队列            */
                        
    /*
     *  更新消息队列状态
     */
    pmq->PMSG_mqattr.mq_curmsgs--;                                      /*  缓存的消息数量++            */
    __PX_MQ_WPOST(pmq);                                                 /*  还有空间可以发送消息        */
    
    if (pmq->PMSG_mqattr.mq_curmsgs) {
        __PX_MQ_RPOST(pmq);                                             /*  还可以读取消息              */
    }
    __PX_MQ_UNLOCK(pmq);                                                /*  解锁消息队列                */
    
    return  ((ssize_t)msglen);
}
/*********************************************************************************************************
** 函数名称: __mqueueInitBuffer
** 功能描述: 初始化消息队列的 buffer.
** 输　入  : pmq           消息队列控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __mqueueInitBuffer (__PX_MSG  *pmq)
{
    ULONG                i;
    __PX_MSG_NODE       *pmsgnode = (__PX_MSG_NODE *)pmq->PMSG_pmsgmem.PMSGM_pcMem;
                                                                        /*  获得消息缓冲首地址          */
    
    for (i = 0; i < pmq->PMSG_mqattr.mq_maxmsg; i++) {                  /*  将所有消息节点插入空闲链表  */
        _List_Ring_Add_Last(&pmsgnode->PMSGN_ringManage,
                            &pmq->PMSG_pmsgmem.PMSGM_pringFreeList);
        pmsgnode = (__PX_MSG_NODE *)((char *)pmsgnode + pmq->PMSG_pmsgmem.PMSGM_stBytesPerMsg);
    }
}
/*********************************************************************************************************
** 函数名称: __mqueueCreate
** 功能描述: 创建一个消息队列.
** 输　入  : name          消息队列名
**           mode          创建mode
**           pmqattr       消息队列属性
** 输　出  : 消息队列控制块
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static __PX_MSG  *__mqueueCreate (const char  *name, mode_t mode, struct mq_attr  *pmqattr)
{
    INT        iErrLevel = 0;
    size_t     stMemBufferSize;                                         /*  需要的缓存大小              */
    size_t     stLen = lib_strlen(name);
    __PX_MSG  *pmq;
    
    /*
     *  创建控制块内存
     */
    pmq = (__PX_MSG *)__SHEAP_ALLOC(sizeof(__PX_MSG) + stLen + 1);
    if (pmq == LW_NULL) {
        errno = ENOMEM;
        return  (LW_NULL);
    }
    lib_bzero(pmq, sizeof(__PX_MSG));                                   /*  所有内容全部清零            */
    
    pmq->PMSG_pxnode.PXNODE_pcName = (char *)pmq + sizeof(__PX_MSG);    /*  确定名字缓冲的位置          */
    pmq->PMSG_pxnode.PXNODE_pvData = (void *)pmq;                       /*  记录控制块地址              */
    pmq->PMSG_pxnode.PXNODE_iType  = __PX_NAMED_OBJECT_MQ;              /*  消息队列                    */
    lib_strcpy(pmq->PMSG_pxnode.PXNODE_pcName, name);                   /*  保存消息队列名              */
    
    pmq->PMSG_mode = mode;                                              /*  创建 mode                   */
    
    /*
     *  计算每一条消息占用的字节数
     */
    stMemBufferSize = ((size_t)pmqattr->mq_msgsize 
                    + sizeof(__PX_MSG_NODE));                           /*  一个消息节点的大小          */
    stMemBufferSize = ROUND_UP(stMemBufferSize, sizeof(LW_STACK));      /*  对齐大小处理                */
    
    pmq->PMSG_pmsgmem.PMSGM_stBytesPerMsg = stMemBufferSize;            /*  每一条消息占用的字节数      */
    
    /*
     *  创建消息缓冲内存
     */
    stMemBufferSize *= (size_t)pmqattr->mq_maxmsg;                      /*  所有消息节点总共需要的大小  */
    pmq->PMSG_pmsgmem.PMSGM_pcMem = (caddr_t)__SHEAP_ALLOC(stMemBufferSize);
    if (pmq->PMSG_pmsgmem.PMSGM_pcMem == LW_NULL) {
        iErrLevel = 1;
        errno     = ENOMEM;
        goto    __error_handle;
    }
    pmq->PMSG_mqattr = *pmqattr;                                        /*  保存属性块                  */
    pmq->PMSG_mqattr.mq_curmsgs = 0;                                    /*  目前没有任何有效消息        */
    
    /*
     *  初始化信号量
     */
    pmq->PMSG_ulReadSync = API_SemaphoreBCreate("pxmq_rdsync", LW_FALSE, 
                                                __PX_MQUEUE_OPTION, 
                                                LW_NULL);               /*  初始化为不可读              */
    if (pmq->PMSG_ulReadSync == LW_OBJECT_HANDLE_INVALID) {
        iErrLevel = 2;
        errno     = ENOSPC;
        goto    __error_handle;
    }
    
    pmq->PMSG_ulWriteSync = API_SemaphoreBCreate("pxmq_wrsync", LW_TRUE,
                                                 __PX_MQUEUE_OPTION, 
                                                 LW_NULL);              /*  初始化为可写                */
    if (pmq->PMSG_ulWriteSync == LW_OBJECT_HANDLE_INVALID) {
        iErrLevel = 3;
        errno     = ENOSPC;
        goto    __error_handle;
    }
    
    pmq->PMSG_ulMutex = API_SemaphoreMCreate("pxmq_mutex", LW_PRIO_DEF_CEILING, 
                                             LW_OPTION_INHERIT_PRIORITY | 
                                             LW_OPTION_DELETE_SAFE |
                                             LW_OPTION_OBJECT_GLOBAL, LW_NULL);
    if (pmq->PMSG_ulMutex == LW_OBJECT_HANDLE_INVALID) {
        iErrLevel = 4;
        errno     = ENOSPC;
        goto    __error_handle;
    }
    
    /*
     *  将所有的消息节点全部存入空闲队列
     */
    __mqueueInitBuffer(pmq);
    
    return  (pmq);
    
__error_handle:
    if (iErrLevel > 3) {
        API_SemaphoreBDelete(&pmq->PMSG_ulWriteSync);
    }
    if (iErrLevel > 2) {
        API_SemaphoreBDelete(&pmq->PMSG_ulReadSync);
    }
    if (iErrLevel > 1) {
        __SHEAP_FREE(pmq->PMSG_pmsgmem.PMSGM_pcMem);
    }
    if (iErrLevel > 0) {
        __SHEAP_FREE(pmq);
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __mqueueDelete
** 功能描述: 删除一个消息队列.
** 输　入  : pmq           消息队列控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __mqueueDelete (__PX_MSG  *pmq)
{
    API_SemaphoreMDelete(&pmq->PMSG_ulMutex);
    API_SemaphoreBDelete(&pmq->PMSG_ulWriteSync);
    API_SemaphoreBDelete(&pmq->PMSG_ulReadSync);
    __SHEAP_FREE(pmq->PMSG_pmsgmem.PMSGM_pcMem);
    __SHEAP_FREE(pmq);
}
/*********************************************************************************************************
** 函数名称: mq_open
** 功能描述: 打开一个命名的 posix 消息队列.
** 输　入  : name          信号量的名字
**           flag          打开选项 (O_NONBLOCK, O_CREAT, O_EXCL, O_RDONLY, O_WRONLY, O_RDWR...)
**           ...
** 输　出  : 消息队列句柄
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
mqd_t  mq_open (const char  *name, int  flag, ...)
{
    __PX_MSG           *pmq;
    __PX_NAME_NODE     *pxnode;
    __PX_MSG_FILE      *pmqfile;
    
    va_list             valist;
    size_t              stNameLen;

    if (name == LW_NULL) {
        errno = EINVAL;
        return  (MQ_FAILED);
    }
    
    stNameLen = lib_strnlen(name, (NAME_MAX + 1));
    if (stNameLen > NAME_MAX) {
        errno = ENAMETOOLONG;
        return  (MQ_FAILED);
    }
    
    __PX_LOCK();                                                        /*  锁住 posix                  */
    pxnode = __pxnameSeach(name, -1);
    if (pxnode) {                                                       /*  找到                        */
        if ((flag & O_EXCL) && (flag & O_CREAT)) {
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            errno = EEXIST;
            return  (MQ_FAILED);                                        /*  不能同名新建                */
        
        } else {
            if (pxnode->PXNODE_iType != __PX_NAMED_OBJECT_MQ) {         /*  检查类型                    */
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                errno = EEXIST;
                return  (MQ_FAILED);
            }
            
            pmqfile = (__PX_MSG_FILE *)__SHEAP_ALLOC(sizeof(__PX_MSG_FILE));
            if (pmqfile == LW_NULL) {
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                errno = ENOMEM;
                return  (MQ_FAILED);
            }
            pmqfile->PMSGF_pmg   = (__PX_MSG *)pxnode->PXNODE_pvData;
            pmqfile->PMSGF_iFlag = flag;
            
            API_AtomicInc(&pxnode->PXNODE_atomic);
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            
            __resAddRawHook(&pmqfile->PMSGF_resraw, (VOIDFUNCPTR)mq_close, 
                            pmqfile, 0, 0, 0, 0, 0);                    /*  加入资源管理器              */
                    
            return  ((mqd_t)pmqfile);                                   /*  返回句柄地址                */
        }
    
    } else {
        
        if (flag & O_CREAT) {
            mode_t          mode;
            struct mq_attr *pmqattr;
            
            va_start(valist, flag);
            mode    = va_arg(valist, mode_t);
            pmqattr = va_arg(valist, struct mq_attr *);
            va_end(valist);
        
            if (pmqattr == LW_NULL) {
                pmqattr =  &mq_attr_default;                            /*  使用默认属性                */
            } else {
                if ((pmqattr->mq_msgsize <= 0) ||
                    (pmqattr->mq_maxmsg  <= 0)) {
                    __PX_UNLOCK();                                      /*  解锁 posix                  */
                    errno = EINVAL;
                    return  (MQ_FAILED);
                }
            }
            
            /*
             *  开始创建消息队列及句柄.
             */
            pmqfile = (__PX_MSG_FILE *)__SHEAP_ALLOC(sizeof(__PX_MSG_FILE));
            if (pmqfile == LW_NULL) {
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                errno = ENOMEM;
                return  (MQ_FAILED);
            }
            
            pmq = __mqueueCreate(name, mode, pmqattr);                  /*  创建消息队列                */
            if (pmq == LW_NULL) {
                __SHEAP_FREE(pmqfile);
                __PX_UNLOCK();                                          /*  解锁 posix                  */
                return  (MQ_FAILED);
            }
            __pxnameAdd(&pmq->PMSG_pxnode);                             /*  加入名字节点表              */
            
            pmqfile->PMSGF_pmg   = pmq;
            pmqfile->PMSGF_iFlag = flag;
            
            API_AtomicInc(&pmq->PMSG_pxnode.PXNODE_atomic);
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            
            __resAddRawHook(&pmqfile->PMSGF_resraw, (VOIDFUNCPTR)mq_close, 
                            pmqfile, 0, 0, 0, 0, 0);                    /*  加入资源管理器              */
            
            _DebugFormat(__LOGMESSAGE_LEVEL, "posix msgqueue \"%s\" has been create.\r\n", name);
            
            return  ((mqd_t)pmqfile);                                   /*  返回句柄地址                */
        
        } else {
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            errno = ENOENT;
            return  (MQ_FAILED);
        }
    }
}
/*********************************************************************************************************
** 函数名称: mq_close
** 功能描述: 关闭一个 posix 消息队列.
** 输　入  : mqd       句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  mq_close (mqd_t  mqd)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;

    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    __PX_LOCK();                                                        /*  锁住 posix                  */
    if (API_AtomicGet(&pmq->PMSG_pxnode.PXNODE_atomic) > 0) {
        API_AtomicDec(&pmq->PMSG_pxnode.PXNODE_atomic);                 /*  仅仅减少使用计数            */
    }
    __PX_UNLOCK();                                                      /*  解锁 posix                  */

    __resDelRawHook(&pmqfile->PMSGF_resraw);

    __SHEAP_FREE(pmqfile);                                              /*  释放句柄                    */

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: mq_unlink
** 功能描述: 删除一个 posix 消息队列.
** 输　入  : name      消息队列名
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  mq_unlink (const char  *name)
{
    __PX_MSG           *pmq;
    __PX_NAME_NODE     *pxnode;

    if (name == LW_NULL) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    __PX_LOCK();                                                        /*  锁住 posix                  */
    pxnode = __pxnameSeach(name, -1);
    if (pxnode) {
        if (pxnode->PXNODE_iType != __PX_NAMED_OBJECT_MQ) {             /*  检查类型                    */
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            errno = ENOENT;
            return  (PX_ERROR);
        }
        if (API_AtomicGet(&pxnode->PXNODE_atomic) > 0) {
            __PX_UNLOCK();                                              /*  解锁 posix                  */
            errno = EBUSY;
            return  (PX_ERROR);
        }
        pmq = (__PX_MSG *)pxnode->PXNODE_pvData;
        
        __pxnameDel(name);                                              /*  从名字表中删除              */
        
        _DebugFormat(__LOGMESSAGE_LEVEL, "posix msgqueue \"%s\" has been delete.\r\n", name);
        
        __mqueueDelete(pmq);                                            /*  删除消息队列控制块          */
    
        __PX_UNLOCK();                                                  /*  解锁 posix                  */
        return  (ERROR_NONE);
        
    } else {
        __PX_UNLOCK();                                                  /*  解锁 posix                  */
        errno = ENOENT;
        return  (PX_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: mq_getattr
** 功能描述: 获得 posix 消息队列属性.
** 输　入  : mqd       句柄
**           pmqattr   属性
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  mq_getattr (mqd_t  mqd, struct mq_attr *pmqattr)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;
    
    if (pmqattr == LW_NULL) {
        errno = EINVAL;
        return  (PX_ERROR);
    }

    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    __PX_MQ_LOCK(pmq);                                                  /*  锁定消息队列                */
    *pmqattr = pmq->PMSG_mqattr;
    __PX_MQ_UNLOCK(pmq);                                                /*  解锁消息队列                */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: mq_setattr
** 功能描述: 设置 posix 消息队列属性.
** 输　入  : mqd           句柄
**           pmqattrNew    属性
**           pmqattrOld    先早的属性
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  mq_setattr (mqd_t  mqd, const struct mq_attr *pmqattrNew, struct mq_attr *pmqattrOld)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;
    
    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    __PX_MQ_LOCK(pmq);                                                  /*  锁定消息队列                */
    if (pmqattrOld) {
        *pmqattrOld = pmq->PMSG_mqattr;
    }
    if (pmqattrNew) {
        if (pmqattrNew->mq_flags & O_NONBLOCK) {
            pmq->PMSG_mqattr.mq_flags |= O_NONBLOCK;
        } else {
            pmq->PMSG_mqattr.mq_flags &= ~O_NONBLOCK;
        }
    }
    __PX_MQ_UNLOCK(pmq);                                                /*  解锁消息队列                */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: mq_send
** 功能描述: 将一条消息发送到 posix 消息队列.
** 输　入  : mqd           句柄
**           msg           消息指针
**           msglen        消息长度
**           msgprio       消息优先级    (< MQ_PRIO_MAX)
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  mq_send (mqd_t  mqd, const char  *msg, size_t  msglen, unsigned msgprio)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;
    uint_t              uiRealPrio;
    
    ULONG               ulTimeout;
    INT                 iRet;
    
    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    if ((msg == LW_NULL) || (msglen == 0)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    if (msgprio >= MQ_PRIO_MAX) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    uiRealPrio = MQ_PRIO_MAX - msgprio;                                 /*  计算真实优先级              */
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    if (msglen > pmq->PMSG_mqattr.mq_msgsize) {                         /*  消息太大                    */
        errno = EMSGSIZE;
        return  (PX_ERROR);
    }
    if ((pmqfile->PMSGF_iFlag & O_ACCMODE) == O_RDONLY) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    
    if ((pmqfile->PMSGF_iFlag & O_NONBLOCK) ||
        (pmq->PMSG_mqattr.mq_flags & O_NONBLOCK)) {                     /*  不阻塞                      */
        ulTimeout = LW_OPTION_NOT_WAIT;
        
    } else {
        ulTimeout = LW_OPTION_WAIT_INFINITE;
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    iRet = __mqueueSend(pmq, msg, msglen, uiRealPrio, ulTimeout);       /*  发送消息                    */
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: mq_timedsend
** 功能描述: 将一条消息发送到 posix 消息队列.
** 输　入  : mqd           句柄
**           msg           消息指针
**           msglen        消息长度
**           msgprio       消息优先级    (< MQ_PRIO_MAX)
**           abs_timeout   超时时间 (注意: 这里是绝对时间, 即一个确定的历史时间例如: 2009.12.31 15:36:04)
                           笔者觉得这个不是很爽, 应该再加一个函数可以等待相对时间.
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  mq_timedsend (mqd_t  mqd, const char  *msg, size_t  msglen, 
                   unsigned msgprio, const struct timespec *abs_timeout)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;
    uint_t              uiRealPrio;
    
    ULONG               ulTimeout;
    INT                 iRet;
    struct timespec     tvNow;
    struct timespec     tvWait = {0, 0};
    
    if ((abs_timeout == LW_NULL) || (abs_timeout->tv_nsec >= __TIMEVAL_NSEC_MAX)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    if ((msg == LW_NULL) || (msglen == 0)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    if (msgprio >= MQ_PRIO_MAX) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    uiRealPrio = MQ_PRIO_MAX - msgprio;                                 /*  计算真实优先级              */
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    if (msglen > pmq->PMSG_mqattr.mq_msgsize) {                         /*  消息太大                    */
        errno = EMSGSIZE;
        return  (PX_ERROR);
    }
    if ((pmqfile->PMSGF_iFlag & O_ACCMODE) == O_RDONLY) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    
    lib_clock_gettime(CLOCK_REALTIME, &tvNow);                          /*  获得当前系统时间            */
    if (__timespecLeftTime(&tvNow, abs_timeout)) {
        tvWait = *abs_timeout;
        __timespecSub(&tvWait, &tvNow);                                 /*  计算与当前等待的时间间隔    */
    }
    /*
     *  注意: 当 tvWait 超过ulong tick范围时, 将自然产生溢出, 导致超时时间不准确.
     */
    ulTimeout = __timespecToTick(&tvWait);                              /*  变换为 tick                 */
    
    if ((pmqfile->PMSGF_iFlag & O_NONBLOCK) ||
        (pmq->PMSG_mqattr.mq_flags & O_NONBLOCK)) {                     /*  不阻塞                      */
        ulTimeout = LW_OPTION_NOT_WAIT;
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    iRet = __mqueueSend(pmq, msg, msglen, uiRealPrio, ulTimeout);       /*  发送消息                    */
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: mq_reltimedsend_np
** 功能描述: 将一条消息发送到 posix 消息队列.
** 输　入  : mqd           句柄
**           msg           消息指针
**           msglen        消息长度
**           msgprio       消息优先级    (< MQ_PRIO_MAX)
**           rel_timeout   相对超时时间.
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if LW_CFG_POSIXEX_EN > 0

LW_API 
int  mq_reltimedsend_np (mqd_t  mqd, const char  *msg, size_t  msglen, 
                         unsigned msgprio, const struct timespec *rel_timeout)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;
    uint_t              uiRealPrio;
    
    ULONG               ulTimeout;
    INT                 iRet;
    
    if ((rel_timeout == LW_NULL) || (rel_timeout->tv_nsec >= __TIMEVAL_NSEC_MAX)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    if ((msg == LW_NULL) || (msglen == 0)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    if (msgprio >= MQ_PRIO_MAX) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    uiRealPrio = MQ_PRIO_MAX - msgprio;                                 /*  计算真实优先级              */
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    if (msglen > pmq->PMSG_mqattr.mq_msgsize) {                         /*  消息太大                    */
        errno = EMSGSIZE;
        return  (PX_ERROR);
    }
    if ((pmqfile->PMSGF_iFlag & O_ACCMODE) == O_RDONLY) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    
    /*
     *  注意: 当 rel_timeout 超过ulong tick范围时, 将自然产生溢出, 导致超时时间不准确.
     */
    ulTimeout = __timespecToTick(rel_timeout);                          /*  变换为 tick                 */
    
    if ((pmqfile->PMSGF_iFlag & O_NONBLOCK) ||
        (pmq->PMSG_mqattr.mq_flags & O_NONBLOCK)) {                     /*  不阻塞                      */
        ulTimeout = LW_OPTION_NOT_WAIT;
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    iRet = __mqueueSend(pmq, msg, msglen, uiRealPrio, ulTimeout);       /*  发送消息                    */
    
    return  (iRet);
}

#endif                                                                  /*  LW_CFG_POSIXEX_EN > 0       */
/*********************************************************************************************************
** 函数名称: mq_receive
** 功能描述: 从 posix 消息队列接收一条消息.
** 输　入  : mqd           句柄
**           msg           消息缓冲区
**           msglen        缓冲区长度
**           pmsgprio      接收消息的优先级 (返回)
** 输　出  : 接收消息的长度
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ssize_t  mq_receive (mqd_t  mqd, char  *msg, size_t  msglen, unsigned *pmsgprio)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;
    
    ULONG               ulTimeout;
    ssize_t             sstRet;
    
    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    if ((msg == LW_NULL) || (msglen == 0)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    if (msglen < pmq->PMSG_mqattr.mq_msgsize) {                         /*  缓冲太小                    */
        errno = EMSGSIZE;
        return  (PX_ERROR);
    }
    if ((pmqfile->PMSGF_iFlag & O_ACCMODE) & O_WRONLY) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    
    if ((pmqfile->PMSGF_iFlag & O_NONBLOCK) ||
        (pmq->PMSG_mqattr.mq_flags & O_NONBLOCK)) {                     /*  不阻塞                      */
        ulTimeout = LW_OPTION_NOT_WAIT;
        
    } else {
        ulTimeout = LW_OPTION_WAIT_INFINITE;
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    sstRet = __mqueueRecv(pmq, msg, msglen, pmsgprio, ulTimeout);       /*  接收消息                    */
    
    return  (sstRet);
}
/*********************************************************************************************************
** 函数名称: mq_timedreceive
** 功能描述: 从 posix 消息队列接收一条消息.
** 输　入  : mqd           句柄
**           msg           消息缓冲区
**           msglen        缓冲区长度
**           pmsgprio      接收消息的优先级 (返回)
**           abs_timeout   超时时间
** 输　出  : 接收消息的长度
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ssize_t  mq_timedreceive (mqd_t  mqd, char  *msg, size_t  msglen, 
                          unsigned *pmsgprio, const struct timespec *abs_timeout)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;
    
    ULONG               ulTimeout;
    ssize_t             sstRet;
    struct timespec     tvNow;
    struct timespec     tvWait = {0, 0};
    
    if ((abs_timeout == LW_NULL) || (abs_timeout->tv_nsec >= __TIMEVAL_NSEC_MAX)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    if ((msg == LW_NULL) || (msglen == 0)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    if (msglen < pmq->PMSG_mqattr.mq_msgsize) {                         /*  缓冲太小                    */
        errno = EMSGSIZE;
        return  (PX_ERROR);
    }
    if ((pmqfile->PMSGF_iFlag & O_ACCMODE) & O_WRONLY) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    
    lib_clock_gettime(CLOCK_REALTIME, &tvNow);                          /*  获得当前系统时间            */
    if (__timespecLeftTime(&tvNow, abs_timeout)) {
        tvWait = *abs_timeout;
        __timespecSub(&tvWait, &tvNow);                                 /*  计算与当前等待的时间间隔    */
    }
    /*
     *  注意: 当 tvWait 超过ulong tick范围时, 将自然产生溢出, 导致超时时间不准确.
     */
    ulTimeout = __timespecToTick(&tvWait);                              /*  变换为 tick                 */
    
    if ((pmqfile->PMSGF_iFlag & O_NONBLOCK) ||
        (pmq->PMSG_mqattr.mq_flags & O_NONBLOCK)) {                     /*  不阻塞                      */
        ulTimeout = LW_OPTION_NOT_WAIT;
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    sstRet = __mqueueRecv(pmq, msg, msglen, pmsgprio, ulTimeout);       /*  接收消息                    */
    
    return  (sstRet);
}
/*********************************************************************************************************
** 函数名称: mq_reltimedreceive_np
** 功能描述: 从 posix 消息队列接收一条消息.
** 输　入  : mqd           句柄
**           msg           消息缓冲区
**           msglen        缓冲区长度
**           pmsgprio      接收消息的优先级 (返回)
**           rel_timeout   相对超时时间
** 输　出  : 接收消息的长度
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if LW_CFG_POSIXEX_EN > 0

LW_API 
ssize_t  mq_reltimedreceive_np (mqd_t  mqd, char  *msg, size_t  msglen, 
                                unsigned *pmsgprio, const struct timespec *rel_timeout)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;
    
    ULONG               ulTimeout;
    ssize_t             sstRet;
    
    if ((rel_timeout == LW_NULL) || (rel_timeout->tv_nsec >= __TIMEVAL_NSEC_MAX)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    if ((msg == LW_NULL) || (msglen == 0)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    if (msglen < pmq->PMSG_mqattr.mq_msgsize) {                         /*  缓冲太小                    */
        errno = EMSGSIZE;
        return  (PX_ERROR);
    }
    if ((pmqfile->PMSGF_iFlag & O_ACCMODE) & O_WRONLY) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    
    /*
     *  注意: 当 rel_timeout 超过ulong tick范围时, 将自然产生溢出, 导致超时时间不准确.
     */
    ulTimeout = __timespecToTick(rel_timeout);                          /*  变换为 tick                 */
    
    if ((pmqfile->PMSGF_iFlag & O_NONBLOCK) ||
        (pmq->PMSG_mqattr.mq_flags & O_NONBLOCK)) {                     /*  不阻塞                      */
        ulTimeout = LW_OPTION_NOT_WAIT;
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    sstRet = __mqueueRecv(pmq, msg, msglen, pmsgprio, ulTimeout);       /*  接收消息                    */
    
    return  (sstRet);
}

#endif                                                                  /*  LW_CFG_POSIXEX_EN > 0       */
/*********************************************************************************************************
** 函数名称: mq_notify
** 功能描述: 当消息队列可读时, 注册一个通知信号.
** 输　入  : mqd           句柄
**           pnotify       信号事件
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  mq_notify (mqd_t  mqd, const struct sigevent  *pnotify)
{
    __PX_MSG           *pmq;
    __PX_MSG_FILE      *pmqfile;

    if ((mqd == MQ_FAILED) || (mqd == 0)) {
        errno = EBADF;
        return  (PX_ERROR);
    }
    
    pmqfile = (__PX_MSG_FILE *)mqd;
    pmq = pmqfile->PMSGF_pmg;
    
    if (pnotify) {                                                      /*  安装                        */
        if (!__issig(pnotify->sigev_signo)) {                           /*  信号值错误                  */
            errno = EINVAL;
            return  (PX_ERROR);
        }
        __PX_MQ_LOCK(pmq);                                              /*  锁定消息队列                */
        pmq->PMSG_pmsgntf.PMSGNTF_sigevent   = *pnotify;
        pmq->PMSG_pmsgntf.PMSGNTF_ulThreadId = API_ThreadIdSelf();
        __PX_MQ_UNLOCK(pmq);                                            /*  解锁消息队列                */
        
    } else {                                                            /*  卸载                        */
        __PX_MQ_LOCK(pmq);                                              /*  锁定消息队列                */
        pmq->PMSG_pmsgntf.PMSGNTF_ulThreadId = LW_OBJECT_HANDLE_INVALID;
        __PX_MQ_UNLOCK(pmq);                                            /*  解锁消息队列                */
    }

    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
