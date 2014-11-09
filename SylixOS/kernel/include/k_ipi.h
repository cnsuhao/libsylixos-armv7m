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
** 文   件   名: k_ipi.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 07 月 19 日
**
** 描        述: SMP 系统核间中断.
*********************************************************************************************************/

#ifndef __K_IPI_H
#define __K_IPI_H

/*********************************************************************************************************
  核间中断自定义消息类型
  
  同步执行函数为 IPI Call 必须等待其执行完毕才能退出, 异步执行函数为不需要等待的函数.
*********************************************************************************************************/

typedef struct __ipi_msg {
    LW_LIST_RING     IPIM_ringManage;                                   /*  管理链表                    */
    FUNCPTR          IPIM_pfuncCall;                                    /*  同步执行函数                */
    PVOID            IPIM_pvArg;                                        /*  同步执行参数                */
    VOIDFUNCPTR      IPIM_pfuncAsyncCall;                               /*  异步执行函数                */
    PVOID            IPIM_pvAsyncArg;                                   /*  异步执行参数                */
    INT              IPIM_iRet;                                         /*  同步执行函数返回值          */
    volatile INT     IPIM_iWait;                                        /*  等待信息                    */
} LW_IPI_MSG;
typedef LW_IPI_MSG  *PLW_IPI_MSG;

/*********************************************************************************************************
  核间中断短等待时间
*********************************************************************************************************/

#define LW_SPINLOCK_DELAY() \
        {   volatile INT i; \
            for (i = 0; i < 10; i++);    \
        }

#endif                                                                  /*  __K_IPI_H                   */
/*********************************************************************************************************
  END
*********************************************************************************************************/
