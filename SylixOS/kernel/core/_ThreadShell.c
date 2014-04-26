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
** 文   件   名: _ThreadShell.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 13 日
**
** 描        述: 这是系统线程的外壳。

** BUG
2007.11.04  将 0xFFFFFFFF 改为 __ARCH_ULONG_MAX.
2008.01.16  API_ThreadDelete() -> API_ThreadForceDelete();
2012.03.20  减少对 _K_ptcbTCBCur 的引用, 尽量采用局部变量, 减少对当前 CPU ID 获取的次数.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: _BuidTCB
** 功能描述: 创建一个TCB
** 输　入  : pvThreadStartAddress              线程代码段起始地址
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PVOID  _ThreadShell (PVOID  pvThreadStartAddress)
{
    PLW_CLASS_TCB       ptcbCur;
    PVOID               pvReturnVal;
    LW_OBJECT_HANDLE    ulId;
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);                                       /*  当前任务控制块              */
    
    pvReturnVal = ((PTHREAD_START_ROUTINE)pvThreadStartAddress)
                  (ptcbCur->TCB_pvArg);                                 /*  执行线程                    */

    ulId = ptcbCur->TCB_ulId;

#if LW_CFG_THREAD_DEL_EN > 0
    API_ThreadForceDelete(&ulId, pvReturnVal);                          /*  删除线程                    */
#endif

#if LW_CFG_THREAD_SUSPEND_EN > 0
    API_ThreadSuspend(ulId);                                            /*  阻塞线程                    */
#endif

    for (;;) {
        API_TimeSleep(__ARCH_ULONG_MAX);                                /*  睡眠                        */
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
