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
** 文   件   名: phyPage.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 12 月 09 日
**
** 描        述: 物理页面管理.

** BUG
2009.03.05  __vmmPhysicalGetZone() 对连续域的定位错误.
2009.06.23  加入物理内存对齐开辟功能.
2009.11.13  假如获取碎片物理内存大小接口.
            同时允许从指定的 zone 中分配物理内存.
2011.03.02  加入 __vmmPhysicalPageSetListFlag() 函数.
2011.05.19  加入物理页面链遍历功能 __vmmPhysicalPageTraversalList().
2011.08.11  物理页面分配优先使用属相相同的分区, 这样可以避免 DMA 分区被其他分配浪费.
2013.05.30  加入物理页面引用功能.
2014.07.27  加入物理页面 CACHE 操作功能.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0
#include "phyPage.h"
#include "virPage.h"
/*********************************************************************************************************
  物理 zone 控制块数组
*********************************************************************************************************/
LW_VMM_ZONE      _G_vmzonePhysical[LW_CFG_VMM_ZONE_NUM];                /*  物理区域                    */
/*********************************************************************************************************
** 函数名称: __vmmPhysicalCreate
** 功能描述: 创建一个物理分页区域.
** 输　入  : ulZoneIndex       物理区域编号
**           ulAddr            起始地址
**           stSize            物理区域大小
**           uiAttr            物理区域属性
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
ULONG  __vmmPhysicalCreate (ULONG  ulZoneIndex, addr_t  ulAddr, size_t  stSize, UINT  uiAttr)
{
    REGISTER ULONG  ulError;

    ulError = __pageZoneCreate(&_G_vmzonePhysical[ulZoneIndex], ulAddr, stSize, uiAttr,
                               __VMM_PAGE_TYPE_PHYSICAL);
    if (ulError) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "kernel low memory.\r\n");
        return  (ulError);
    
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageAlloc
** 功能描述: 分配指定的连续物理页面
** 输　入  : ulPageNum     需要分配的物理页面个数
**           uiAttr        需要满足的物理页面属性
**           pulZoneIndex  物理 zone 下标
** 输　出  : 页面控制块
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_VMM_PAGE  __vmmPhysicalPageAlloc (ULONG  ulPageNum, UINT  uiAttr, ULONG  *pulZoneIndex)
{
             INT            i;
    REGISTER PLW_VMM_ZONE   pvmzone;
    REGISTER PLW_VMM_PAGE   pvmpage = LW_NULL;
    
    for (i = 0; i < LW_CFG_VMM_ZONE_NUM; i++) {                         /*  优先选择 uiAttr 相同的 zone */
        pvmzone = &_G_vmzonePhysical[i];
        if (pvmzone && (pvmzone->ZONE_uiAttr == uiAttr)) {
            if (pvmzone->ZONE_ulFreePage >= ulPageNum) {
                pvmpage = __pageAllocate(pvmzone, ulPageNum, __VMM_PAGE_TYPE_PHYSICAL);
                if (pvmpage) {
                    *pulZoneIndex = (ULONG)i;
                    return  (pvmpage);
                }
            }
        }
    }
    
    for (i = 0; i < LW_CFG_VMM_ZONE_NUM; i++) {
        pvmzone = &_G_vmzonePhysical[i];
        if (pvmzone && ((pvmzone->ZONE_uiAttr & uiAttr) == uiAttr)) {   /*  其次选择拥有 uiAttr 的 zone */
            if (pvmzone->ZONE_ulFreePage >= ulPageNum) {
                pvmpage = __pageAllocate(pvmzone, ulPageNum, __VMM_PAGE_TYPE_PHYSICAL);
                if (pvmpage) {
                    *pulZoneIndex = (ULONG)i;
                    return  (pvmpage);
                }
            }
        }
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageAllocZone
** 功能描述: 分配指定的连续物理页面 (指定物理区域)
** 输　入  : ulZoneIndex   物理区域
**           ulPageNum     需要分配的物理页面个数
**           uiAttr        需要满足的页面属性
** 输　出  : 页面控制块
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_VMM_PAGE  __vmmPhysicalPageAllocZone (ULONG  ulZoneIndex, ULONG  ulPageNum, UINT  uiAttr)
{
    REGISTER PLW_VMM_ZONE   pvmzone;
    REGISTER PLW_VMM_PAGE   pvmpage = LW_NULL;
    
    if (ulZoneIndex >= LW_CFG_VMM_ZONE_NUM) {
        return  (LW_NULL);
    }
        
    pvmzone = &_G_vmzonePhysical[ulZoneIndex];
    if (pvmzone && ((pvmzone->ZONE_uiAttr & uiAttr) == uiAttr)) {
        if (pvmzone->ZONE_ulFreePage >= ulPageNum) {
            pvmpage = __pageAllocate(pvmzone, ulPageNum, __VMM_PAGE_TYPE_PHYSICAL);
            if (pvmpage) {
                return  (pvmpage);
            }
        }
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageAllocAlign
** 功能描述: 分配指定的连续物理页面 (可指定对齐关系)
** 输　入  : ulPageNum     需要分配的物理页面个数
**           stAlign       内存对齐关系
**           uiAttr        需要满足的物理页面属性
**           pulZoneIndex  物理 zone 下标
** 输　出  : 页面控制块
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_VMM_PAGE  __vmmPhysicalPageAllocAlign (ULONG   ulPageNum, 
                                           size_t  stAlign, 
                                           UINT    uiAttr, 
                                           ULONG  *pulZoneIndex)
{
             INT            i;
    REGISTER PLW_VMM_ZONE   pvmzone;
    REGISTER PLW_VMM_PAGE   pvmpage = LW_NULL;
    
    for (i = 0; i < LW_CFG_VMM_ZONE_NUM; i++) {
        pvmzone = &_G_vmzonePhysical[i];
        if (pvmzone && (pvmzone->ZONE_uiAttr == uiAttr)) {              /*  优先选择 uiAttr 相同的 zone */
            if (pvmzone->ZONE_ulFreePage >= ulPageNum) {
                pvmpage = __pageAllocateAlign(pvmzone, ulPageNum, stAlign, __VMM_PAGE_TYPE_PHYSICAL);
                if (pvmpage) {
                    *pulZoneIndex = (ULONG)i;
                    return  (pvmpage);
                }
            }
        }
    }
    
    for (i = 0; i < LW_CFG_VMM_ZONE_NUM; i++) {
        pvmzone = &_G_vmzonePhysical[i];
        if (pvmzone && ((pvmzone->ZONE_uiAttr & uiAttr) == uiAttr)) {   /*  其次选择拥有 uiAttr 的 zone */
            if (pvmzone->ZONE_ulFreePage >= ulPageNum) {
                pvmpage = __pageAllocateAlign(pvmzone, ulPageNum, stAlign, __VMM_PAGE_TYPE_PHYSICAL);
                if (pvmpage) {
                    *pulZoneIndex = (ULONG)i;
                    return  (pvmpage);
                }
            }
        }
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageClone
** 功能描述: 克隆一个物理页面
** 输　入  : pvmpage       页面控制块
** 输　出  : 新的物理页面
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_VMM_PAGE  __vmmPhysicalPageClone (PLW_VMM_PAGE  pvmpage)
{
    PLW_VMM_PAGE            pvmpageNew;
    PLW_MMU_VIRTUAL_DESC    pvirdesc = __vmmVirtualDesc();
    ULONG                   ulZoneIndex;
    ULONG                   ulError;
    
    if ((pvmpage->PAGE_ulCount != 1) ||
        (pvmpage->PAGE_iPageType != __VMM_PAGE_TYPE_PHYSICAL) ||
        (pvmpage->PAGE_ulMapPageAddr == PAGE_MAP_ADDR_INV)) {           /*  必须是有映射关系的单页面    */
        _ErrorHandle(ERROR_VMM_PHYSICAL_PAGE);
        return  (LW_NULL);
    }
    
    pvmpageNew = __vmmPhysicalPageAlloc(1, LW_ZONE_ATTR_NONE, &ulZoneIndex);
    if (pvmpageNew == LW_NULL) {
        _ErrorHandle(ERROR_VMM_LOW_PHYSICAL_PAGE);
        return  (LW_NULL);
    }
    
    ulError = __vmmLibPageMap(pvmpageNew->PAGE_ulPageAddr,              /*  使用 CACHE 操作             */
                              pvirdesc->ulVirtualSwitch, 1,             /*  缓冲区虚拟地址              */
                              LW_VMM_FLAG_RDWR);                        /*  映射指定的虚拟地址          */
    if (ulError) {
        __vmmPhysicalPageFree(pvmpageNew);
        return  (LW_NULL);
    }
    
    KN_COPY_PAGE((PVOID)pvirdesc->ulVirtualSwitch, 
                 (PVOID)pvmpage->PAGE_ulMapPageAddr);                   /*  拷贝页面内容                */
               
    if (API_CacheLocation(DATA_CACHE) == CACHE_LOCATION_VIVT) {         /*  如果是虚拟地址 cache        */
        API_CacheClear(DATA_CACHE, (PVOID)pvirdesc->ulVirtualSwitch,
                       LW_CFG_VMM_PAGE_SIZE);                           /*  将数据写入内存并不再命中    */
    }
    
    __vmmLibSetFlag(pvirdesc->ulVirtualSwitch, LW_VMM_FLAG_FAIL);       /*  VIRTUAL_SWITCH 不允许访问   */
    
    return  (pvmpageNew);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageRef
** 功能描述: 引用物理页面
** 输　入  : pvmpage       页面控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_VMM_PAGE  __vmmPhysicalPageRef (PLW_VMM_PAGE  pvmpage)
{
    PLW_VMM_PAGE    pvmpageFake = (PLW_VMM_PAGE)__KHEAP_ALLOC(sizeof(LW_VMM_PAGE));
    PLW_VMM_PAGE    pvmpageReal;
    
    if (pvmpageFake == LW_NULL) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "kernel low memory.\r\n");
        _ErrorHandle(ERROR_KERNEL_LOW_MEMORY);                          /*  缺少内核内存                */
        return  (LW_NULL);
    }
    
    *pvmpageFake = *pvmpage;
    
    if (LW_VMM_PAGE_IS_FAKE(pvmpage)) {
        pvmpageReal = LW_VMM_PAGE_GET_REAL(pvmpage);
    
    } else {
        pvmpageReal = pvmpage;
    }
    
    pvmpageReal->PAGE_ulRef++;                                          /*  真实页面引用++              */
    
    _INIT_LIST_LINE_HEAD(&pvmpageFake->PAGE_lineFreeHash);
    _INIT_LIST_LINE_HEAD(&pvmpageFake->PAGE_lineManage);
    
    pvmpageFake->PAGE_ulRef         = 0ul;                              /*  fake 页面, 引用计数无效     */
    pvmpageFake->PAGE_pvmpageReal   = pvmpageReal;
    pvmpageFake->PAGE_ulMapPageAddr = PAGE_MAP_ADDR_INV;
    
    return  (pvmpageFake);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageFree
** 功能描述: 回收指定的连续物理页面
** 输　入  : pvmpage       页面控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __vmmPhysicalPageFree (PLW_VMM_PAGE  pvmpage)
{
    REGISTER PLW_VMM_ZONE   pvmzone = pvmpage->PAGE_pvmzoneOwner;
             PLW_VMM_PAGE   pvmpageReal;
    
    if (LW_VMM_PAGE_IS_FAKE(pvmpage)) {
        pvmpageReal = LW_VMM_PAGE_GET_REAL(pvmpage);
        __KHEAP_FREE(pvmpage);                                          /*  释放 fake 控制块            */
    
    } else {
        pvmpageReal = pvmpage;
        pvmpageReal->PAGE_ulMapPageAddr = PAGE_MAP_ADDR_INV;            /*  对应的地址不再有效          */
    }
    
    pvmpageReal->PAGE_ulRef--;
    
    if (pvmpageReal->PAGE_ulRef == 0) {
        __pageFree(pvmzone, pvmpageReal);                               /*  没有引用则直接释放          */
    }
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageFreeAll
** 功能描述: 回收指定虚拟空间的所有物理页面
** 输　入  : pvmpageVirtual        虚拟空间
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
** 注  意  : 这里没有为每一个物理页面调用 __pageUnlink 因为调用此函数后, 虚拟空间即将被释放.
*********************************************************************************************************/
VOID  __vmmPhysicalPageFreeAll (PLW_VMM_PAGE  pvmpageVirtual)
{
    __pageTraversalLink(pvmpageVirtual, __vmmPhysicalPageFree, 
                        LW_NULL, LW_NULL, LW_NULL, LW_NULL, LW_NULL, LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageSetFlag
** 功能描述: 设置物理页面的 flag 标志
** 输　入  : pvmpage       页面控制块
**           ulFlag        flag 标志
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __vmmPhysicalPageSetFlag (PLW_VMM_PAGE  pvmpage, ULONG  ulFlag)
{
    pvmpage->PAGE_ulFlags = ulFlag;
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageSetFlagAll
** 功能描述: 设置虚拟空间内所有物理页面的 flag 标志
** 输　入  : pvmpageVirtual 虚拟空间
**           ulFlag         flag 标志
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __vmmPhysicalPageSetFlagAll (PLW_VMM_PAGE  pvmpageVirtual, ULONG  ulFlag)
{
    __pageTraversalLink(pvmpageVirtual, __vmmPhysicalPageSetFlag, (PVOID)ulFlag,
                        LW_NULL, LW_NULL, LW_NULL, LW_NULL, LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageFlush
** 功能描述: 设置物理页面所有 CACHE 回写
** 输　入  : pvmpage       页面控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_CACHE_EN > 0

VOID  __vmmPhysicalPageFlush (PLW_VMM_PAGE  pvmpage)
{
    if (pvmpage->PAGE_ulMapPageAddr != PAGE_MAP_ADDR_INV) {
        API_CacheFlushPage(DATA_CACHE, 
                           (PVOID)pvmpage->PAGE_ulMapPageAddr, 
                           (PVOID)pvmpage->PAGE_ulPageAddr,
                           (size_t)(pvmpage->PAGE_ulCount * LW_CFG_VMM_PAGE_SIZE));
    }
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageFlushAll
** 功能描述: 使虚拟空间内所有物理页面回写
** 输　入  : pvmpageVirtual 虚拟空间
**           ulFlag         flag 标志
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __vmmPhysicalPageFlushAll (PLW_VMM_PAGE  pvmpageVirtual)
{
    __pageTraversalLink(pvmpageVirtual, __vmmPhysicalPageFlush,
                        LW_NULL, LW_NULL, LW_NULL, LW_NULL, LW_NULL, LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageInvalidate
** 功能描述: 设置物理页面所有 CACHE 无效
** 输　入  : pvmpage       页面控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __vmmPhysicalPageInvalidate (PLW_VMM_PAGE  pvmpage)
{
    if (pvmpage->PAGE_ulMapPageAddr != PAGE_MAP_ADDR_INV) {
        API_CacheInvalidatePage(DATA_CACHE, 
                                (PVOID)pvmpage->PAGE_ulMapPageAddr, 
                                (PVOID)pvmpage->PAGE_ulPageAddr,
                                (size_t)(pvmpage->PAGE_ulCount * LW_CFG_VMM_PAGE_SIZE));
    }
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageInvalidateAll
** 功能描述: 使虚拟空间内所有物理页面无效
** 输　入  : pvmpageVirtual 虚拟空间
**           ulFlag         flag 标志
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __vmmPhysicalPageInvalidateAll (PLW_VMM_PAGE  pvmpageVirtual)
{
    __pageTraversalLink(pvmpageVirtual, __vmmPhysicalPageInvalidate,
                        LW_NULL, LW_NULL, LW_NULL, LW_NULL, LW_NULL, LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageClear
** 功能描述: 设置物理页面所有 CACHE 回写并无效
** 输　入  : pvmpage       页面控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __vmmPhysicalPageClear (PLW_VMM_PAGE  pvmpage)
{
    if (pvmpage->PAGE_ulMapPageAddr != PAGE_MAP_ADDR_INV) {
        API_CacheClearPage(DATA_CACHE, 
                           (PVOID)pvmpage->PAGE_ulMapPageAddr, 
                           (PVOID)pvmpage->PAGE_ulPageAddr,
                           (size_t)(pvmpage->PAGE_ulCount * LW_CFG_VMM_PAGE_SIZE));
    }
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageClearAll
** 功能描述: 使虚拟空间内所有物理页面回写并无效
** 输　入  : pvmpageVirtual 虚拟空间
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __vmmPhysicalPageClearAll (PLW_VMM_PAGE  pvmpageVirtual)
{
    __pageTraversalLink(pvmpageVirtual, __vmmPhysicalPageClear,
                        LW_NULL, LW_NULL, LW_NULL, LW_NULL, LW_NULL, LW_NULL);
}

#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */
/*********************************************************************************************************
** 函数名称: __vmmPhysicalGetZone
** 功能描述: 根据物理地址, 确定有效的 zone
** 输　入  : ulAddr        物理地址
** 输　出  : ulZoneIndex   物理 zone 下标
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
ULONG  __vmmPhysicalGetZone (addr_t  ulAddr)
{
             INT            i;
    REGISTER PLW_VMM_ZONE   pvmzone;

    for (i = 0; i < LW_CFG_VMM_ZONE_NUM; i++) {
        pvmzone = &_G_vmzonePhysical[i];
        if (pvmzone) {
            if ((ulAddr >= pvmzone->ZONE_ulAddr) &&
                (ulAddr <  pvmzone->ZONE_ulAddr + pvmzone->ZONE_stSize)) {
                return  ((ULONG)i);
            }
        }
    }
    
    return  (LW_CFG_VMM_ZONE_NUM);
}
/*********************************************************************************************************
** 函数名称: __vmmPhysicalPageGetMinContinue
** 功能描述: 获得物理页面内, 最小连续分页的个数
** 输　入  : pulZoneIndex      获得最小页面的 zone 下标
**           uiAttr            需要满足的物理页面属性
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
ULONG  __vmmPhysicalPageGetMinContinue (ULONG  *pulZoneIndex, UINT  uiAttr)
{
             INT            i;
    REGISTER PLW_VMM_ZONE   pvmzone;
             ULONG          ulMin = __ARCH_ULONG_MAX;
             ULONG          ulTemp;
             
    for (i = 0; i < LW_CFG_VMM_ZONE_NUM; i++) {
        pvmzone = &_G_vmzonePhysical[i];
        if (pvmzone && (pvmzone->ZONE_uiAttr == uiAttr)) {              /*  优先选择 uiAttr 相同的 zone */
            ulTemp = __pageGetMinContinue(pvmzone);
            if ((ulTemp > 0) && (ulMin > ulTemp)) {
                ulMin = ulTemp;
                *pulZoneIndex = (ULONG)i;
            }
        }
    }
    
    if (ulMin != __ARCH_ULONG_MAX) {
        return  (ulMin);
    }
    
    for (i = 0; i < LW_CFG_VMM_ZONE_NUM; i++) {
        pvmzone = &_G_vmzonePhysical[i];
        if (pvmzone && ((pvmzone->ZONE_uiAttr & uiAttr) == uiAttr)) {   /*  其次选择拥有 uiAttr 的 zone */
            ulTemp = __pageGetMinContinue(pvmzone);
            if ((ulTemp > 0) && (ulMin > ulTemp)) {
                ulMin = ulTemp;
                *pulZoneIndex = (ULONG)i;
            }
        }
    }
    
    return  (ulMin);
}
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
