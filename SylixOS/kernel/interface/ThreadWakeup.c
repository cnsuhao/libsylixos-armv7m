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
** 文   件   名: ThreadWakeup.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 07 月 19 日
**
** 描        述: 线程从睡眠模式唤醒

** BUG
2007.07.19  加入 _DebugHandle() 功能
2008.03.29  使用新的等待机制.
2008.03.30  使用新的就绪环操作.
2010.01.22  支持 SMP.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: API_ThreadWakeup
** 功能描述: 线程从睡眠模式唤醒
** 输　入  : 
**           ulId    线程句柄
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API
ULONG  API_ThreadWakeup (LW_OBJECT_HANDLE  ulId)
{
             INTREG                iregInterLevel;
    REGISTER UINT16                usIndex;
    REGISTER PLW_CLASS_TCB         ptcb;
    REGISTER PLW_CLASS_PCB         ppcb;
	
    usIndex = _ObjectGetIndex(ulId);
	
#if LW_CFG_ARG_CHK_EN > 0
    if (!_ObjectClassOK(ulId, _OBJECT_THREAD)) {                         /*  检查 ID 类型有效性         */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "thread handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    
    if (_Thread_Index_Invalid(usIndex)) {                                /*  检查线程有效性             */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "thread handle invalidate.\r\n");
        _ErrorHandle(ERROR_THREAD_NULL);
        return  (ERROR_THREAD_NULL);
    }
#endif

    __KERNEL_ENTER();                                                   /*  进入内核                    */
    if (_Thread_Invalid(usIndex)) {
        __KERNEL_EXIT();                                                /*  退出内核                    */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "thread handle invalidate.\r\n");
        _ErrorHandle(ERROR_THREAD_NULL);
        return  (ERROR_THREAD_NULL);
    }
    
    iregInterLevel = KN_INT_DISABLE();                                  /*  关闭中断                    */
    
    ptcb = _K_ptcbTCBIdTable[usIndex];
    ppcb = _GetPcb(ptcb);
    
    if (ptcb->TCB_usStatus & LW_THREAD_STATUS_DELAY) {
        __DEL_FROM_WAKEUP_LINE(ptcb);                                   /*  从等待链中删除              */
        ptcb->TCB_ulDelay = 0ul;
        
        if (ptcb->TCB_usStatus & LW_THREAD_STATUS_PEND_ANY) {
            ptcb->TCB_usStatus &= (~LW_THREAD_STATUS_PEND_ANY);
            ptcb->TCB_ucWaitTimeOut = LW_WAIT_TIME_OUT;                 /*  等待超时                    */
        } else {
            ptcb->TCB_ucWaitTimeOut = LW_WAIT_TIME_CLEAR;               /*  没有等待事件                */
        }
        
        if (__LW_THREAD_IS_READY(ptcb)) {                               /*  检查是否就绪                */
            ptcb->TCB_ucSchedActivate = LW_SCHED_ACT_OTHER;
            __ADD_TO_READY_RING(ptcb, ppcb);                            /*  加入就绪表                  */
        }
        
        KN_INT_ENABLE(iregInterLevel);
        __KERNEL_EXIT();                                                /*  退出内核 (可能会调度)       */
        
        MONITOR_EVT_LONG1(MONITOR_EVENT_ID_THREAD, MONITOR_EVENT_THREAD_WAKEUP, ulId, LW_NULL);
        
        _ErrorHandle(ERROR_NONE);
        return  (ERROR_NONE);
    
    } else {
        KN_INT_ENABLE(iregInterLevel);
        __KERNEL_EXIT();                                                /*  退出内核                    */
        
        _DebugHandle(__ERRORMESSAGE_LEVEL, "thread is not in SLEEP mode.\r\n");
        _ErrorHandle(ERROR_THREAD_NOT_SLEEP);
        return  (ERROR_THREAD_NOT_SLEEP);
    }
}

/*********************************************************************************************************
  END
*********************************************************************************************************/
