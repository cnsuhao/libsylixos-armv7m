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
2014.08.08  加入 __vmmVirtualDesc() 函数.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0
#include "virPage.h"
/*********************************************************************************************************
  虚拟空间描述
  
  注意:  以下地址区域不能与 '内核内存' 和 'ATTR_DMA' 运行的物理地址区间有任何重合.
         同时, 大小最好比所有 zone 空间之和要大, 因为 vmmMalloc 的碎片链接需要占用连续逻辑空间.
         这里默认开辟 1GB 空间, 
         ulVirtualStart 之上的一个虚拟页面保留作为高速数据切换通道.
*********************************************************************************************************/
static LW_MMU_VIRTUAL_DESC  _G_vmvirDesc = {
    0xC0000000,                                                         /*  高速数据切换通道一个虚拟页面*/
    0xC0000000 + LW_CFG_VMM_PAGE_SIZE,                                  /*  start addr : 3GB + 1page    */
    0x3FFF0000                                                          /*  size       : 1GB - 16page   */
};                                                                      /*  虚拟空间                    */
/*********************************************************************************************************
** 函数名称: __vmmVirtualDesc
** 功能描述: 获得虚拟空间区域.
** 输　入  : NONE
** 输　出  : 虚拟空间描述
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_MMU_VIRTUAL_DESC  __vmmVirtualDesc (VOID)
{
    return  (&_G_vmvirDesc);
}
/*********************************************************************************************************
** 函数名称: __vmmVirtualIsInside
** 功能描述: 判断地址是否在虚拟空间中.
** 输　入  : ulAddr        地址
** 输　出  : 是否在 VMM 虚拟空间中
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
BOOL  __vmmVirtualIsInside (addr_t  ulAddr)
{
    if ((ulAddr >= _G_vmvirDesc.ulVirtualStart) &&
        ((ulAddr - _G_vmvirDesc.ulVirtualStart) < _G_vmvirDesc.stSize)) {
        return  (LW_TRUE);
    
    } else {
        return  (LW_FALSE);
    }
}
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
