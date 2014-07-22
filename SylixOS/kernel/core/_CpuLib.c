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
** 文   件   名: _CpuLib.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2014 年 01 月 10 日
**
** 描        述: CPU 操作库.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: _CpuActive
** 功能描述: 将 CPU 设置为激活状态 (关闭中断状态下被调用)
** 输　入  : pcpu      CPU 结构
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
** 注  意  : 必须保证 pcpu 当前执行线程有一个有效的 TCB 例如 _K_tcbDummyKernel 或者其他
*********************************************************************************************************/
INT  _CpuActive (PLW_CLASS_CPU   pcpu)
{
    PLW_CLASS_TCB   ptcbOrg;

    if (LW_CPU_IS_ACTIVE(pcpu)) {
        return  (PX_ERROR);
    }
    
    LW_TCB_GET_CUR(ptcbOrg);
    
    LW_SPIN_LOCK_IGNIRQ(&_K_slScheduler);
    
    pcpu->CPU_ulStatus |= LW_CPU_STATUS_ACTIVE;
    KN_SMP_MB();
    
    _CandTableUpdate(pcpu);                                             /*  更新候选线程                */

    pcpu->CPU_ptcbTCBCur  = LW_CAND_TCB(pcpu);
    pcpu->CPU_ptcbTCBHigh = LW_CAND_TCB(pcpu);
    
    LW_SPIN_UNLOCK_SCHED(&_K_slScheduler, ptcbOrg);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: _CpuInactiveNoLock
** 功能描述: 将 CPU 设置为非激活状态 (锁定调度器并关闭中断状态下被调用)
** 输　入  : pcpu      CPU 结构
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  _CpuInactiveNoLock (PLW_CLASS_CPU   pcpu)
{
    INT             i;
    ULONG           ulCPUId = pcpu->CPU_ulCPUId;
    PLW_CLASS_TCB   ptcb;

    if (!LW_CPU_IS_ACTIVE(pcpu)) {
        return  (PX_ERROR);
    }
    
    ptcb = LW_CAND_TCB(pcpu);
    
    pcpu->CPU_ulStatus &= ~LW_CPU_STATUS_ACTIVE;
    KN_SMP_MB();
    
    _CandTableRemove(pcpu);                                             /*  移除候选执行线程            */
    LW_CAND_ROT(pcpu) = LW_FALSE;                                       /*  清除优先级卷绕标志          */

    pcpu->CPU_ptcbTCBCur  = LW_NULL;
    pcpu->CPU_ptcbTCBHigh = LW_NULL;
    
    for (i = 0; i < LW_NCPUS; i++) {                                    /*  请求其他 CPU 调度           */
        if (ulCPUId != i) {
            PLW_CLASS_CPU   pcpuOther = LW_CPU_GET(i);
            PLW_CLASS_TCB   ptcbCand  = LW_CAND_TCB(pcpuOther);
            if (LW_PRIO_IS_HIGH(ptcb->TCB_ucPriority,
                                ptcbCand->TCB_ucPriority)) {            /*  当前退出的任务优先级高      */
                LW_CAND_ROT(pcpuOther) = LW_TRUE;
            }
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
