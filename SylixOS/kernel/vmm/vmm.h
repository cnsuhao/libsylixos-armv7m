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
** 文   件   名: vmm.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 12 月 09 日
**
** 描        述: 平台无关虚拟内存管理.

** BUG:
2009.11.10  LW_VMM_ZONEDESC 中加入 DMA 区域判断符, 表明该区域是否可供 DMA 使用.
            DMA 内存分配, 返回值为物理地址.
2011.03.18  将 LW_VMM_ZONEDESC 更名为 LW_VMM_ZONE_DESC.
*********************************************************************************************************/

#ifndef __VMM_H
#define __VMM_H

/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0
/*********************************************************************************************************
  zone describer
  
  注意:
  
  编写 BSP 时需要划分内存分区, SylixOS 物理内存分区一般为如下格式:
  
  |<-- 物理虚拟地址相同 -->|
  |                        |
  +-----------+------------+--------------------------------+
  |  KERN MEM |  DMA ZONE  |          COMMON VMM            |
  |           |            |                                |
  |  内核内存 |  ATTR_DMA  |          ATTR_NONE             |
  +-----------+------------+--------------------------------+
              |                                             |
              |<----------- VMM 分配管理器 ---------------->|
  
  这里只是举例, 内存的顺序没有要求, 例如, 内核内存可以不必要在最前面, VMM 分配管理器也可以不在最后面.
  但是, SylixOS 对 BSP 有以下要求: "内核内存"和具有"ATTR_DMA"属性的 DMA 内存编址, 不能与"虚拟空间"有重叠!
  这里所说的"虚拟空间"就是 LW_MMU_VIRTUAL_DESC 描述的这段空间, 默认 3GB~4GB 之间, 可以通过内核启动参数修改
  而且, 内核内存与 ATTR_DMA 物理地址和虚拟地址初始化成相同. 
  (因为很多驱动程度使用 sys_malloc() 开辟 DMA 内存, 通过 cacheFlush 与 cacheInvalidate 与主存保持一致, 
   而且有些内存算法使用物理地址索引, 所以内核内存与 ATTR_DMA 物理地址和虚拟地址初始化成相同)
  
  "ATTR_NONE"可以和虚拟空间编址有重叠的, 因为 vmmDmaAlloc 出来的内存地址是物理地址, 而且他的虚拟地址也是
  相同的. 也就是说: 内核内存 与 ATTR_DMA 内存必须是平板映射, 而且地址不能与虚拟空间重叠!
  
  COMMON VMM 的物理地址是可以与虚拟空间重叠的, 因为这段物理地址都不能直接访问, 如果驱动程序要访问的
  寄存器与虚拟地址空间有地址重叠, 则必须使用 vmmIoRemap, vmmIoUnmap 来映射物理内存, 然后才能操作, 也就是说
  所有与虚拟空间有地址重叠的内存都需要通过 vmm 管理器所提供的相关函数才能访问, (必须先分配相应的虚拟地址)
  不能直接使用物理地址访问.
  
  一般的 BSP 中除了 COMMON VMM 区域以外都很小, COMMON VMM 是通用 VMM 内存的意思, 我们所有加载的程序或者
  模块都是在这段物理内存中运行的, 但是他们实际只能看到 VMM 分配给他们的虚拟空间!
  SylixOS 提供虚拟空间一般比真实的物理空间大很多, 一般在 32 位机器上可以达到 3GB.
*********************************************************************************************************/
#define LW_ZONE_ATTR_NONE    0x0000                                     /*  无特殊属性                  */
#define LW_ZONE_ATTR_DMA     0x0001                                     /*  此区域可以被 DMA 使用       */

typedef struct __lw_vmm_zone_desc {
    addr_t                   ZONED_ulAddr;                              /*  起始地址                    */
    size_t                   ZONED_stSize;                              /*  区域长度                    */
    UINT                     ZONED_uiAttr;                              /*  区域属性                    */
} LW_VMM_ZONE_DESC;
typedef LW_VMM_ZONE_DESC    *PLW_VMM_ZONE_DESC;

/*********************************************************************************************************
  mmu 全局初始化映射关系节点
*********************************************************************************************************/

typedef struct __lw_mmu_global_desc {
    addr_t                   ulVirtualAddr;                             /*  虚拟地址                    */
    addr_t                   ulPhysicalAddr;                            /*  物理地址                    */
    size_t                   stSize;                                    /*  长度                        */
    ULONG                    ulFlag;                                    /*  标志                        */
} LW_MMU_GLOBAL_DESC;
typedef LW_MMU_GLOBAL_DESC  *PLW_MMU_GLOBAL_DESC;

/*********************************************************************************************************
  mmu 虚拟地址空间
*********************************************************************************************************/

typedef struct __lw_mmu_virtual_desc {
    addr_t                   ulVirtualSwitch;                           /*  内部页面交换空间地址(自动)  */
    addr_t                   ulVirtualStart;                            /*  虚拟空间起始地址            */
    size_t                   stSize;                                    /*  虚拟空间长度                */
} LW_MMU_VIRTUAL_DESC;
typedef LW_MMU_VIRTUAL_DESC *PLW_MMU_VIRTUAL_DESC;

/*********************************************************************************************************
  vmm 当前状态
*********************************************************************************************************/

typedef struct __lw_vmm_status {
    INT64                    VMMS_i64AbortCounter;                      /*  异常中止次数                */
    INT64                    VMMS_i64PageFailCounter;                   /*  缺页中断正常处理次数        */
    INT64                    VMMS_i64PageLackCounter;                   /*  系统缺少物理页面次数        */
    INT64                    VMMS_i64MapErrCounter;                     /*  映射错误次数                */
    INT64                    VMMS_i64SwpCounter;                        /*  交换次数                    */
    INT64                    VMMS_i64Reseve[8];
} LW_VMM_STATUS;
typedef LW_VMM_STATUS       *PLW_VMM_STATUS;

/*********************************************************************************************************
  VMM 初始化, 只能放在 API_KernelStart 回调中, 
  
  当为 SMP 系统时, API_KernelPrimaryStart    对应启动回调调用 API_VmmLibPrimaryInit
                   API_KernelSecondaryStart  对应启动回调调用 API_VmmLibSecondaryInit
*********************************************************************************************************/

#ifdef __SYLIXOS_KERNEL
LW_API ULONG        API_VmmLibPrimaryInit(LW_VMM_ZONE_DESC      vmzone[],
                                          LW_MMU_GLOBAL_DESC    mmugdesc[],
                                          CPCHAR                pcMachineName);
                                                                        /*  初始化 VMM 组件及物理区域   */
#define API_VmmLibInit      API_VmmLibPrimaryInit

#if LW_CFG_SMP_EN > 0
LW_API ULONG        API_VmmLibSecondaryInit(CPCHAR  pcMachineName);
#endif                                                                  /*  LW_CFG_SMP_EN               */

/*********************************************************************************************************
  VMM API (以下分配函数可以分配出确定的, 可供直接访问的内存空间)
*********************************************************************************************************/

LW_API PVOID        API_VmmMalloc(size_t stSize);                       /*  分配逻辑连续内存, 虚拟地址  */
LW_API PVOID        API_VmmMallocEx(size_t stSize, ULONG ulFlag);       /*  分配逻辑连续内存, 虚拟地址  */
LW_API PVOID        API_VmmMallocAlign(size_t stSize, 
                                       size_t stAlign, 
                                       ULONG  ulFlag);                  /*  分配逻辑连续内存, 虚拟地址  */
LW_API VOID         API_VmmFree(PVOID  pvVirtualMem);                   /*  回收虚拟连续内存            */

LW_API ULONG        API_VmmVirtualToPhysical(addr_t  ulVirtualAddr, 
                                             addr_t *pulPhysicalAddr);  /*  通过虚拟地址获取物理地址    */
                                             
LW_API BOOL         API_VmmVirtualIsInside(addr_t  ulAddr);             /*  指定地址是否在管理的虚拟空间*/
                                             
LW_API ULONG        API_VmmZoneStatus(ULONG     ulZoneIndex,
                                      addr_t   *pulPhysicalAddr,
                                      size_t   *pstSize,
                                      addr_t   *pulPgd,
                                      ULONG    *pulFreePage,
                                      BOOL     *puiAttr);               /*  获得物理区域的信息          */
                                      
LW_API ULONG        API_VmmVirtualStatus(addr_t  *pulVirtualAddr,
                                         size_t  *pulSize,
                                         ULONG   *pulFreePage);         /*  获得虚拟空间信息            */
                                         
/*********************************************************************************************************
  VMM 扩展操作
  
  仅开辟虚拟空间, 当出现第一次访问时, 将通过缺页中断分配物理内存, 当缺页中断中无法分配物理页面时, 将收到
  SIGSEGV 信号并结束线程. 
  
  API_VmmRemapArea() 仅供驱动程序使用, 方法类似于 linux remap_pfn_range() 函数.
*********************************************************************************************************/

LW_API PVOID        API_VmmMallocArea(size_t stSize, FUNCPTR  pfuncFiller, PVOID  pvArg);
                                                                        /*  分配逻辑连续内存, 虚拟地址  */
                                                                        
LW_API PVOID        API_VmmMallocAreaEx(size_t stSize, FUNCPTR  pfuncFiller, PVOID  pvArg, 
                                        INT  iFlags, ULONG ulFlag);     /*  分配逻辑连续内存, 虚拟地址  */
                                                                        
LW_API PVOID        API_VmmMallocAreaAlign(size_t stSize, size_t stAlign, 
                                           FUNCPTR  pfuncFiller, PVOID  pvArg, 
                                           INT  iFlags, ULONG  ulFlag); /*  分配逻辑连续内存, 虚拟地址  */
                                                                        
LW_API VOID         API_VmmFreeArea(PVOID  pvVirtualMem);               /*  回收虚拟连续内存            */

LW_API ULONG        API_VmmExpandArea(PVOID  pvVirtualMem, size_t  stExpSize);

LW_API PVOID        API_VmmSplitArea(PVOID  pvVirtualMem, size_t  stSize);

LW_API ULONG        API_VmmPCountInArea(PVOID  pvVirtualMem, ULONG  *pulPageNum);
                                                                        /*  统计缺页中断分配的内存页面  */

LW_API ULONG        API_VmmRemapArea(PVOID  pvVirtualAddr, PVOID  pvPhysicalAddr, 
                                     size_t stSize, ULONG  ulFlag,
                                     FUNCPTR pfuncFiller, PVOID  pvArg);/*  建立新的映射关系            */
                                                                        /*  仅供驱动程序 mmap 使用此函数*/
LW_API ULONG        API_VmmInvalidateArea(PVOID  pvVirtualMem, 
                                          PVOID  pvSubMem, 
                                          size_t stSize);               /*  释放物理内存, 保留虚拟空间  */
                                          
LW_API VOID         API_VmmAbortStatus(PLW_VMM_STATUS  pvmms);          /*  获得访问中止统计信息        */
       
/*********************************************************************************************************
  VMM 对于 loader 或者其他内核模块提供的共享段支持 (仅供 loader 或其他 SylixOS 内核服务自己使用)
*********************************************************************************************************/

LW_API ULONG        API_VmmSetFiller(PVOID  pvVirtualMem, FUNCPTR  pfuncFiller, PVOID  pvArg);
                                                                        /*  设置填充函数                */
LW_API ULONG        API_VmmSetFindShare(PVOID  pvVirtualMem, PVOIDFUNCPTR  pfuncFindShare, PVOID  pvArg);
                                                                        /*  设置查询共享函数            */
LW_API ULONG        API_VmmPreallocArea(PVOID       pvVirtualMem, 
                                        PVOID       pvSubMem, 
                                        size_t      stSize, 
                                        FUNCPTR     pfuncTempFiller, 
                                        PVOID       pvTempArg,
                                        ULONG       ulFlag);            /*  预分配物理内存页面          */
                                        
LW_API ULONG        API_VmmShareArea(PVOID      pvVirtualMem1, 
                                     PVOID      pvVirtualMem2,
                                     size_t     stStartOft1, 
                                     size_t     stStartOft2, 
                                     size_t     stSize,
                                     BOOL       bExecEn,
                                     FUNCPTR    pfuncTempFiller, 
                                     PVOID      pvTempArg);             /*  设置共享区域                */

/*********************************************************************************************************
  内核专用 API 用户慎用 
  
  仅可以修改虚拟内存区分配出的页面, 修改大小已分配时的大小为基准
*********************************************************************************************************/

LW_API ULONG        API_VmmSetFlag(PVOID  pvVirtualAddr, 
                                   ULONG  ulFlag);                      /*  设置虚拟地址权限            */
                                   
LW_API ULONG        API_VmmGetFlag(PVOID  pvVirtualAddr, 
                                   ULONG *pulFlag);                     /*  获取虚拟地址权限            */
                                   
/*********************************************************************************************************
  以下 API 只负责分配物理内存, 并没有产生映射关系. 不能直接使用, 必须通过虚拟内存映射才能使用.
*********************************************************************************************************/

LW_API PVOID        API_VmmPhyAlloc(size_t stSize);                     /*  分配物理内存                */
LW_API PVOID        API_VmmPhyAllocEx(size_t  stSize, UINT  uiAttr);    /*  与上相同, 但可以指定内存属性*/
LW_API PVOID        API_VmmPhyAllocAlign(size_t stSize, 
                                         size_t stAlign,
                                         UINT   uiAttr);                /*  分配物理内存, 指定对其关系  */
LW_API VOID         API_VmmPhyFree(PVOID  pvPhyMem);                    /*  释放物理内存                */

/*********************************************************************************************************
  以下 API 只允许驱动程序使用
  
  no cache 区域操作 (dma 操作返回的是直接的物理地址, 即平板映射, 并且在 LW_ZONE_ATTR_DMA 域中)
*********************************************************************************************************/

LW_API PVOID        API_VmmDmaAlloc(size_t  stSize);                    /*  分配物理连续内存, 物理地址  */
LW_API PVOID        API_VmmDmaAllocAlign(size_t stSize, size_t stAlign);/*  与上相同, 但可以指定对齐关系*/
LW_API VOID         API_VmmDmaFree(PVOID  pvDmaMem);                    /*  回收 DMA 内存缓冲区         */

/*********************************************************************************************************
  以下 API 只允许驱动程序使用
  
  如果驱动程序寄存器地址与操作系统虚拟地址空间有重叠, 操作寄存器时, 一定要使用 ioremap 和 iounmap 函数
  
  这样才能保证操作的正确性与安全性.
*********************************************************************************************************/

LW_API PVOID        API_VmmIoRemap(PVOID  pvPhysicalAddr, 
                                   size_t stSize);                      /*  将物理 IO 映射到虚拟空间    */
LW_API VOID         API_VmmIoUnmap(PVOID  pvVirtualAddr);               /*  释放 IO 映射虚拟空间        */

LW_API PVOID        API_VmmIoRemapNocache(PVOID  pvPhysicalAddr, 
                                          size_t stSize);
LW_API ULONG        API_VmmMap(PVOID  pvVirtualAddr, 
                               PVOID  pvPhysicalAddr, 
                               size_t stSize, 
                               ULONG  ulFlag);                          /*  指定内存映射 (慎用此函数)   */
#endif                                                                  /*  __SYLIXOS_KERNEL            */

#else

#ifdef __SYLIXOS_KERNEL
static LW_INLINE ULONG        API_VmmVirtualToPhysical(addr_t  ulVirtualAddr, 
                                                       addr_t *pulPhysicalAddr)
{
    if (pulPhysicalAddr) {
        *pulPhysicalAddr = ulVirtualAddr;
    }
    
    return  (ERROR_NONE);
}

static LW_INLINE PVOID        API_VmmIoRemap(PVOID  pvPhysicalAddr, 
                                             size_t stSize)
{
    return  (pvPhysicalAddr);
}
static LW_INLINE VOID         API_VmmIoUnmap(PVOID  pvVirtualAddr)
{
}
static LW_INLINE PVOID        API_VmmIoRemapNocache(PVOID  pvPhysicalAddr, 
                                                    size_t stSize)
{
    return  (pvPhysicalAddr);
}
#endif                                                                  /*  __SYLIXOS_KERNEL            */

#endif                                                                  /*  LW_CFG_VMM_EN > 0           */

/*********************************************************************************************************
  API_VmmAbortIsr() 为异常处理函数, 只允许 ARCH 代码使用. 
  没有使能 VMM SylixOS 依然可以处理异常情况
*********************************************************************************************************/

#define LW_VMM_ABORT_TYPE_TERMINAL          1                           /*  体系结构相关严重错误        */
#define LW_VMM_ABORT_TYPE_MAP               2                           /*  页表映射错误 (MMU 报告)     */
#define LW_VMM_ABORT_TYPE_EXEC              3                           /*  不允许执行                  */
#define LW_VMM_ABORT_TYPE_WRITE             4                           /*  写权限错误 (MMU 报告)       */
#define LW_VMM_ABORT_TYPE_FPE               5                           /*  浮点运算器异常              */
#define LW_VMM_ABORT_TYPE_BUS               6                           /*  总线访问异常                */
#define LW_VMM_ABORT_TYPE_BREAK             7                           /*  断点异常                    */
#define LW_VMM_ABORT_TYPE_SYS               8                           /*  系统调用异常                */
#define LW_VMM_ABORT_TYPE_UNDEF             9                           /*  未定义指令, 将产生 SIGILL   */

LW_API VOID         API_VmmAbortIsr(addr_t          ulRetAddr,
                                    addr_t          ulAbortAddr, 
                                    ULONG           ulAbortType,
                                    PLW_CLASS_TCB   ptcb);              /*  异常中断服务函数            */
                                    
/*********************************************************************************************************
  vmm api macro
*********************************************************************************************************/

#define vmmMalloc               API_VmmMalloc
#define vmmMallocEx             API_VmmMallocEx
#define vmmMallocAlign          API_VmmMallocAlign
#define vmmFree                 API_VmmFree

#define vmmMallocArea           API_VmmMallocArea
#define vmmMallocAreaEx         API_VmmMallocAreaEx
#define vmmMallocAreaAlign      API_VmmMallocAreaAlign
#define vmmFreeArea             API_VmmFreeArea
#define vmmPCountInArea         API_VmmPCountInArea
#define vmmRemapArea            API_VmmRemapArea
#define vmmInvalidateArea       API_VmmInvalidateArea
#define vmmAbortStatus          API_VmmAbortStatus

#define vmmPhyAlloc             API_VmmPhyAlloc
#define vmmPhyAllocEx           API_VmmPhyAllocEx
#define vmmPhyAllocAlign        API_VmmPhyAllocAlign
#define vmmPhyFree              API_VmmPhyFree

#define vmmDmaAlloc             API_VmmDmaAlloc                         /*  返回值为 物理地址           */
#define vmmDmaAllocAlign        API_VmmDmaAllocAlign                    /*  返回值为 物理地址           */
#define vmmDmaFree              API_VmmDmaFree

#define vmmIoRemap              API_VmmIoRemap
#define vmmIoUnmap              API_VmmIoUnmap
#define vmmIoRemapNocache       API_VmmIoRemapNocache

#define vmmMap                  API_VmmMap
#define vmmVirtualToPhysical    API_VmmVirtualToPhysical
#define vmmPhysicalToVirtual    API_VmmPhysicalToVirtual                /*  仅支持VMM管理的物理内存查询 */
#define vmmVirtualIsInside      API_VmmVirtualIsInside

#define vmmSetFlag              API_VmmSetFlag
#define vmmGetFlag              API_VmmGetFlag

#define vmmZoneStatus           API_VmmZoneStatus
#define vmmVirtualStatus        API_VmmVirtualStatus

#endif                                                                  /*  __VMM_H                     */
/*********************************************************************************************************
  END
*********************************************************************************************************/
