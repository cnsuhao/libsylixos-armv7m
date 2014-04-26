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
** 文   件   名: virPage.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 12 月 09 日
**
** 描        述: 虚拟内存管理.

** BUG:
2013.05.24  加入虚拟空间对齐开辟.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0
#include "virPage.h"
/*********************************************************************************************************
** 函数名称: __vmmVirtualCreate
** 功能描述: 创建虚拟空间区域.
** 输　入  : ulAddr            起始地址
**           stSize            虚拟空间大小
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
ULONG  __vmmVirtualCreate (addr_t  ulAddr, size_t  stSize)
{
    REGISTER PLW_MMU_CONTEXT  pmmuctx = __vmmGetCurCtx();
    REGISTER ULONG            ulError;

    ulError = __pageZoneCreate(&pmmuctx->MMUCTX_vmzoneVirtual, ulAddr, stSize, LW_ZONE_ATTR_NONE,
                               __VMM_PAGE_TYPE_VIRTUAL);
    if (ulError) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "kernel low memory.\r\n");
        return  (ulError);
    
    } else {
        _ErrorHandle(ERROR_NONE);
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: __vmmVirtualPageAlloc
** 功能描述: 分配指定的连续虚拟页面
** 输　入  : ulPageNum     需要分配的虚拟页面个数
** 输　出  : 页面控制块
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_VMM_PAGE  __vmmVirtualPageAlloc (ULONG  ulPageNum)
{
    REGISTER PLW_MMU_CONTEXT  pmmuctx = __vmmGetCurCtx();

    return  (__pageAllocate(&pmmuctx->MMUCTX_vmzoneVirtual, ulPageNum, 
                            __VMM_PAGE_TYPE_VIRTUAL));
}
/*********************************************************************************************************
** 函数名称: __vmmVirtualPageAllocAlign
** 功能描述: 分配指定的连续虚拟页面 (指定对齐关系)
** 输　入  : ulPageNum     需要分配的虚拟页面个数
**           stAlign       内存对齐关系
** 输　出  : 页面控制块
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_VMM_PAGE  __vmmVirtualPageAllocAlign (ULONG  ulPageNum, size_t  stAlign)
{
    REGISTER PLW_MMU_CONTEXT  pmmuctx = __vmmGetCurCtx();
    
    return  (__pageAllocateAlign(&pmmuctx->MMUCTX_vmzoneVirtual, ulPageNum, 
                                 stAlign, __VMM_PAGE_TYPE_VIRTUAL));
}
/*********************************************************************************************************
** 函数名称: __vmmVirtualPageFree
** 功能描述: 回收指定的连虚拟页面
** 输　入  : pvmpage       页面控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __vmmVirtualPageFree (PLW_VMM_PAGE  pvmpage)
{
    REGISTER PLW_MMU_CONTEXT  pmmuctx = __vmmGetCurCtx();

    __pageFree(&pmmuctx->MMUCTX_vmzoneVirtual, pvmpage);
}
/*********************************************************************************************************
** 函数名称: __vmmVirtualPageGetMinContinue
** 功能描述: 获得虚拟页面内, 最小连续分页的个数
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
ULONG  __vmmVirtualPageGetMinContinue (VOID)
{
    REGISTER PLW_MMU_CONTEXT  pmmuctx = __vmmGetCurCtx();

    return  (__pageGetMinContinue(&pmmuctx->MMUCTX_vmzoneVirtual));
}
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
