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
** 文   件   名: MsgQueueTryReceive.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 01 月 07 日
**
** 描        述: 无等待获得消息队列消息

** BUG
2007.09.19  加入 _DebugHandle() 功能。
2009.04.08  加入对 SMP 多核的支持.
2009.06.25  pulMsgLen 可以为 NULL,
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: API_MsgQueueTryReceive
** 功能描述: 无等待获得消息队列消息
** 输　入  : 
**           ulId            消息队列句柄
**           pvMsgBuffer     消息缓冲区
**           stMaxByteSize   消息缓冲区大小
**           pstMsgLen       消息长度
** 输　出  : ERROR_CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if (LW_CFG_MSGQUEUE_EN > 0) && (LW_CFG_MAX_MSGQUEUES > 0)

LW_API  
ULONG  API_MsgQueueTryReceive (LW_OBJECT_HANDLE    ulId,
                               PVOID               pvMsgBuffer,
                               size_t              stMaxByteSize,
                               size_t             *pstMsgLen)
{
             INTREG                iregInterLevel;
    REGISTER UINT16                usIndex;
    REGISTER PLW_CLASS_EVENT       pevent;
    REGISTER PLW_CLASS_MSGQUEUE    pmsgqueue;
            
             size_t                stMsgLenTemp;
             
    usIndex = _ObjectGetIndex(ulId);
    
    if (pstMsgLen == LW_NULL) {
        pstMsgLen =  &stMsgLenTemp;                                     /*  临时变量记录消息长短        */
    }
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!pvMsgBuffer) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pvMsgBuffer invalidate.\r\n");
        _ErrorHandle(ERROR_MSGQUEUE_MSG_NULL);
        return  (ERROR_MSGQUEUE_MSG_NULL);
    }
    if (!_ObjectClassOK(ulId, _OBJECT_MSGQUEUE)) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "msgqueue handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    if (_Event_Index_Invalid(usIndex)) {
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
    
    if (pevent->EVENT_ulCounter) {                                      /*  事件有效                    */
        pevent->EVENT_ulCounter--;
        
        _MsgQueueGetMsg(pmsgqueue, 
                        pvMsgBuffer, 
                        stMaxByteSize,
                        pstMsgLen);                                     /*  获得消息                    */
        
        LW_SPIN_UNLOCK_QUICK(&pevent->EVENT_slLock, iregInterLevel);    /*  打开中断, 同时打开 spinlock */
        
        return  (ERROR_NONE);
    
    } else {                                                            /*  事件无效                    */
        LW_SPIN_UNLOCK_QUICK(&pevent->EVENT_slLock, iregInterLevel);    /*  打开中断, 同时打开 spinlock */
    
        _ErrorHandle(ERROR_THREAD_WAIT_TIMEOUT);                        /*  没有事件发生                */
        return  (ERROR_THREAD_WAIT_TIMEOUT);
    }
}
#endif                                                                  /*  (LW_CFG_MSGQUEUE_EN > 0)    */
                                                                        /*  (LW_CFG_MAX_MSGQUEUES > 0)  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
