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
** 文   件   名: CpuActive.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2014 年 07 月 21 日
**
** 描        述: SMP 系统开启/关闭一个 CPU.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_SMP_EN > 0
/*********************************************************************************************************
** 函数名称: API_CpuUp
** 功能描述: 开启一个 CPU. (非 0 号 CPU)
** 输　入  : ulCPUId       CPU ID
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_CpuUp (ULONG  ulCPUId)
{
    INTREG          iregInterLevel;
    PLW_CLASS_CPU   pcpu;

    if ((ulCPUId == 0) || (ulCPUId >= LW_NCPUS)) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    pcpu = LW_CPU_GET(ulCPUId);
    
    KN_SMP_MB();
    if (LW_CPU_IS_ACTIVE(pcpu)) {
        return  (ERROR_NONE);
    }
    
    iregInterLevel = KN_INT_DISABLE();
    bspCpuUp(ulCPUId);
    KN_INT_ENABLE(iregInterLevel);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_CpuDown
** 功能描述: 关闭一个 CPU. (非 0 号 CPU)
** 输　入  : ulCPUId       CPU ID
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_CpuDown (ULONG  ulCPUId)
{
    PLW_CLASS_CPU   pcpu;

    if ((ulCPUId == 0) || (ulCPUId >= LW_NCPUS)) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    pcpu = LW_CPU_GET(ulCPUId);
    
    KN_SMP_MB();
    if (!LW_CPU_IS_ACTIVE(pcpu)) {
        return  (ERROR_NONE);
    }
    
    _SmpSendIpi(ulCPUId, LW_IPI_DOWN, 1);                               /*  使用核间中断通知 CPU 停止   */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_CpuIsUp
** 功能描述: 指定 CPU 是否启动.
** 输　入  : ulCPUId       CPU ID
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
BOOL  API_CpuIsUp (ULONG  ulCPUId)
{
    PLW_CLASS_CPU   pcpu;
    
    if (ulCPUId >= LW_NCPUS) {
        _ErrorHandle(EINVAL);
        return  (LW_FALSE);
    }
    
    pcpu = LW_CPU_GET(ulCPUId);
    
    KN_SMP_MB();
    if (LW_CPU_IS_ACTIVE(pcpu)) {
        return  (LW_TRUE);
    
    } else {
        return  (LW_FALSE);
    }
}

#endif                                                                  /*  LW_CFG_SMP_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
