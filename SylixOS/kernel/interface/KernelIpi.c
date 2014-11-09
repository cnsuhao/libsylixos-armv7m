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
** 文   件   名: KernelIpi.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2014 年 11 月 08 日
**
** 描        述: SMP 系统核间中断.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: API_KernelSmpCall
** 功能描述: 指定 CPU 调用指定函数
** 输　入  : ulCPUId       CPU ID
**           pfunc         同步执行函数
**           pvArg         同步参数
**           pfuncAsync    异步执行函数
**           pvAsync       异步执行参数
** 输　出  : 同步调用返回值
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if LW_CFG_SMP_EN > 0

LW_API  
INT  API_KernelSmpCall (ULONG        ulCPUId, 
                        FUNCPTR      pfunc, 
                        PVOID        pvArg,
                        VOIDFUNCPTR  pfuncAsync,
                        PVOID        pvAsync)
{
    INTREG      iregInterLevel;
    INT         iRet;
    
    iregInterLevel = KN_INT_DISABLE();
    
    if (ulCPUId == LW_CPU_GET_CUR_ID()) {                               /*  不能自己调用自己            */
        KN_INT_ENABLE(iregInterLevel);
        return  (PX_ERROR);
    }
    
    iRet = _SmpCallFunc(ulCPUId, pfunc, pvArg, pfuncAsync, pvAsync);
    
    KN_INT_ENABLE(iregInterLevel);
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: API_KernelSmpCallAllOther
** 功能描述: 其他所有激活的 CPU 调用指定函数
** 输　入  : pfunc         同步执行函数
**           pvArg         同步参数
**           pfuncAsync    异步执行函数
**           pvAsync       异步执行参数
** 输　出  : NONE (无法确定返回值)
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
VOID  API_KernelSmpCallAllOther (FUNCPTR      pfunc, 
                                 PVOID        pvArg,
                                 VOIDFUNCPTR  pfuncAsync,
                                 PVOID        pvAsync)
{
    INTREG      iregInterLevel;
    
    iregInterLevel = KN_INT_DISABLE();
    
    _SmpCallFuncAllOther(pfunc, pvArg, pfuncAsync, pvAsync);
    
    KN_INT_ENABLE(iregInterLevel);
}

#endif                                                                  /*  LW_CFG_SMP_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
