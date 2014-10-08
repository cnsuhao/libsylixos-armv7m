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
** 文   件   名: loader_lib.h
**
** 创   建   人: Jiang.Taijin (蒋太金)
**
** 文件创建日期: 2010 年 04 月 17 日
**
** 描        述: 公用头文件

** BUG:
2011.02.20  为兼容 posix 动态链接库标准, 加入对符号全局性与局部性的处理.
2012.09.21  兼容 BSD 程序, 加入了对 issetugid 的功能.
2012.10.25  加入进程 I/O 环境.
2012.12.09  加入进程树功能.
2012.12.18  加入进程私有 FD 表.
2014.05.13  加入子线程链表.
2014.09.30  加入进程定时器.
*********************************************************************************************************/

#ifndef __LOADER_LIB_H
#define __LOADER_LIB_H

/*********************************************************************************************************
  进程最大文件描述符 
  (因为进程0 1 2标准文件与内核一样映射方式不用, 这里的标准文件为真实打开的文件, 所以没有 STD_UNFIX 操作.
   为了继承内核文件描述符, 这里必须为 LW_CFG_MAX_FILES + 3)
*********************************************************************************************************/

#define LW_VP_MAX_FILES     (LW_CFG_MAX_FILES + 3)

/*********************************************************************************************************
  进程定时器
*********************************************************************************************************/

typedef struct lw_vproc_timer {
    ULONG                   VPT_ulCounter;                              /*  定时器当前定时时间          */
    ULONG                   VPT_ulInterval;                             /*  定时器自动装载值            */
} LW_LD_VPROC_T;

/*********************************************************************************************************
  进程控制块
*********************************************************************************************************/

typedef struct lw_ld_vproc {
    LW_LIST_LINE            VP_lineManage;                              /*  管理链表                    */
    LW_LIST_RING_HEADER     VP_ringModules;                             /*  模块链表                    */
    FUNCPTR                 VP_pfuncProcess;                            /*  进程主入口                  */
    
    LW_OBJECT_HANDLE        VP_ulModuleMutex;                           /*  进程模块链表锁              */
    
    BOOL                    VP_bRunAtExit;                              /*  是否允许 atexit 安装的函数  */
    pid_t                   VP_pid;                                     /*  进程号                      */
    BOOL                    VP_bIssetugid;                              /*  是否设置的 ugid             */
    PCHAR                   VP_pcName;                                  /*  进程名称                    */
    LW_OBJECT_HANDLE        VP_ulMainThread;                            /*  主线程句柄                  */
    PVOID                   VP_pvProcInfo;                              /*  proc 文件系统信息           */
    
    clock_t                 VP_clockUser;                               /*  times 对应的 utime          */
    clock_t                 VP_clockSystem;                             /*  times 对应的 stime          */
    clock_t                 VP_clockCUser;                              /*  times 对应的 cutime         */
    clock_t                 VP_clockCSystem;                            /*  times 对应的 cstime         */
    
    PLW_IO_ENV              VP_pioeIoEnv;                               /*  I/O 环境                    */
    LW_OBJECT_HANDLE        VP_ulWaitForExit;                           /*  主线程等待结束              */

#define __LW_VP_INIT        0
#define __LW_VP_RUN         1
#define __LW_VP_EXIT        2
#define __LW_VP_STOP        3
    INT                     VP_iStatus;                                 /*  当前进程状态                */
    INT                     VP_iExitCode;                               /*  结束代码                    */
    INT                     VP_iSigCode;                                /*  iSigCode                    */
    
    struct lw_ld_vproc     *VP_pvprocFather;                            /*  父亲 (NULL 表示孤儿进程)    */
    LW_LIST_LINE_HEADER     VP_plineChild;                              /*  儿子进程链表头              */
    LW_LIST_LINE            VP_lineBrother;                             /*  兄弟进程                    */
    pid_t                   VP_pidGroup;                                /*  组 id 号                    */
    LW_LIST_LINE_HEADER     VP_plineThread;                             /*  子线程链表                  */
    
    LW_FD_DESC              VP_fddescTbl[LW_VP_MAX_FILES];              /*  进程 fd 表                  */
    
    INT                     VP_iExitMode;                               /*  退出模式                    */
    LW_LD_VPROC_T           VP_vptimer[3];                              /*  REAL / VIRTUAL / PROF 定时器*/
    ULONG                   VP_ulPad[10];                               /*  预留                        */
} LW_LD_VPROC;

/*********************************************************************************************************
  进程装载完成停止控制 (sigval.sival_int == 0 表明装载完成并停止, < 0 表示装载异常, 进程正在退出)
*********************************************************************************************************/

typedef struct {
    INT                     VPS_iSigNo;                                 /*  装载完成后发送的信号        */
    LW_OBJECT_HANDLE        VPS_ulId;                                   /*  目标线程 (或进程)           */
} LW_LD_VPROC_STOP;

/*********************************************************************************************************
  模块运行时占用的内存段，卸载时需释放这些段
*********************************************************************************************************/

typedef struct {
    addr_t                  ESEG_ulAddr;                                /*  内存段地址                  */
    size_t                  ESEG_stLen;                                 /*  内存段长度                  */
} LW_LD_EXEC_SEGMENT;

/*********************************************************************************************************
  模块符号表缓冲
*********************************************************************************************************/

typedef struct {
    LW_LIST_LINE            ESYM_lineManage;                            /*  管理链表                    */
    size_t                  ESYM_stSize;                                /*  分段缓冲大小                */
    size_t                  ESYM_stUsed;                                /*  本分段已经使用的内存数量    */
} LW_LD_EXEC_SYMBOL;

#define LW_LD_EXEC_SYMBOL_HDR_SIZE  ROUND_UP(sizeof(LW_LD_EXEC_SYMBOL), sizeof(LW_STACK))

/*********************************************************************************************************
  模块类型，KO\SO
*********************************************************************************************************/

#define LW_LD_MOD_TYPE_KO           0                                   /*  内核模块                    */
#define LW_LD_MOD_TYPE_SO           1                                   /*  应用程序或动态链接库        */

/*********************************************************************************************************
  模块运行状态
*********************************************************************************************************/

#define LW_LD_STATUS_UNLOAD         0                                   /*  未加载                      */
#define LW_LD_STATUS_LOADED         1                                   /*  已加载未初始化              */
#define LW_LD_STATUS_INITED         2                                   /*  已初始化                    */
#define LW_LD_STATUS_FINIED         3                                   /*  已初始化                    */

/*********************************************************************************************************
  模块结构体用于组织模块的信息
*********************************************************************************************************/

#define __LW_LD_EXEC_MODULE_MAGIC   0x25ef68af

typedef struct {
    ULONG                   EMOD_ulMagic;                               /*  用于识别本结构体            */
    ULONG                   EMOD_ulModType;                             /*  模块类型，KO还是SO文件      */
    ULONG                   EMOD_ulStatus;                              /*  模块当前运行状态            */
    ULONG                   EMOD_ulRefs;                                /*  模块引用计数                */
    PVOID                   EMOD_pvUsedArr;                             /*  依赖的模块数组              */
    ULONG                   EMOD_ulUsedCnt;                             /*  依赖模块数组大小            */

    PCHAR                   EMOD_pcSymSection;                          /*  仅加载指定 section 符号表   */

    VOIDFUNCPTR            *EMOD_ppfuncInitArray;                       /*  初始化函数数组              */
    ULONG                   EMOD_ulInitArrCnt;                          /*  初始化函数个数              */
    VOIDFUNCPTR            *EMOD_ppfuncFiniArray;                       /*  结束函数数组                */
    ULONG                   EMOD_ulFiniArrCnt;                          /*  结束函数个数                */

    PCHAR                   EMOD_pcInit;                                /*  初始化函数名称              */
    FUNCPTR                 EMOD_pfuncInit;                             /*  初始化函数指针              */

    PCHAR                   EMOD_pcExit;                                /*  结束函数名称                */
    FUNCPTR                 EMOD_pfuncExit;                             /*  结束函数指针                */

    PCHAR                   EMOD_pcEntry;                               /*  入口函数名称                */
    FUNCPTR                 EMOD_pfuncEntry;                            /*  main函数指针                */
    FUNCPTR                 EMOD_pfuncDestroy;                          /*  销毁模块函数指针            */
    BOOL                    EMOD_bIsSymbolEntry;                        /*  是否为符号入口              */

    size_t                  EMOD_stLen;                                 /*  为模块分配的内存长度        */
    PVOID                   EMOD_pvBaseAddr;                            /*  为模块分配的内存地址        */

    BOOL                    EMOD_bIsGlobal;                             /*  是否为全局符号              */
    ULONG                   EMOD_ulSymCount;                            /*  导出符号数目                */
    ULONG                   EMOD_ulSymHashSize;                         /*  符号 hash 表大小            */
    LW_LIST_LINE_HEADER    *EMOD_psymbolHash;                           /*  导出符号列 hash 表          */
    LW_LIST_LINE_HEADER     EMOD_plineSymbolBuffer;                     /*  导出符号表缓冲              */

    ULONG                   EMOD_ulSegCount;                            /*  内存段数目                  */
    LW_LD_EXEC_SEGMENT     *EMOD_psegmentArry;                          /*  内存段列表                  */
    BOOL                    EMOD_bExportSym;                            /*  是否导出符号                */
    PCHAR                   EMOD_pcModulePath;                          /*  模块文件路径                */

    LW_LD_VPROC            *EMOD_pvproc;                                /*  所属进程                    */
    LW_LIST_RING            EMOD_ringModules;                           /*  进程中所有的库链表          */
    PVOID                   EMOD_pvFormatInfo;                          /*  重定位相关信息              */

#ifdef LW_CFG_CPU_ARCH_ARM
    size_t                  EMOD_stARMExidxCount;                       /*  ARM.exidx 段长度            */
    PVOID                   EMOD_pvARMExidx;                            /*  ARM.exidx 段内存地址        */
#endif                                                                  /*  LW_CFG_CPU_ARCH_ARM         */

} LW_LD_EXEC_MODULE;

/*********************************************************************************************************
  内核进程控制块
*********************************************************************************************************/

extern LW_LD_VPROC          _G_vprocKernel;

/*********************************************************************************************************
  module 操作锁
*********************************************************************************************************/

extern LW_OBJECT_HANDLE     _G_ulVProcMutex;

#define LW_LD_LOCK()        API_SemaphoreMPend(_G_ulVProcMutex, LW_OPTION_WAIT_INFINITE)
#define LW_LD_UNLOCK()      API_SemaphoreMPost(_G_ulVProcMutex)

#define LW_VP_LOCK(a)       API_SemaphoreMPend(a->VP_ulModuleMutex, LW_OPTION_WAIT_INFINITE)
#define LW_VP_UNLOCK(a)     API_SemaphoreMPost(a->VP_ulModuleMutex)

/*********************************************************************************************************
  调试信息打印
*********************************************************************************************************/

#ifdef  __SYLIXOS_DEBUG
#define LD_DEBUG_MSG(msg)   printf msg
#else
#define LD_DEBUG_MSG(msg)
#endif                                                                  /*  __LOAD_DEBUG                */

/*********************************************************************************************************
  进程默认入口与应用入口
*********************************************************************************************************/

#define LW_LD_DEFAULT_ENTRY         "_start"                            /*  进程初始化入口符号          */
#define LW_LD_PROCESS_ENTRY         "main"                              /*  进程主入口符号              */

/*********************************************************************************************************
  小容量内存管理
*********************************************************************************************************/

#define LW_LD_SAFEMALLOC(size)      __SHEAP_ALLOC((size_t)size)
#define LW_LD_SAFEFREE(a)           { if (a) { __SHEAP_FREE((PVOID)a); a = 0; } }
                                                                        /*  安全释放                    */

/*********************************************************************************************************
  内核模块大容量内存管理
*********************************************************************************************************/

PVOID __ldMalloc(size_t  stLen);                                        /*  分配内存                    */
PVOID __ldMallocAlign(size_t  stLen, size_t  stAlign);
VOID  __ldFree(PVOID  pvAddr);                                          /*  释放内存                    */

#define LW_LD_VMSAFEMALLOC(size)    \
        __ldMalloc(size)
        
#define LW_LD_VMSAFEMALLOC_ALIGN(size, align)   \
        __ldMallocAlign(size, align)
        
#define LW_LD_VMSAFEFREE(a) \
        { if (a) { __ldFree((PVOID)a); a = 0; } }

/*********************************************************************************************************
  共享库大容量内存管理
*********************************************************************************************************/

extern LW_OBJECT_HANDLE             _G_ulExecShareLock;                 /*  共享库内存段锁              */

PVOID  __ldMallocArea(size_t  stLen);
PVOID  __ldMallocAreaAlign(size_t  stLen, size_t  stAlign);
VOID   __ldFreeArea(PVOID  pvAddr);

#define LW_LD_VMSAFEMALLOC_AREA(size)   \
        __ldMallocArea(size)
        
#define LW_LD_VMSAFEMALLOC_AREA_ALIGN(size, align)  \
        __ldMallocAreaAlign(size, align)
        
#define LW_LD_VMSAFEFREE_AREA(a)    \
        { if (a) { __ldFreeArea((PVOID)a); a = 0; } }
        
INT     __ldMmap(PVOID  pvBase, size_t  stAddrOft, INT  iFd, struct stat64 *pstat64,
                 off_t  oftOffset, size_t  stLen,  BOOL  bCanShare, BOOL  bCanExec);
VOID    __ldShare(PVOID  pvBase, size_t  stLen, dev_t  dev, ino64_t ino64);
VOID    __ldShareAbort(dev_t  dev, ino64_t  ino64);
INT     __ldShareConfig(BOOL  bShareEn, BOOL  *pbPrev);

#define LW_LD_VMSAFEMAP_AREA(base, addr_offset, fd, pstat64, file_offset, len, can_share, can_exec) \
        __ldMmap(base, addr_offset, fd, pstat64, file_offset, len, can_share, can_exec)
        
#define LW_LD_VMSAFE_SHARE(base, len, dev, ino64) \
        __ldShare(base, len, dev, ino64)
        
#define LW_LD_VMSAFE_SHARE_ABORT(dev, ino64)    \
        __ldShareAbort(dev, ino64)
        
#define LW_LD_VMSAFE_SHARE_CONFIG(can_share, prev)  \
        __ldShareConfig(can_share, prev)

/*********************************************************************************************************
  地址转换
*********************************************************************************************************/

#define LW_LD_V2PADDR(vBase, pBase, vAddr)      \
        ((size_t)pBase + (size_t)vAddr - (size_t)vBase)                 /*  计算物理地址                */

#define LW_LD_P2VADDR(vBase, pBase, pAddr)      \
        ((size_t)vBase + (size_t)pAddr - (size_t)pBase)                 /*  计算虚拟地址                */
        
/*********************************************************************************************************
  vp 补丁操作
*********************************************************************************************************/
#define LW_LD_VMEM_MAX      64

PCHAR __moduleVpPatchVersion(LW_LD_EXEC_MODULE *pmodule);               /*  获得补丁版本                */
PVOID __moduleVpPatchHeap(LW_LD_EXEC_MODULE *pmodule);                  /*  获得补丁独立内存堆          */
INT   __moduleVpPatchVmem(LW_LD_EXEC_MODULE *pmodule, PVOID  ppvArea[], INT  iSize);
                                                                        /*  获得进程虚拟内存空间        */
VOID  __moduleVpPatchInit(LW_LD_EXEC_MODULE *pmodule);                  /*  进程补丁构造与析构          */
VOID  __moduleVpPatchFini(LW_LD_EXEC_MODULE *pmodule);
/*********************************************************************************************************
  体系结构特有函数
*********************************************************************************************************/

#ifdef LW_CFG_CPU_ARCH_ARM
typedef long unsigned int   *_Unwind_Ptr;
_Unwind_Ptr dl_unwind_find_exidx(_Unwind_Ptr pc, int *pcount, PVOID *pvVProc);
#endif                                                                  /*  LW_CFG_CPU_ARCH_ARM         */

#endif                                                                  /*  __LOADER_LIB_H              */
/*********************************************************************************************************
  END
*********************************************************************************************************/
