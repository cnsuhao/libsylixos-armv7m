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
** 文   件   名: ThreadIpc.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 03 月 16 日
**
** 描        述: 这是系统对 IPC 或者其他要求阻塞与激活的支持。

** BUG:
2013.05.05  判断调度器返回值, 决定是重启调用还是退出.
2013.07.18  使用新的获取 TCB 的方法, 确保 SMP 系统安全.
2013.09.17  使用 POSIX 规定的取消点动作.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  s_internal.h 中也有相关定义
*********************************************************************************************************/
#if LW_CFG_THREAD_CANCEL_EN > 0
#define __THREAD_CANCEL_POINT()         API_ThreadTestCancel()
#else
#define __THREAD_CANCEL_POINT()
#endif                                                                  /*  LW_CFG_THREAD_CANCEL_EN     */
/*********************************************************************************************************
** 函数名称: API_ThreadIpcWait
** 功能描述: 当前线程等待一个 IPC 事件
** 输　入  : ulTimeOut     超时时间
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
ULONG  API_ThreadIpcWait (ULONG  ulTimeOut)
{
             INTREG             iregInterLevel;
             ULONG              ulTimeSave;                             /*  系统事件记录                */
             INT                iSchedRet;
             
             PLW_CLASS_TCB      ptcbCur;
    REGISTER PLW_CLASS_PCB      ppcb;
    
__wait_again:
    if (ulTimeOut == LW_OPTION_NOT_WAIT) {                              /*  不进行等待                  */
        _ErrorHandle(EAGAIN);
        return  (EAGAIN);
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);                                       /*  当前任务控制块              */
    
    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */
    
    ptcbCur->TCB_usStatus      |= LW_THREAD_STATUS_IPC;                 /*  等待 IPC                    */
    ptcbCur->TCB_ucWaitTimeOut  = LW_WAIT_TIME_CLEAR;                   /*  清空等待时间                */
    
    ppcb = _GetPcb(ptcbCur);
    __DEL_FROM_READY_RING(ptcbCur, ppcb);                               /*  从就绪表中删除              */
    
    if (ulTimeOut == LW_OPTION_WAIT_INFINITE) {
        ptcbCur->TCB_ulDelay = 0ul;
    } else {
        ptcbCur->TCB_ulDelay = ulTimeOut;                               /*  设置超时时间                */
        __ADD_TO_WAKEUP_LINE(ptcbCur);                                  /*  加入等待扫描链              */
    }
    
    __KERNEL_TIME_GET(ulTimeSave, ULONG);                               /*  记录系统时间                */
    
    iSchedRet = __KERNEL_EXIT_IRQ(iregInterLevel);                      /*  调度器解锁                  */
    if (iSchedRet == LW_SIGNAL_EINTR) {
        _ErrorHandle(EINTR);
        return  (EINTR);
    
    } else if (iSchedRet == LW_SIGNAL_RESTART) {
        ulTimeOut = _sigTimeOutRecalc(ulTimeSave, ulTimeOut);           /*  重新计算超时时间            */
        goto    __wait_again;
    }
    
    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核                    */
    if (ptcbCur->TCB_ucWaitTimeOut == LW_WAIT_TIME_OUT) {               /*  等待超时                    */
        __KERNEL_EXIT_IRQ(iregInterLevel);                              /*  退出内核                    */
        _ErrorHandle(EAGAIN);
        return  (EAGAIN);
    }
    __KERNEL_EXIT_IRQ(iregInterLevel);                                  /*  退出内核                    */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_ThreadIpcWakeup
** 功能描述: 激活等待指定 IPC 的线程
** 输　入  : ulId      线程 ID
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
** 注  意  : 使用的时候一定要确保目标线程不是阻塞在等待信号上, 否则或造成错误.

                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
ULONG  API_ThreadIpcWakeup (LW_OBJECT_HANDLE  ulId)
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
    }
    
    if (ptcb->TCB_usStatus & LW_THREAD_STATUS_IPC) {
        ptcb->TCB_usStatus &= (~LW_THREAD_STATUS_IPC);
    }
        
    ptcb->TCB_ucWaitTimeOut = LW_WAIT_TIME_CLEAR;
    
    if (__LW_THREAD_IS_READY(ptcb)) {                                   /*  检查是否就绪                */
        ptcb->TCB_ucSchedActivate = LW_SCHED_ACT_OTHER;
        __ADD_TO_READY_RING(ptcb, ppcb);                                /*  加入就绪表                  */
    }
    
    KN_INT_ENABLE(iregInterLevel);
    __KERNEL_EXIT();                                                    /*  退出内核 (可能会调度)       */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
