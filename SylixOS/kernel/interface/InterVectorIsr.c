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
** 文   件   名: InterVectorIsr.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 02 月 02 日
**
** 描        述: 向量中断总服务

** BUG
2007.06.06  加入最多嵌套信息记录功能。
2010.08.03  使用 interrupt vector spinlock 作为多核锁.
2011.03.31  加入 vector queue 类型中断向量支持.
2013.07.19  加入核间中断处理分支.
2013.08.28  加入内核事件监视器.
2014.04.21  中断被处理后不在遍历后面的中断服务函数.
2014.05.09  加入对中断计数器的处理.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: API_InterVectorIsr
** 功能描述: 向量中断总服务
** 输　入  : ulVector                      中断向量号 (arch 层函数需要保证此参数正确)
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
** 注  意  : 这里并不处理中断嵌套, 他需要 arch 层移植函数支持.
                                           API 函数
*********************************************************************************************************/
LW_API
VOID  API_InterVectorIsr (ULONG  ulVector)
{
    INTREG              iregInterLevel;
    PLW_CLASS_CPU       pcpu;
    
    PLW_LIST_LINE       plineTemp;
    PLW_CLASS_INTDESC   pidesc;
    PLW_CLASS_INTACT    piaction;
    irqreturn_t         irqret;
           
    pcpu = LW_CPU_GET_CUR();                                            /*  中断处理程序中, 不会改变 CPU*/
    
    __LW_CPU_INT_ENTER_HOOK(ulVector, pcpu->CPU_ulInterNesting);
    
    MONITOR_EVT_LONG2(MONITOR_EVENT_ID_INT, MONITOR_EVENT_INT_ENTER, 
                      ulVector, pcpu->CPU_ulInterNesting, LW_NULL);
    
#if LW_CFG_SMP_EN > 0
    if ((pcpu->CPU_ulIPIVector != __ARCH_ULONG_MAX) && 
        (pcpu->CPU_ulIPIVector == ulVector)) {                          /*  核间中断                    */
        _SmpProcIpi(pcpu);
    }
#endif                                                                  /*  LW_CFG_SMP_EN               */

    pidesc = LW_IVEC_GET_IDESC(ulVector);
    
    LW_SPIN_LOCK(&pidesc->IDESC_slLock);                                /*  锁住 spinlock               */
    
    for (plineTemp  = pidesc->IDESC_plineAction;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
        
        piaction = _LIST_ENTRY(plineTemp, LW_CLASS_INTACT, IACT_plineManage);
        if (piaction->IACT_pfuncIsr) {
            irqret = piaction->IACT_pfuncIsr(piaction->IACT_pvArg, ulVector);
            if (LW_IRQ_RETVAL(irqret)) {                                /*  中断是否已经被处理          */
                piaction->IACT_iIntCnt[pcpu->CPU_ulCPUId]++;
                if (piaction->IACT_pfuncClear) {
                    piaction->IACT_pfuncClear(piaction->IACT_pvArg, ulVector);
                }
                break;
            }
        }
    }
    
#if LW_CFG_INTER_INFO > 0
    iregInterLevel = KN_INT_DISABLE();
    if (pcpu->CPU_ulInterNestingMax < pcpu->CPU_ulInterNesting) {
        pcpu->CPU_ulInterNestingMax = pcpu->CPU_ulInterNesting;
    }
    KN_INT_ENABLE(iregInterLevel);
#endif
    
    LW_SPIN_UNLOCK(&pidesc->IDESC_slLock);                              /*  解锁 spinlock               */
    
    __LW_CPU_INT_EXIT_HOOK(ulVector, pcpu->CPU_ulInterNesting);
    
    MONITOR_EVT_LONG2(MONITOR_EVENT_ID_INT, MONITOR_EVENT_INT_EXIT, 
                      ulVector, pcpu->CPU_ulInterNesting, LW_NULL);
}
/*********************************************************************************************************
** 函数名称: API_InterVectorIpi
** 功能描述: 设置核间中断向量   
             BSP 在系统启动前调用此函数, 每个 CPU 都要设置, SylixOS 允许不同的 CPU 核间中断向量不同.
** 输　入  : 
**           ulCPUId                       CPU ID
**           ulIPIVector                   核间中断向量号
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if LW_CFG_SMP_EN > 0

LW_API
VOID  API_InterVectorIpi (ULONG  ulCPUId, ULONG  ulIPIVector)
{
    if (ulCPUId < LW_CFG_MAX_PROCESSORS) {
        LW_CPU_GET(ulCPUId)->CPU_ulIPIVector = ulIPIVector;
    }
}

#endif                                                                  /*  LW_CFG_SMP_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
