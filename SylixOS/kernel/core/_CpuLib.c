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
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
** 注  意  : 必须保证 pcpu 当前执行线程有一个有效的 TCB 例如 _K_tcbDummyKernel 或者其他
*********************************************************************************************************/
VOID  _CpuActive (PLW_CLASS_CPU   pcpu)
{
    PLW_CLASS_TCB   ptcbOrg;

    if (LW_CPU_IS_ACTIVE(pcpu)) {
        return;
    }
    
    LW_TCB_GET_CUR(ptcbOrg);
    
    LW_SPIN_LOCK_IGNIRQ(&_K_slScheduler);
    
    pcpu->CPU_ulStatus |= LW_CPU_STATUS_ACTIVE;
    KN_SMP_MB();
    
    _CandTableUpdate(pcpu);                                             /*  更新候选线程                */

    pcpu->CPU_ptcbTCBCur  = LW_CAND_TCB(pcpu);
    pcpu->CPU_ptcbTCBHigh = LW_CAND_TCB(pcpu);
    
    LW_SPIN_UNLOCK_SCHED(&_K_slScheduler, ptcbOrg);
}
/*********************************************************************************************************
** 函数名称: _CpuInactive
** 功能描述: 将 CPU 设置为非激活状态 (关闭中断状态下被调用)
** 输　入  : pcpu      CPU 结构
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _CpuInactive (PLW_CLASS_CPU   pcpu)
{
    PLW_CLASS_TCB   ptcbOrg;

    if (!LW_CPU_IS_ACTIVE(pcpu)) {
        return;
    }
    
    LW_TCB_GET_CUR(ptcbOrg);
    
    LW_SPIN_LOCK_IGNIRQ(&_K_slScheduler);
    
    pcpu->CPU_ulStatus &= ~LW_CPU_STATUS_ACTIVE;
    KN_SMP_MB();
    
    _CandTableRemove(pcpu);                                             /*  移除候选执行线程            */
    LW_CAND_ROT(pcpu) = LW_FALSE;                                       /*  清除优先级卷绕标志          */

    pcpu->CPU_ptcbTCBCur  = LW_NULL;
    pcpu->CPU_ptcbTCBHigh = LW_NULL;
    
    LW_SPIN_UNLOCK_SCHED(&_K_slScheduler, ptcbOrg);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
