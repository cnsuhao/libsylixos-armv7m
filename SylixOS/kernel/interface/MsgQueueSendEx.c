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
** 文   件   名: MsgQueueSendEx.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 01 月 07 日
**
** 描        述: 向消息队列发送消息(高级接口函数)

** BUG
2007.09.19  加入 _DebugHandle() 功能。
2008.05.18  使用 __KERNEL_ENTER() 代替 ThreadLock();
2009.04.08  加入对 SMP 多核的支持.
2010.01.22  修正进入内核的时机.
2013.03.17  加入对自动截断消息时的判断.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: API_MsgQueueSendEx
** 功能描述: 向消息队列发送消息
** 输　入  : 
**           ulId                   消息队列句柄
**           pvMsgBuffer            消息缓冲区
**           stMsgLen               消息长短
**           ulOption               消息选项       LW_OPTION_DEFAULT or LW_OPTION_URGENT or 
**                                                 LW_OPTION_BROADCAST
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if (LW_CFG_MSGQUEUE_EN > 0) && (LW_CFG_MAX_MSGQUEUES > 0)

LW_API  
ULONG  API_MsgQueueSendEx (LW_OBJECT_HANDLE  ulId,
                           const PVOID       pvMsgBuffer,
                           size_t            stMsgLen,
                           ULONG             ulOption)
{
             INTREG                iregInterLevel;
    REGISTER UINT16                usIndex;
    REGISTER PLW_CLASS_EVENT       pevent;
    REGISTER PLW_CLASS_MSGQUEUE    pmsgqueue;
    REGISTER PLW_CLASS_TCB         ptcb;
    REGISTER PLW_LIST_RING        *ppringList;                          /*  等待队列地址                */
    
    REGISTER size_t                stRealLen;
    
    if (ulOption == LW_OPTION_DEFAULT) {                                /*  普通发送                    */
        return  (API_MsgQueueSend(ulId, pvMsgBuffer, stMsgLen));
    }
    
    usIndex = _ObjectGetIndex(ulId);
    
__re_send:
#if LW_CFG_ARG_CHK_EN > 0
    if (!pvMsgBuffer) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pvMsgBuffer invalidate.\r\n");
        _ErrorHandle(ERROR_MSGQUEUE_MSG_NULL);
        return  (ERROR_MSGQUEUE_MSG_NULL);
    }
    if (!stMsgLen) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "ulMsgLen invalidate.\r\n");
        _ErrorHandle(ERROR_MSGQUEUE_MSG_LEN);
        return  (ERROR_MSGQUEUE_MSG_LEN);
    }
    if (!_ObjectClassOK(ulId, _OBJECT_MSGQUEUE)) {                      /*  类型是否正确                */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "msgqueue handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    if (_Event_Index_Invalid(usIndex)) {                                /*  下标是否正正确              */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "msgqueue handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
#endif
    pevent = &_K_eventBuffer[usIndex];
    
    LW_SPIN_LOCK_QUICK(&pevent->EVENT_slLock, &iregInterLevel);         /*  关闭中断同时锁住 spinlock   */
    
    if (_Event_Type_Invalid(usIndex, LW_TYPE_EVENT_MSGQUEUE)) {
        LW_SPIN_UNLOCK_QUICK(&pevent->EVENT_slLock, iregInterLevel);    /*  打开中断, 同时打开 spinlock */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "msgqueue handle invalidate.\r\n");
        _ErrorHandle(ERROR_MSGQUEUE_TYPE);
        return  (ERROR_MSGQUEUE_TYPE);
    }
    pmsgqueue = (PLW_CLASS_MSGQUEUE)pevent->EVENT_pvPtr;
    
    if (stMsgLen > pmsgqueue->MSGQUEUE_stEachMsgByteSize) {             /*  长度太长                    */
        LW_SPIN_UNLOCK_QUICK(&pevent->EVENT_slLock, iregInterLevel);    /*  打开中断, 同时打开 spinlock */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "ulMsgLen invalidate.\r\n");
        _ErrorHandle(ERROR_MSGQUEUE_MSG_LEN);
        return  (ERROR_MSGQUEUE_MSG_LEN);
    }
    
    switch (ulOption) {
    
    case LW_OPTION_URGENT:
        if (_EventWaitNum(pevent)) {
            BOOL    bSendOk = LW_TRUE;
            
            __KERNEL_ENTER();                                           /*  进入内核                    */
            
            if (pevent->EVENT_ulOption & LW_OPTION_WAIT_PRIORITY) {     /*  优先级等待队列              */
                _EVENT_DEL_Q_PRIORITY(ppringList);                      /*  检查需要激活的队列          */
                                                                        /*  激活优先级等待线程          */
                ptcb = _EventReadyPriorityLowLevel(pevent, LW_NULL, ppringList);
            
            } else {
                _EVENT_DEL_Q_FIFO(ppringList);                          /*  检查需要激活的FIFO队列      */
                                                                        /*  激活FIFO等待线程            */
                ptcb = _EventReadyFifoLowLevel(pevent, LW_NULL, ppringList);
            }
        
            if ((stMsgLen > ptcb->TCB_stMaxByteSize) && 
                !(ptcb->TCB_ulRecvOption & LW_OPTION_NOERROR)) {        /*  是否允许自动截断            */
                *ptcb->TCB_pstMsgByteSize = 0;
                ptcb->TCB_stMaxByteSize = 0;
                bSendOk = LW_FALSE;
            
            } else {
                stRealLen = (stMsgLen < ptcb->TCB_stMaxByteSize) ?
                            (stMsgLen) : (ptcb->TCB_stMaxByteSize);     /*  确定信息拷贝长短            */
                
                *ptcb->TCB_pstMsgByteSize = stRealLen;                  /*  保存长短                    */
                lib_memcpy(ptcb->TCB_pvMsgQueueMessage,                 /*  传递消息                    */
                           pvMsgBuffer, 
                           stRealLen);
            }
        
            LW_SPIN_UNLOCK_QUICK(&pevent->EVENT_slLock, iregInterLevel);/*  打开中断, 同时打开 spinlock */
            
            _EventReadyHighLevel(ptcb, LW_THREAD_STATUS_MSGQUEUE);      /*  处理 TCB                    */
            
            MONITOR_EVT_LONG2(MONITOR_EVENT_ID_MSGQ, MONITOR_EVENT_MSGQ_POST, 
                              ulId, ptcb->TCB_ulId, LW_NULL);
                             
            __KERNEL_EXIT();                                            /*  退出内核                    */
            
            if (bSendOk == LW_FALSE) {
                goto    __re_send;                                      /*  重新发送                    */
            }
            return  (ERROR_NONE);
        
        } else {                                                        /*  没有线程等待                */
            if (pevent->EVENT_ulCounter < pevent->EVENT_ulMaxCounter) { /*  检查是否还有空间加          */
                pevent->EVENT_ulCounter++;
                _MsgQueueSendMsgUrgent(pmsgqueue, pvMsgBuffer, stMsgLen);  
                                                                        /*  保存消息                    */
                LW_SPIN_UNLOCK_QUICK(&pevent->EVENT_slLock, iregInterLevel);  
                                                                        /*  打开中断, 同时打开 spinlock */
                return  (ERROR_NONE);
            
            } else {                                                    /*  已经满了                    */
                LW_SPIN_UNLOCK_QUICK(&pevent->EVENT_slLock, iregInterLevel);  
                                                                        /*  打开中断, 同时打开 spinlock */
                _ErrorHandle(ERROR_MSGQUEUE_FULL);
                return  (ERROR_MSGQUEUE_FULL);
            }
        }
    case LW_OPTION_BROADCAST:                                           /*  广播发送                    */
                                                                        /*  只激活等待线程              */
        __KERNEL_ENTER();                                               /*  进入内核                    */
        
        while (_EventWaitNum(pevent)) {
            if (pevent->EVENT_ulOption & LW_OPTION_WAIT_PRIORITY) {     /*  优先级等待队列              */
                _EVENT_DEL_Q_PRIORITY(ppringList);                      /*  检查需要激活的队列          */
                                                                        /*  激活优先级等待线程          */
                ptcb = _EventReadyPriorityLowLevel(pevent, LW_NULL, ppringList);
            
            } else {
                _EVENT_DEL_Q_FIFO(ppringList);                          /*  检查需要激活的FIFO队列      */
                                                                        /*  激活FIFO等待线程            */
                ptcb = _EventReadyFifoLowLevel(pevent, LW_NULL, ppringList);
            }
            
            if ((stMsgLen > ptcb->TCB_stMaxByteSize) && 
                !(ptcb->TCB_ulRecvOption & LW_OPTION_NOERROR)) {        /*  是否允许自动截断            */
                *ptcb->TCB_pstMsgByteSize = 0;
                ptcb->TCB_stMaxByteSize = 0;
            
            } else {
                stRealLen = (stMsgLen < ptcb->TCB_stMaxByteSize) ?
                            (stMsgLen) : (ptcb->TCB_stMaxByteSize);     /*  确定信息拷贝长短            */
                
                *ptcb->TCB_pstMsgByteSize = stRealLen;                  /*  保存长短                    */
                lib_memcpy(ptcb->TCB_pvMsgQueueMessage,                 /*  传递消息                    */
                           pvMsgBuffer, 
                           stRealLen);
            }
                       
            /*
             *  注意: 以下操作没有释放 spinlock.
             */
            KN_INT_ENABLE(iregInterLevel);                              /*  打开中断                    */
            
            _EventReadyHighLevel(ptcb, LW_THREAD_STATUS_MSGQUEUE);      /*  处理 TCB                    */
            
            MONITOR_EVT_LONG2(MONITOR_EVENT_ID_MSGQ, MONITOR_EVENT_MSGQ_POST, 
                              ulId, ptcb->TCB_ulId, LW_NULL);
                         
            iregInterLevel = KN_INT_DISABLE();
        }
        
        LW_SPIN_UNLOCK_QUICK(&pevent->EVENT_slLock, iregInterLevel);    /*  打开中断, 同时打开 spinlock */
        
        __KERNEL_EXIT();                                                /*  退出内核                    */
        return  (ERROR_NONE);
        
    default:
        LW_SPIN_UNLOCK_QUICK(&pevent->EVENT_slLock, iregInterLevel);    /*  打开中断, 同时打开 spinlock */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "ulOption invalidate.\r\n");
        _ErrorHandle(ERROR_MSGQUEUE_OPTION);
        return  (ERROR_MSGQUEUE_OPTION);
    }
}
#endif                                                                  /*  (LW_CFG_MSGQUEUE_EN > 0)    */
                                                                        /*  (LW_CFG_MAX_MSGQUEUES > 0)  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
