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
** 文   件   名: _EventInit.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 13 日
**
** 描        述: 这是系统事件初始化函数库。

** BUG:
2009.07.28  加入对自旋锁的初始化.
2013.11.14  使用对象资源管理器结构管理空闲资源.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: _EventInit
** 功能描述: 初始化事件缓冲池
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _EventInit (VOID)
{
#if  (LW_CFG_EVENT_EN > 0) && (LW_CFG_MAX_EVENTS > 0)
    REGISTER ULONG                 ulI;
    REGISTER PLW_CLASS_EVENT       peventTemp1;
    REGISTER PLW_LIST_MONO         pmonoTemp1;
    REGISTER PLW_CLASS_WAITQUEUE   pwqTemp1;

#if  LW_CFG_MAX_EVENTS == 1

    _K_resrcEvent.RESRC_pmonoFreeHeader = &_K_eventBuffer[0].EVENT_monoResrcList;
                                                                            /*  设置资源表头            */
    peventTemp1 = &_K_eventBuffer[0];                                       /*  指向缓冲池首地址        */
    pmonoTemp1  = &peventTemp1->EVENT_monoResrcList;                        /*  获得资源表              */

    peventTemp1->EVENT_ucType       = LW_TYPE_EVENT_UNUSED;                 /*  事件类型                */
    peventTemp1->EVENT_ulCounter    = 0ul;                                  /*  计数器值                */
    peventTemp1->EVENT_ulMaxCounter = 0ul;                                  /*  最大技术值              */
    peventTemp1->EVENT_ulOption     = LW_OPTION_NONE;                       /*  事件选项                */
    peventTemp1->EVENT_pvPtr        = LW_NULL;                              /*  多用途指针              */
    peventTemp1->EVENT_pvTcbOwn     = LW_NULL;                              /*  从属线程                */
    peventTemp1->EVENT_usIndex      = 0;                                    /*  事件缓冲区下标          */
    
    pwqTemp1 = &peventTemp1->EVENT_wqWaitList;                              /*  开始初始化等待队列      */
    pwqTemp1->WAIT_Q_usWaitNum      = 0;                                    /*  没有任务等待事件        */
    
    for (ulI = 0; ulI < __THREAD_PRIORITY_Q_NUM; ulI++) {
        pwqTemp1->WAIT_Q_wlWaitList.WAIT_Q_pringPRIOList[ulI] = LW_NULL;    /*  初始化等待队列          */
    }
    
    LW_SPIN_INIT(&peventTemp1->EVENT_slLock);
    
    _INIT_LIST_MONO_HEAD(pmonoTemp1);                                       /*  初始化最后节点          */
    
    _K_resrcEvent.RESRC_pmonoFreeTail = pmonoTemp1;
    
#else

    REGISTER UINT8              ucJ;
    REGISTER PLW_CLASS_EVENT    peventTemp2;
    REGISTER PLW_LIST_MONO      pmonoTemp2;
    
    _K_resrcEvent.RESRC_pmonoFreeHeader = &_K_eventBuffer[0].EVENT_monoResrcList;
                                                                            /*  设置资源表头            */
    peventTemp1 = &_K_eventBuffer[0];                                       /*  指向缓冲池首地址        */
    peventTemp2 = &_K_eventBuffer[1];                                       /*  指向缓冲池首地址        */
    
    for (ulI = 0; ulI < ((LW_CFG_MAX_EVENTS) - 1); ulI++) {
        
        pmonoTemp1 = &peventTemp1->EVENT_monoResrcList;                     /*  获得资源表              */
        pmonoTemp2 = &peventTemp2->EVENT_monoResrcList;                     /*  获得资源表              */
        
        peventTemp1->EVENT_ucType       = LW_TYPE_EVENT_UNUSED;             /*  事件类型                */
        peventTemp1->EVENT_ulCounter    = 0ul;                              /*  计数器值                */
        peventTemp1->EVENT_ulMaxCounter = 0ul;                              /*  最大技术值              */
        peventTemp1->EVENT_ulOption     = LW_OPTION_NONE;                   /*  事件选项                */
        peventTemp1->EVENT_pvPtr        = LW_NULL;                          /*  多用途指针              */
        peventTemp1->EVENT_pvTcbOwn     = LW_NULL;                          /*  从属线程                */
        peventTemp1->EVENT_usIndex      = (UINT16)ulI;                      /*  事件缓冲区下标          */
        
        pwqTemp1 = &peventTemp1->EVENT_wqWaitList;                          /*  开始初始化等待队列      */
        pwqTemp1->WAITQUEUE_usWaitNum      = 0;                             /*  没有任务等待事件        */
        
        for (ucJ = 0; ucJ < __THREAD_PRIORITY_Q_NUM; ucJ++) {               /*  初始化等待队列          */
            pwqTemp1->WAITQUEUE_wlWaitList.WAITLIST_pringPRIOList[ucJ] = LW_NULL;
        }
        
        LW_SPIN_INIT(&peventTemp1->EVENT_slLock);
   
        _LIST_MONO_LINK(pmonoTemp1, pmonoTemp2);                            /*  建立资源连接            */
        
        peventTemp1++;
        peventTemp2++;
        
    }
                                                                            /*  初始化最后一个节点      */
    pmonoTemp1 = &peventTemp1->EVENT_monoResrcList;                         /*  获得资源表              */
    
    peventTemp1->EVENT_ucType       = LW_TYPE_EVENT_UNUSED;                 /*  事件类型                */
    peventTemp1->EVENT_ulCounter    = 0ul;                                  /*  计数器值                */
    peventTemp1->EVENT_ulMaxCounter = 0ul;                                  /*  最大技术值              */
    peventTemp1->EVENT_ulOption     = LW_OPTION_NONE;                       /*  事件选项                */
    peventTemp1->EVENT_pvPtr        = LW_NULL;                              /*  多用途指针              */
    peventTemp1->EVENT_pvTcbOwn     = LW_NULL;                              /*  从属线程                */
    peventTemp1->EVENT_usIndex      = (UINT16)ulI;                          /*  事件缓冲区下标          */

    pwqTemp1 = &peventTemp1->EVENT_wqWaitList;                              /*  开始初始化等待队列      */
    pwqTemp1->WAITQUEUE_usWaitNum      = 0;                                 /*  没有任务等待事件        */
    
    for (ucJ = 0; ucJ < __THREAD_PRIORITY_Q_NUM; ucJ++) {                   /*  初始化等待队列          */
        pwqTemp1->WAITQUEUE_wlWaitList.WAITLIST_pringPRIOList[ucJ] = LW_NULL;
    }
    
    LW_SPIN_INIT(&peventTemp1->EVENT_slLock);
    
    _INIT_LIST_MONO_HEAD(pmonoTemp1);                                       /*  初始化最后节点          */
    
    _K_resrcEvent.RESRC_pmonoFreeTail = pmonoTemp1;
#endif                                                                      /*  LW_CFG_MAX_EVENTS == 1  */

    _K_resrcEvent.RESRC_uiUsed    = 0;
    _K_resrcEvent.RESRC_uiMaxUsed = 0;

#endif                                                                      /*  (LW_CFG_EVENT_EN > 0)   */
                                                                            /*  (LW_CFG_MAX_EVENTS > 0) */
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
