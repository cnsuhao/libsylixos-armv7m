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
** 文   件   名: vmmAbort.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2012 年 09 月 05 日
**
** 描        述: 平台无关异常管理.
**
** 注        意: 当发生缺页中断时, 所有的填充I/O操作(交换区或mmap填充)都是在 vmm lock 下进行的!
                 SylixOS 处理缺页异常是在任务上下文中, 通过陷阱程序完成, 所以, 不影响无关任务的调度与运行.
                 但是也带来了诸多问题. 如多任务可能在同一地址发生异常等等, 这里就是用的一些方法基本解决了
                 这个问题.
                 
** BUG:
2012.09.15  未定义指令将产生 SIGILL 信号.
2012.12.22  错误类型的打印使用字符串.
2012.12.22  __vmmAbortKillSignal() 不应该返回, 而是等待被删除.
2013.03.29  产生异常后, 应该恢复之前的 errno.
2013.05.09  API_VmmAbortIsr() 如果不在中断上下文, 直接处理即可, 不需要进行任务上下文构造.
2013.05.10  缺少物理页面时, 应该使用 SIGKILL 信号杀死进程.
2013.09.13  异常退出后, 恢复 kernel space 状态.
2013.09.14  支持文件之间的共享映射.
2013.11.21  升级 _doSigEvent() 调用参数.
2013.12.23  支持对 LW_VMM_ABORT_TYPE_EXEC 的错误处理.
2014.05.17  断点探测不再在这里进行.
2014.05.21  系统发生段错误时, 需要通知 dtrace.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if LW_CFG_GDB_EN > 0
#include "dtrace.h"
#endif                                                                  /*  LW_CFG_GDB_EN > 0           */
#if LW_CFG_VMM_EN > 0
#include "vmmSwap.h"
#include "phyPage.h"
/*********************************************************************************************************
  内核信息打印与其他内核函数
*********************************************************************************************************/
#ifndef printk
#define printk
#endif                                                                  /*  printk                      */
#if LW_CFG_MODULELOADER_EN > 0
extern pid_t  API_ModulePid(PVOID pvVProc);
#else
#define API_ModulePid(x)     0
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
/*********************************************************************************************************
  虚拟分页空间链表 (缺页中断查找表)
*********************************************************************************************************/
LW_LIST_LINE_HEADER       _K_plineVmmVAddrSpaceHeader = LW_NULL;
/*********************************************************************************************************
  统计变量
*********************************************************************************************************/
static LW_VMM_STATUS      _K_vmmStatus;
/*********************************************************************************************************
  内部函数声明
*********************************************************************************************************/
static VOID   __vmmAbortKill(PLW_VMM_PAGE_FAIL_CTX  pvmpagefailctx);
static VOID   __vmmAbortAccess(PLW_VMM_PAGE_FAIL_CTX  pvmpagefailctx);
/*********************************************************************************************************
  操作锁
*********************************************************************************************************/
extern  LW_OBJECT_HANDLE    _G_ulVmmLock;
#define __VMM_LOCK()        API_SemaphoreMPend(_G_ulVmmLock, LW_OPTION_WAIT_INFINITE)
#define __VMM_UNLOCK()      API_SemaphoreMPost(_G_ulVmmLock)
/*********************************************************************************************************
  基本参数
*********************************************************************************************************/
#define __PAGEFAIL_ALLOC_PAGE_NUM   1                                   /*  缺页中断分配页面数, 必须为1 */
#define __PAGEFAIL_CUR_PID          API_ModulePid(ptcbCur->TCB_pvVProcessContext)
/*********************************************************************************************************
** 函数名称: __vmmAbortCacheRefresh
** 功能描述: 当 MMU 缺页处理完成后, 这里处理 cache 的刷新操作.
** 输　入  : bSwapNeedLoad     是否是从 swap 中读出数据
**           
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __vmmAbortCacheRefresh (BOOL    bSwapNeedLoad,
                                    addr_t  ulVirtualPageAlign,
                                    ULONG   ulAllocPageNum)
{
#if LW_CFG_CACHE_EN > 0
    size_t  stSize = 0;
    
    if (API_CacheLocation(DATA_CACHE) == CACHE_LOCATION_VIVT) {         /*  如果是虚拟地址 cache        */
        stSize = (size_t)(ulAllocPageNum * LW_CFG_VMM_PAGE_SIZE);
        API_CacheClear(DATA_CACHE, (PVOID)LW_CFG_VMM_VIRTUAL_SWITCH,
                       stSize);                                         /*  将数据写入内存并不再命中    */
        API_CacheInvalidate(DATA_CACHE, (PVOID)ulVirtualPageAlign, 
                            stSize);                                    /*  无效新虚拟内存空间          */
    }
    
    if (bSwapNeedLoad) {                                                /*  swap load 有可能是代码      */
        if (stSize == 0) {
            stSize = (size_t)(ulAllocPageNum * LW_CFG_VMM_PAGE_SIZE);
        }
        API_CacheTextUpdate((PVOID)ulVirtualPageAlign, stSize);         /*  无效 I-CACHE (内部已经回写) */
    }
#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */
}
/*********************************************************************************************************
** 函数名称: __vmmAbortPageGet
** 功能描述: 根据异常地址, 获取虚拟页面控制块
** 输　入  : ulAbortAddr       异常地址
** 输　出  : 虚拟页面控制块
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static PLW_VMM_PAGE  __vmmAbortPageGet (addr_t  ulAbortAddr)
{
             PLW_LIST_LINE          plineTemp;
    REGISTER PLW_VMM_PAGE           pvmpageVirtual;
    REGISTER PLW_VMM_PAGE_PRIVATE   pvmpagep;
    
    for (plineTemp  = _K_plineVmmVAddrSpaceHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {                 /*  查询访问的地址是否为合法地址*/
         
        pvmpagep       = _LIST_ENTRY(plineTemp, LW_VMM_PAGE_PRIVATE, PAGEP_lineManage);
        pvmpageVirtual = pvmpagep->PAGEP_pvmpageVirtual;
        
        if ((ulAbortAddr >= pvmpageVirtual->PAGE_ulPageAddr) &&
            (ulAbortAddr <  (pvmpageVirtual->PAGE_ulPageAddr + 
                             (pvmpageVirtual->PAGE_ulCount * LW_CFG_VMM_PAGE_SIZE)))) {
            break;
        }
    }
    
    if (plineTemp) {
        return  (pvmpageVirtual);
    } else {
        return  (LW_NULL);
    }
}
/*********************************************************************************************************
** 函数名称: __vmmAbortCopyOnWrite
** 功能描述: 共享区域发上改变时, 需要将改变的页面变为 private, 不再被共享.
** 输　入  : pvmpageVirtual        虚拟空间
**           ulAbortAddrAlign      异常的地址
** 输　出  : 是否成功
** 全局变量: 
** 调用模块: 
** 注  意  : 1: 如果是共享变化的段, 处理器为虚拟地址 cache 类型, 则 mmap 肯定已经将对应区域的 cache 禁能
                所以, 这里不需要操作 cache
             2: __vmmPhysicalPageClone() 参数一定为 pvmpagePhysical, 因为 pvmpagePhysical 对应的虚拟地址
                就是我们需要 copy 的数据源, 因为 pvmpageReal 对应的虚拟地址可能已经不存在或者被改写了.
*********************************************************************************************************/
static INT  __vmmAbortCopyOnWrite (PLW_VMM_PAGE  pvmpageVirtual, addr_t  ulAbortAddrAlign)
{
    PLW_VMM_PAGE            pvmpagePhysical = __pageFindLink(pvmpageVirtual, ulAbortAddrAlign);
    PLW_VMM_PAGE            pvmpageReal;
    PLW_VMM_PAGE            pvmpageNew;
    PLW_VMM_PAGE_PRIVATE    pvmpagep;

    ULONG                   ulError;
    
    if (pvmpagePhysical == LW_NULL) {
        return  (PX_ERROR);
    }
    
    pvmpagep = (PLW_VMM_PAGE_PRIVATE)pvmpageVirtual->PAGE_pvAreaCb;

    if (LW_VMM_PAGE_IS_FAKE(pvmpagePhysical)) {
        pvmpageReal = LW_VMM_PAGE_GET_REAL(pvmpagePhysical);
    
    } else {
        pvmpageReal = pvmpagePhysical;
    }
    
    if ((pvmpageReal->PAGE_ulRef == 1) ||                               /*  当前虚拟页面是唯一引用      */
        (pvmpagep->PAGEP_iFlags & LW_VMM_SHARED_CHANGE)) {              /*  共享变化                    */
        
        pvmpageReal->PAGE_iChange     = 1;
        pvmpagePhysical->PAGE_iChange = 1;
        pvmpageReal->PAGE_ulFlags     = pvmpageVirtual->PAGE_ulFlags;
        pvmpagePhysical->PAGE_ulFlags = pvmpageVirtual->PAGE_ulFlags;
        
        __vmmLibSetFlag(ulAbortAddrAlign, pvmpageVirtual->PAGE_ulFlags);/*  可写                        */
        return  (ERROR_NONE);                                           /*  更改属性即可                */
    
    } else {                                                            /*  有其他的共享存在            */
                                                                        /*  执行 copy-on-write 操作     */
        pvmpageNew = __vmmPhysicalPageClone(pvmpagePhysical);
        if (pvmpageNew == LW_NULL) {
            if (API_GetLastError() == ERROR_VMM_LOW_PHYSICAL_PAGE) {
                printk(KERN_CRIT "kernel no more physical page.\n");    /*  系统无法分配物理页面        */
            }
            return  (PX_ERROR);
        }
        
        __pageUnlink(pvmpageVirtual, pvmpagePhysical);                  /*  解除连接关系                */
        
        __vmmPhysicalPageFree(pvmpagePhysical);                         /*  不再使用老物理页面          */
        
        ulError = __vmmLibPageMap(pvmpageNew->PAGE_ulPageAddr,          
                                  ulAbortAddrAlign, 1,
                                  pvmpageVirtual->PAGE_ulFlags);        /*  可写                        */
        if (ulError) {
            __vmmPhysicalPageFree(pvmpageNew);
            return  (PX_ERROR);
        }
        
        pvmpageNew->PAGE_ulMapPageAddr = ulAbortAddrAlign;
        pvmpageNew->PAGE_ulFlags       = pvmpageVirtual->PAGE_ulFlags;
        pvmpageNew->PAGE_iChange       = 1;
                                                                        
        __pageLink(pvmpageVirtual, pvmpageNew);                         /*  建立连接                    */
    }

    return  (ERROR_NONE);
}

/*********************************************************************************************************
** 函数名称: __vmmAbortWriteProtect
** 功能描述: 发生写保护中止
** 输　入  : pvmpageVirtual        虚拟空间
**           ulAbortAddrAlign      异常的地址
**           ulFlag                页面属性
** 输　出  : 是否成功
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
static INT  __vmmAbortWriteProtect (PLW_VMM_PAGE  pvmpageVirtual, 
                                    addr_t        ulAbortAddrAlign,
                                    ULONG         ulFlag)
{
    if (ulFlag & LW_VMM_FLAG_WRITABLE) {
        return  (ERROR_NONE);                                           /*  可能其他任务同时访问此地址  */
    }
    
    if (pvmpageVirtual->PAGE_ulFlags & LW_VMM_FLAG_WRITABLE) {          /*  虚拟空间允许写操作          */
        return  (__vmmAbortCopyOnWrite(pvmpageVirtual, 
                                       ulAbortAddrAlign));              /*  copy-on-write               */
    }
    
    return  (PX_ERROR);                                                 /*  杀死任务, 内存不可写        */
}
/*********************************************************************************************************
** 函数名称: __vmmAbortNoPage
** 功能描述: 缺页中止时处理新的页面
** 输　入  : pvmpagePhysical       物理页面
**           pvmpagep              虚拟空间属性
**           ulVirtualPageAlign    虚拟地址
**           ulAllocPageNum        物理页面数量
** 输　出  : 是否成功
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
static INT  __vmmAbortNoPage (PLW_VMM_PAGE           pvmpagePhysical, 
                              PLW_VMM_PAGE_PRIVATE   pvmpagep,
                              addr_t                 ulVirtualPageAlign, 
                              ULONG                  ulAllocPageNum)
{
    ULONG   ulError;

    if (pvmpagep->PAGEP_pfuncFiller) {                                  /*  需要填充                    */
        ulError = __vmmLibPageMap(pvmpagePhysical->PAGE_ulPageAddr,     /*  使用 CACHE 操作             */
                                  LW_CFG_VMM_VIRTUAL_SWITCH,            /*  缓冲区虚拟地址              */
                                  ulAllocPageNum, 
                                  LW_VMM_FLAG_RDWR);                    /*  映射指定的虚拟地址          */
        if (ulError) {
            _K_vmmStatus.VMMS_i64MapErrCounter++;
            printk(KERN_CRIT "kernel physical page map error.\n");      /*  系统无法映射物理页面        */
            return  (PX_ERROR);
        }
        
        if (pvmpagep->PAGEP_pfuncFiller) {                              /*  新页面需要填充              */
            pvmpagep->PAGEP_pfuncFiller(pvmpagep->PAGEP_pvArg,          
                                        LW_CFG_VMM_VIRTUAL_SWITCH,      /*  需要拷贝的缓存目标虚拟地址  */
                                        ulVirtualPageAlign,             /*  缓存最后切换的目标虚拟地址  */
                                        ulAllocPageNum);                /*  拷贝的页面个数              */
        }
                                                                        /*  cache 刷新                  */
        __vmmAbortCacheRefresh(LW_FALSE, ulVirtualPageAlign, ulAllocPageNum);
    
        __vmmLibSetFlag(LW_CFG_VMM_VIRTUAL_SWITCH, LW_VMM_FLAG_FAIL);   /*  VIRTUAL_SWITCH 不允许访问   */
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __vmmAbortSwapPage
** 功能描述: 页面交换
** 输　入  : pvmpagePhysical       物理页面
**           ulVirtualPageAlign    虚拟地址
**           ulAllocPageNum        页面个数
** 输　出  : 是否成功
** 全局变量: 
** 调用模块: 
** 注  意  : 这里进行数据填充, 数据填充时, 必须在 vmm lock 下进行, 如果打开 vmm 其他阻塞在 vmm 上的任务
             立即运行可能会产生错误! 这里也不能直接将数据填充到指定的虚拟地址中, 因为多核状态下没有任何锁
             可以锁住其他任务机器指令访问内存! 因此只能先将填充内存转向高速切换用虚拟页面.
             (所有的填充都是在 vmm lock 下串行执行的)
*********************************************************************************************************/
static INT  __vmmAbortSwapPage (PLW_VMM_PAGE  pvmpagePhysical, 
                                addr_t        ulVirtualPageAlign, 
                                ULONG         ulAllocPageNum)
{
    PLW_CLASS_TCB   ptcbCur;                                            /*  当前任务控制块              */
    ULONG           ulError;

    LW_TCB_GET_CUR_SAFE(ptcbCur);

    ulError = __vmmLibPageMap(pvmpagePhysical->PAGE_ulPageAddr,         /*  使用 CACHE 操作             */
                              LW_CFG_VMM_VIRTUAL_SWITCH,                /*  缓冲区虚拟地址              */
                              ulAllocPageNum, 
                              LW_VMM_FLAG_RDWR);                        /*  映射指定的虚拟地址          */
    if (ulError) {
        _K_vmmStatus.VMMS_i64MapErrCounter++;
        printk(KERN_CRIT "kernel physical page map error.\n");          /*  系统无法映射物理页面        */
        return  (PX_ERROR);
    }
    
    ulError = __vmmPageSwapLoad(__PAGEFAIL_CUR_PID,
                                LW_CFG_VMM_VIRTUAL_SWITCH, 
                                ulVirtualPageAlign);                    /*  从交换区中读取页面原始内容  */
    if (ulError) {
        printk(KERN_CRIT "kernel swap load page error.\n");             /*  系统无法读出 swap 页面      */
        return  (PX_ERROR);                                             /*  如果错误 swap 会更新统计变量*/
    }
    
    __vmmAbortCacheRefresh(LW_TRUE, ulVirtualPageAlign, ulAllocPageNum);/*  cache 刷新                  */
    
    __vmmLibSetFlag(LW_CFG_VMM_VIRTUAL_SWITCH, LW_VMM_FLAG_FAIL);       /*  VIRTUAL_SWITCH 不允许访问   */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __vmmAbortNewPage
** 功能描述: 分配一个新的页面, 如果缺少物理页面则交换一个老旧的物理页面
** 输　入  : ulAllocPageNum        需要的内存页面个数
** 输　出  : 物理页面
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
static PLW_VMM_PAGE  __vmmAbortNewPage (ULONG  ulAllocPageNum)
{
    PLW_VMM_PAGE    pvmpagePhysical;
    ULONG           ulZoneIndex;
    PLW_CLASS_TCB   ptcbCur;                                            /*  当前任务控制块              */
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);
    
    pvmpagePhysical = __vmmPhysicalPageAlloc(ulAllocPageNum, 
                                             LW_ZONE_ATTR_NONE,
                                             &ulZoneIndex);             /*  分配物理内存(1个页面)       */
    if (pvmpagePhysical == LW_NULL) {                                   /*  如果分配失败择交换出一个页面*/
        _K_vmmStatus.VMMS_i64PageLackCounter++;
        pvmpagePhysical = __vmmPageSwapSwitch(__PAGEFAIL_CUR_PID,
                                              ulAllocPageNum, 
                                              LW_ZONE_ATTR_NONE);       /*  进行页面交换                */
        return  (pvmpagePhysical);
    }
    
    return  (pvmpagePhysical);
}
/*********************************************************************************************************
** 函数名称: __vmmAbortShareCallback
** 功能描述: __vmmAbortShare 回调函数
** 输　入  : pvStartAddr       查找到可共享区域后的区域其实虚拟地址
**           stOffset          可共享页面偏移
** 输　出  : 物理页面
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
static PVOID __vmmAbortShareCallback (PVOID  pvStartAddr, size_t  stOffset)
{
    PLW_VMM_PAGE         pvmpageVirtual;
    PLW_VMM_PAGE         pvmpagePhysical;
    PLW_VMM_PAGE_PRIVATE pvmpagep;
    addr_t               ulVirAddr = (addr_t)pvStartAddr;
    
    pvmpageVirtual = __areaVirtualSearchPage(ulVirAddr);
    if (pvmpageVirtual == LW_NULL) {
        return  (LW_NULL);
    }
    
    pvmpagep = (PLW_VMM_PAGE_PRIVATE)pvmpageVirtual->PAGE_pvAreaCb;
    if (pvmpagep == LW_NULL) {
        return  (LW_NULL);
    }
    
    if (pvmpagep->PAGEP_iFlags & LW_VMM_PRIVATE_CHANGE) {               /*  目标必须允许被共享          */
        return  (LW_NULL);
    }
    
    ulVirAddr += stOffset;
    
    pvmpagePhysical = __pageFindLink(pvmpageVirtual, ulVirAddr);
    
    return  ((PVOID)pvmpagePhysical);
}
/*********************************************************************************************************
** 函数名称: __vmmAbortShare
** 功能描述: 试图从 mmap 区间找到可以共享访问的页面
** 输　入  : pvmpageVirtual        发送异常的虚拟页面
**           pvmpagep              虚拟页面信息
**           ulVirtualPageAlign    发生异常的页面对齐地址
** 输　出  : 是否已经设置共享
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
static INT  __vmmAbortShare (PLW_VMM_PAGE         pvmpageVirtual, 
                             PLW_VMM_PAGE_PRIVATE pvmpagep,
                             addr_t               ulVirtualPageAlign)
{
    PLW_VMM_PAGE    pvmpagePhysical;
    PLW_VMM_PAGE    pvmpageRef;
    ULONG           ulError;
    
    if (pvmpagep->PAGEP_pfuncFindShare == LW_NULL) {
        return  (PX_ERROR);
    }
    
    pvmpagePhysical = (PLW_VMM_PAGE)pvmpagep->PAGEP_pfuncFindShare(pvmpagep->PAGEP_pvFindArg, 
                                                                   ulVirtualPageAlign,
                                                                   __vmmAbortShareCallback);
    if (pvmpagePhysical == LW_NULL) {
        return  (PX_ERROR);
    }
    
    pvmpageRef = __vmmPhysicalPageRef(pvmpagePhysical);                 /*  共享物理页面                */
    if (pvmpageRef == LW_NULL) {
        return  (PX_ERROR);
    }
    
    ulError = __vmmLibPageMap(pvmpageRef->PAGE_ulPageAddr,
                              ulVirtualPageAlign, 1, 
                              pvmpageRef->PAGE_ulFlags);
    if (ulError) {
        __vmmPhysicalPageFree(pvmpageRef);
        return  (PX_ERROR);
    }
    
    pvmpageRef->PAGE_ulMapPageAddr = ulVirtualPageAlign;
    
    __pageLink(pvmpageVirtual, pvmpageRef);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __vmmAbortDump
** 功能描述: 当 __vmmAbortShell() 无法执行时, 打印调试信息
** 输　入  : pvmpagefailctx    page fail 上下文
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __vmmAbortDump (PLW_VMM_PAGE_FAIL_CTX  pvmpagefailctx)
{
#if LW_CFG_POSIX_EN > 0
LW_API  int  mmapfd(void  *pvAddr);
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */

             addr_t                 ulAbortAddr = pvmpagefailctx->PAGEFCTX_ulAbortAddr;
             LW_OBJECT_HANDLE       ulOwner;
             CHAR                   cBuffer[PATH_MAX + 1];
             PCHAR                  pcTail = LW_NULL;
             PCHAR                  pcType = LW_NULL;
             CPCHAR                 pcKernelFunc;
             CHAR                   cMmapMsg[PATH_MAX + 1] = "<unknown>.";
             
    REGISTER PLW_VMM_PAGE           pvmpageVirtual;
    REGISTER PLW_VMM_PAGE_PRIVATE   pvmpagep;
    
    ulOwner      = __KERNEL_OWNER();
    pcKernelFunc = __KERNEL_ENTERFUNC();
    
    pvmpageVirtual = __vmmAbortPageGet(ulAbortAddr);                    /*  获得对应虚拟内存控制块      */
    
    if (pvmpageVirtual) {
        pvmpagep = (PLW_VMM_PAGE_PRIVATE)pvmpageVirtual->PAGE_pvAreaCb;
        if (pvmpagep->PAGEP_pfuncFiller) {
#if LW_CFG_POSIX_EN > 0
            INT     iFd = mmapfd((void *)ulAbortAddr);                  /*  获得地址对应的文件描述符    */
            snprintf(cMmapMsg, PATH_MAX + 1, "address in mmap, fd %d\n", iFd);
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
            pcTail = cMmapMsg;
        
        } else {
            pcTail = "address in vmm.";
        }
    } else {
        pcTail = "address invalidate.";
    }
    
    switch (pvmpagefailctx->PAGEFCTX_ulAbortType) {
    
    case LW_VMM_ABORT_TYPE_TERMINAL:
        pcType = "cpu extremity error";
        break;
        
    case LW_VMM_ABORT_TYPE_MAP:
        pcType = "memory map";
        break;
    
    case LW_VMM_ABORT_TYPE_WRITE:
        pcType = "can not write";
        break;
    
    case LW_VMM_ABORT_TYPE_FPE:
        pcType = "float points";
        break;
    
    case LW_VMM_ABORT_TYPE_BUS:
        pcType = "bus";
        break;
    
    case LW_VMM_ABORT_TYPE_BREAK:
        pcType = "break points";
        break;
    
    case LW_VMM_ABORT_TYPE_SYS:
        pcType = "syscall";
        break;
    
    case LW_VMM_ABORT_TYPE_UNDEF:
        pcType = "undefined instruction";
        break;
    
    default:
        pcType = "unknown";
        break;
    }
    
    snprintf(cBuffer, PATH_MAX + 1, "abort in kernel status, "
                                    "owner : 0x%08lx, func : %s, addr: 0x%08lx, type : %s, %s.\r\n", 
                                    ulOwner, 
                                    pcKernelFunc,
                                    pvmpagefailctx->PAGEFCTX_ulAbortAddr,
                                    pcType,
                                    pcTail);
                                    
    _DebugHandle(__ERRORMESSAGE_LEVEL, cBuffer);
}
/*********************************************************************************************************
** 函数名称: __vmmAbortShell
** 功能描述: 当 MMU 产生访问失效时, 线程执行陷阱函数.
** 输　入  : pvmpagefailctx    page fail 上下文
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __vmmAbortShell (PLW_VMM_PAGE_FAIL_CTX  pvmpagefailctx)
{
             addr_t                 ulAbortAddr = pvmpagefailctx->PAGEFCTX_ulAbortAddr;
             ULONG                  ulFlag;
             
    REGISTER PLW_VMM_PAGE           pvmpageVirtual;
    REGISTER PLW_VMM_PAGE           pvmpagePhysical;
    REGISTER PLW_VMM_PAGE_PRIVATE   pvmpagep;
             
             ULONG                  ulAllocPageNum;
             BOOL                   bSwapNeedLoad;
             
             addr_t                 ulVirtualPageAlign;
             ULONG                  ulError;
             INT                    iRet;
             PLW_CLASS_TCB          ptcbCur;                            /*  当前任务控制块              */

    if (__KERNEL_ISENTER()) {
        __vmmAbortDump(pvmpagefailctx);                                 /*  打印关键信息                */
        __vmmAbortKill(pvmpagefailctx);
        goto    __abort_return;
    }
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);
    
    __VMM_LOCK();
    _K_vmmStatus.VMMS_i64AbortCounter++;
    
    if ((pvmpagefailctx->PAGEFCTX_ulAbortType != LW_VMM_ABORT_TYPE_MAP) &&
        (pvmpagefailctx->PAGEFCTX_ulAbortType != LW_VMM_ABORT_TYPE_WRITE)) {
        __VMM_UNLOCK();
        __vmmAbortAccess(pvmpagefailctx);                               /*  访问异常情况                */
        goto    __abort_return;                                         /*  不会运行到这里              */
    }
    
    pvmpageVirtual = __vmmAbortPageGet(ulAbortAddr);                    /*  获得对应虚拟内存控制块      */
    if (pvmpageVirtual) {
        pvmpagep = (PLW_VMM_PAGE_PRIVATE)pvmpageVirtual->PAGE_pvAreaCb;
    } else {
        __VMM_UNLOCK();
        __vmmAbortAccess(pvmpagefailctx);                               /*  访问异常                    */
        goto    __abort_return;                                         /*  不会运行到这里              */
    }
    
    _K_vmmStatus.VMMS_i64PageFailCounter++;                             /*  缺页中断次数++              */
    ptcbCur->TCB_i64PageFailCounter++;                                  /*  缺页中断次数++              */
    
    ulVirtualPageAlign = ulAbortAddr & LW_CFG_VMM_PAGE_MASK;            /*  获得访问地址页边界          */
    
    ulError = __vmmLibGetFlag(ulVirtualPageAlign, &ulFlag);             /*  获得异常地址的物理页面属性  */
    if (ulError == ERROR_NONE) {                                        /*  存在物理页面正常            */
        if (pvmpagefailctx->PAGEFCTX_ulAbortType == LW_VMM_ABORT_TYPE_WRITE) {
            if (__vmmAbortWriteProtect(pvmpageVirtual,                  /*  写操作异常                  */
                                       ulVirtualPageAlign,
                                       ulError) == ERROR_NONE) {        /*  进入写保护处理              */
                __VMM_UNLOCK();
                goto    __abort_return;
            }
        }
        __VMM_UNLOCK();
        __vmmAbortAccess(pvmpagefailctx);                               /*  非法内存访问                */
        goto    __abort_return;                                         /*  不会运行到这里              */
    
    } else {                                                            /*  映射错误, 没有物理页面存在  */
        if (pvmpagep->PAGEP_iFlags & LW_VMM_SHARED_CHANGE) {            /*  共享区间                    */
            if (__vmmAbortShare(pvmpageVirtual, 
                                pvmpagep,
                                ulVirtualPageAlign) == ERROR_NONE) {    /*  尝试共享                    */
                __VMM_UNLOCK();
                goto    __abort_return;
            }
        }
        
        ulAllocPageNum  = __PAGEFAIL_ALLOC_PAGE_NUM;                    /*  缺页中断分配的内存页面个数  */
        
        pvmpagePhysical = __vmmAbortNewPage(ulAllocPageNum);            /*  分配物理页面                */
        if (pvmpagePhysical == LW_NULL) {
            __VMM_UNLOCK();
            
            pvmpagefailctx->PAGEFCTX_ulAbortType = 0;                   /*  缺少物理页面                */
            printk(KERN_CRIT "kernel no more physical page.\n");        /*  系统无法分配物理页面        */
            __vmmAbortKill(pvmpagefailctx);
            goto    __abort_return;
        }
        
        bSwapNeedLoad = __vmmPageSwapIsNeedLoad(__PAGEFAIL_CUR_PID,
                                                ulVirtualPageAlign);    /*  检查是否需要 load swap 数据 */
        if (bSwapNeedLoad) {
            iRet = __vmmAbortSwapPage(pvmpagePhysical, 
                                      ulVirtualPageAlign,
                                      ulAllocPageNum);                  /*  进行页面交换处理            */
        
        } else {
            iRet = __vmmAbortNoPage(pvmpagePhysical, 
                                    pvmpagep,
                                    ulVirtualPageAlign, 
                                    ulAllocPageNum);                    /*  进行页面填充处理            */
        }
        
        if (iRet != ERROR_NONE) {
            __vmmPhysicalPageFree(pvmpagePhysical);
            __VMM_UNLOCK();
            __vmmAbortKill(pvmpagefailctx);
            goto    __abort_return;
        }
    }
    
    ulError = __vmmLibPageMap(pvmpagePhysical->PAGE_ulPageAddr,         /*  与分配时相同的属性          */
                              ulVirtualPageAlign,
                              ulAllocPageNum, 
                              pvmpageVirtual->PAGE_ulFlags);            /*  映射指定的虚拟地址          */
    if (ulError) {
        _K_vmmStatus.VMMS_i64MapErrCounter++;
        __vmmPhysicalPageFree(pvmpagePhysical);
        __VMM_UNLOCK();

        printk(KERN_CRIT "kernel physical page map error.\n");          /*  系统无法映射物理页面        */
        __vmmAbortKill(pvmpagefailctx);
        goto    __abort_return;
    }
    
    pvmpagePhysical->PAGE_ulMapPageAddr = ulVirtualPageAlign;
    pvmpagePhysical->PAGE_ulFlags       = pvmpageVirtual->PAGE_ulFlags;
    
    __pageLink(pvmpageVirtual, pvmpagePhysical);                        /*  建立连接关系                */
    __VMM_UNLOCK();
    
__abort_return:
    __KERNEL_SPACE_SET(pvmpagefailctx->PAGEFCTX_iKernelSpace);          /*  恢复成进入之前的状态        */
    errno = pvmpagefailctx->PAGEFCTX_iLastErrno;                        /*  恢复之前的 errno            */
    archSigCtxLoad(pvmpagefailctx->PAGEFCTX_pvStackRet);                /*  从 page fail 上下文中返回   */
}
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
   以下函数没有 MMU 时也可使用, 主要用来处理各种异常
*********************************************************************************************************/
/*********************************************************************************************************
** 函数名称: __vmmAbortKill
** 功能描述: 向当前线程产生一个信号
** 输　入  : pvmpagefailctx    page fail 上下文
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __vmmAbortKill (PLW_VMM_PAGE_FAIL_CTX  pvmpagefailctx)
{
    ULONG            ulAbortType = pvmpagefailctx->PAGEFCTX_ulAbortType;
    struct sigevent  sigeventAbort;
    INT              iSigCode;
    
#if LW_CFG_GDB_EN > 0
    if (!__KERNEL_ISENTER()) {
        if (API_DtraceAbortTrap(pvmpagefailctx->PAGEFCTX_ulRetAddr) == 
            ERROR_NONE) {                                               /*  可以通知调试器              */
            return;                                                     /*  返回后将会产生断点          */
        }
    }
#endif                                                                  /*  LW_CFG_GDB_EN > 0           */
    
    switch (ulAbortType) {
    
    case 0:
        sigeventAbort.sigev_signo = SIGKILL;                            /*  通过 SIGKILL 信号杀死任务   */
        iSigCode = SI_KILL;
        break;
    
    case LW_VMM_ABORT_TYPE_FPE:
        sigeventAbort.sigev_signo = SIGFPE;
        iSigCode = FPE_INTDIV;                                          /*  默认为除 0 需要进行详细判断 */
        break;
        
    case LW_VMM_ABORT_TYPE_BUS:
        sigeventAbort.sigev_signo = SIGBUS;
        iSigCode = BUS_ADRALN;
        break;
        
    case LW_VMM_ABORT_TYPE_SYS:
        sigeventAbort.sigev_signo = SIGSYS;
        iSigCode = SI_MESGQ;
        break;
        
    case LW_VMM_ABORT_TYPE_UNDEF:
        sigeventAbort.sigev_signo = SIGILL;
        iSigCode = ILL_ILLOPC;
        break;
        
    case LW_VMM_ABORT_TYPE_MAP:
        sigeventAbort.sigev_signo = SIGSEGV;
        iSigCode = SEGV_MAPERR;
        
    case LW_VMM_ABORT_TYPE_EXEC:                                        /*  与 default 共享分支         */
    default:
        sigeventAbort.sigev_signo = SIGSEGV;
        iSigCode = SEGV_ACCERR;
        break;
    }

    sigeventAbort.sigev_value.sival_ptr = (PVOID)pvmpagefailctx->PAGEFCTX_ulAbortAddr;
    sigeventAbort.sigev_notify          = SIGEV_SIGNAL;
    
    _doSigEvent(pvmpagefailctx->PAGEFCTX_ulSelf, &sigeventAbort, iSigCode);
    
    if (__KERNEL_ISENTER()) {                                           /*  出现致命错误                */
        for (;;);
    }
    
    for (;;) {                                                          /*  1.0.0-rc36 开始这里不退出   */
        API_TimeSleep(__ARCH_ULONG_MAX);                                /*  等待被删除                  */
    }
}
/*********************************************************************************************************
** 函数名称: __vmmAbortAccess
** 功能描述: 访问型错误处理 (此函数不会返回)
** 输　入  : pvmpagefailctx    page fail 上下文
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __vmmAbortAccess (PLW_VMM_PAGE_FAIL_CTX  pvmpagefailctx)
{
    ULONG   ulErrorOrig = API_GetLastError();
    
    if (pvmpagefailctx->PAGEFCTX_ulAbortType == LW_VMM_ABORT_TYPE_UNDEF) {
        printk(KERN_EMERG "thread 0x%lx undefine-instruction, address : 0x%lx.\n",
               pvmpagefailctx->PAGEFCTX_ulSelf, 
               pvmpagefailctx->PAGEFCTX_ulAbortAddr);                   /*  操作异常                    */
               
    } else if (pvmpagefailctx->PAGEFCTX_ulAbortType == LW_VMM_ABORT_TYPE_FPE) {
#if LW_CFG_CPU_FPU_EN > 0 && LW_CFG_DEVICE_EN > 0
        PLW_CLASS_TCB   ptcbCur;
        LW_TCB_GET_CUR_SAFE(ptcbCur);
        __ARCH_FPU_CTX_SHOW(ioGlobalStdGet(STD_ERR), ptcbCur->TCB_pvStackFP);
#endif                                                                  /*  LW_CFG_CPU_FPU_EN > 0       */
        printk(KERN_EMERG "thread 0x%lx float-point exception, address : 0x%lx.\n",
               pvmpagefailctx->PAGEFCTX_ulSelf, 
               pvmpagefailctx->PAGEFCTX_ulAbortAddr);                   /*  操作异常                    */
    
    } else {
#if LW_CFG_DEVICE_EN > 0
        archTaskCtxShow(ioGlobalStdGet(STD_ERR), (PLW_STACK)pvmpagefailctx->PAGEFCTX_pvStackRet);
#endif
        printk(KERN_EMERG "thread 0x%lx abort, address : 0x%lx.\n",
               pvmpagefailctx->PAGEFCTX_ulSelf, 
               pvmpagefailctx->PAGEFCTX_ulAbortAddr);                   /*  操作异常                    */
    }
    
    __vmmAbortKill(pvmpagefailctx);                                     /*  发送异常信号                */
    
    _ErrorHandle(ulErrorOrig);                                          /*  恢复之前的 errno            */
    
    /*
     * iRet == 0 or iRet > 0 都需要返回重新执行指令.
     */
    archSigCtxLoad(pvmpagefailctx->PAGEFCTX_pvStackRet);                /*  从 page fail 上下文中返回   */
}
/*********************************************************************************************************
** 函数名称: API_VmmAbortIsr
** 功能描述: 当 MMU 产生访问失效时, 调用此函数(类似于中断服务函数)
** 输　入  : ulRetAddr     异常返回地址
**           ulAbortAddr   异常地址 (异常类型相关)
**           ulAbortType   异常类型
**           ptcb          出现异常的线程控制块 (不能为 NULL)
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
** 注  意  : 从 1.0.0.rc20 版本后, 此函数可以脱离 VMM 运行.
**
**           1. 构造缺页中断线程执行陷阱, 在陷阱中完成所有的页面操作, 如果失败, 线程自我销毁.
**           2. 注意, 陷阱执行完毕后, 系统必须能够回到刚刚访问内存并产生异常的那条指令.
**           3. 由于产生缺页中断时, 相关线程一定是就绪的, 所以这里不用加入调度器处理.

                                           API 函数
*********************************************************************************************************/
LW_API 
VOID  API_VmmAbortIsr (addr_t  ulRetAddr, addr_t  ulAbortAddr, ULONG  ulAbortType, PLW_CLASS_TCB  ptcb)
{
    PLW_VMM_PAGE_FAIL_CTX    pvmpagefailctx;
    PLW_STACK                pstkFailShell;                             /*  启动 fail shell 的堆栈点    */
    
    ULONG                    ulNesting = LW_CPU_GET_CUR_NESTING();
    BYTE                    *pucStkNow;                                 /*  记录还原堆栈点              */
    
    if (ulNesting > 1) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "abort occur in exception mode.\r\n");
        API_KernelReboot(LW_REBOOT_FORCE);                              /*  直接重新启动操作系统        */
        return;
    }
    
    __KERNEL_ENTER();                                                   /*  进入内核                    */
                                                                        /*  产生异常                    */
    pucStkNow = (BYTE *)ptcb->TCB_pstkStackNow;                         /*  记录还原堆栈点              */
#if	CPU_STK_GROWTH == 0
    pucStkNow      += sizeof(LW_STACK);                                 /*  向空栈方向移动一个堆栈空间  */
    pvmpagefailctx  = (PLW_VMM_PAGE_FAIL_CTX)pucStkNow;                 /*  记录 PAGE_FAIL_CTX 位置     */
    pucStkNow      += __PAGEFAILCTX_SIZE_ALIGN;                         /*  让出 PAGE_FAIL_CTX 空间     */
#else
    pucStkNow      -= __PAGEFAILCTX_SIZE_ALIGN;                         /*  让出 PAGE_FAIL_CTX 空间     */
    pvmpagefailctx  = (PLW_VMM_PAGE_FAIL_CTX)pucStkNow;                 /*  记录 PAGE_FAIL_CTX 位置     */
    pucStkNow      -= sizeof(LW_STACK);                                 /*  向空栈方向移动一个堆栈空间  */
#endif
    
    pvmpagefailctx->PAGEFCTX_ulSelf       = ptcb->TCB_ulId;
    pvmpagefailctx->PAGEFCTX_ulRetAddr    = ulRetAddr;                  /*  异常返回地址                */
    pvmpagefailctx->PAGEFCTX_ulAbortAddr  = ulAbortAddr;                /*  异常地址 (异常类型相关)     */
    pvmpagefailctx->PAGEFCTX_ulAbortType  = ulAbortType;                /*  异常类型                    */
    pvmpagefailctx->PAGEFCTX_pvStackRet   = ptcb->TCB_pstkStackNow;
    pvmpagefailctx->PAGEFCTX_iLastErrno   = (errno_t)ptcb->TCB_ulLastError;
    pvmpagefailctx->PAGEFCTX_iKernelSpace = __KERNEL_SPACE_GET2(ptcb);
    
#if LW_CFG_VMM_EN > 0
    pstkFailShell = archTaskCtxCreate((PTHREAD_START_ROUTINE)__vmmAbortShell, 
                                      (PVOID)pvmpagefailctx,
                                      (PLW_STACK)pucStkNow,
                                      0);                               /*  建立缺页处理陷阱外壳环境    */
#else
    pstkFailShell = archTaskCtxCreate((PTHREAD_START_ROUTINE)__vmmAbortAccess, 
                                      (PVOID)pvmpagefailctx,
                                      (PLW_STACK)pucStkNow,
                                      0);                               /*  建立访问异常陷阱外壳环境    */
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */

    archTaskCtxSetFp(pstkFailShell, ptcb->TCB_pstkStackNow);            /*  保存 fp, 使 callstack 正常  */
    ptcb->TCB_pstkStackNow = pstkFailShell;                             /*  保存建立好的陷阱外壳堆栈    */
    _StackCheckGuard(ptcb);                                             /*  堆栈警戒检查                */
    
    __KERNEL_EXIT();                                                    /*  退出内核                    */
}
/*********************************************************************************************************
** 函数名称: API_VmmAbortStatus
** 功能描述: 系统访问中止状态. 
** 输　入  : pvmms         系统状态变量
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0

LW_API 
VOID  API_VmmAbortStatus (PLW_VMM_STATUS  pvmms)
{
    if (!pvmms) {
        return;
    }

    __VMM_LOCK();
    *pvmms = _K_vmmStatus;
    __VMM_UNLOCK();
    
    _ErrorHandle(ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
