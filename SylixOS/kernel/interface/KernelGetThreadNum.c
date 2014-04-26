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
** 文   件   名: KernelGetThreadNum.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 05 月 11 日
**
** 描        述: 用户可以调用这个 API 获得当前线程数量

** BUG
2008.05.18  使用 __KERNEL_ENTER() 代替 ThreadLock();
2008.05.31  改为关闭中断方式.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: API_KernelGetThreadNum
** 功能描述: 获得当前线程数量
** 输　入  : 
** 输　出  : 当前线程数量
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
UINT16  API_KernelGetThreadNum (VOID)
{
    REGISTER UINT16           usThreadNum;
    
    __KERNEL_ENTER();                                                   /*  进入内核                    */
    usThreadNum = _K_usThreadCounter;
    __KERNEL_EXIT();                                                    /*  退出内核                    */

    return  (usThreadNum);
}
/*********************************************************************************************************
** 函数名称: API_KernelGetThreadNumByPriority
** 功能描述: 获得内核指定优先级的线程数量
** 输　入  : 
** 输　出  : 内核指定优先级的线程数量
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
UINT16  API_KernelGetThreadNumByPriority (UINT8  ucPriority)
{
             INTREG           iregInterLevel;
    REGISTER PLW_CLASS_PCB    ppcb;
    REGISTER UINT16           usThreadNum;

#if LW_CFG_ARG_CHK_EN > 0
    if (_PriorityCheck(ucPriority)) {                                   /*  优先级错误                  */
        _ErrorHandle(ERROR_THREAD_PRIORITY_WRONG);
        return  (0);
    }
#endif
    
    ppcb = _GetPcbByPrio(ucPriority);                                   /*  获得优先级控制块            */
    
    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核同时关闭中断        */
    usThreadNum = ppcb->PCB_usThreadCounter;                            /*  获得该优先级线程数量        */
    __KERNEL_EXIT_IRQ(iregInterLevel);                                  /*  退出内核同时打开中断        */

    return  (usThreadNum);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
