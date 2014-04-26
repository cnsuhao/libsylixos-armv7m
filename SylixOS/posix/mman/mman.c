/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                       SylixOS(TM)
**
**                               Copyright  All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: mman.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2010 年 08 月 12 日
**
** 描        述: POSIX IEEE Std 1003.1, 2004 Edition sys/mman.h

** BUG:
2011.03.04  如果 vmmMalloc 没有成功则转而使用 __SHEAP_ALLOC().
2011.05.16  mprotect() 使用 vmmSetFlag() 接口操作.
            支持非 REG 文件类型文件 mmap() 操作.
2011.05.17  支持 mmap() 标准, 首先分配虚拟空间, 产生缺页中断时, 分配物理内存.
2011.07.27  当 mmap() 缺页中断填充文件内容没有达到一页时, 需要填充 0 .
            支持设备文件的 mmap() 操作.
2011.08.03  获得文件 stat 使用 fstat api.
2011.12.09  mmap() 填充函数使用 iosRead() 完成.
            加入 mmapShow() 函数, 查看 mmap() 内存情况.
2012.01.10  加入对 MAP_ANONYMOUS 的支持.
2012.08.16  直接使用 pread/pwrite 操作文件.
2012.10.19  mmap() 与 munmap() 加入对文件描述符引用的处理.
2012.12.07  将 mmap 加入资源管理器.
2012.12.22  映射时, 如果不是建立映射的 pid 则判定为回收资源, 这里不减少对文件的引用, 
            因为当前的文件描述符表已不不是创建进程的文件描述符表. 回收器会回收相关的文件描述符.
2012.12.27  mmap 如果是 reg 文件, 也要首先试一下对应文件系统的 mmap 驱动, 如果有则不适用缺页填充.
            加入对 shm_open shm_unlink 的支持.
2012.12.30  mmapShow() 不再显示文件名, 而加入对 pid 的显示.
2013.01.02  结束映射时需要调用驱动的 unmap 操作.
2013.01.12  munmap 如果不是同一进程, 则不操作文件描述符和对应驱动.
2013.03.12  加入 mmap64.
2013.03.16  mmap 如果设备驱动不支持则也可以映射.
2013.03.17  mmap 根据 MAP_SHARED 标志决定是否使能 CACHE.
2013.09.13  支持 MAP_SHARED.
            msync() 存在物理页面且可写时才回写.
2013.12.21  支持 PROT_EXEC.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "unistd.h"
#include "../include/px_mman.h"                                         /*  已包含操作系统头文件        */
#include "../include/posixLib.h"                                        /*  posix 内部公共库            */
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if (LW_CFG_POSIX_EN > 0) && (LW_CFG_DEVICE_EN > 0)
/*********************************************************************************************************
** 函数名称: mlock
** 功能描述: 锁定指定内存地址空间不进行换页处理.
** 输　入  : pvAddr        起始地址
**           stLen         内存空间长度
** 输　出  : ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  mlock (const void  *pvAddr, size_t  stLen)
{
    (VOID)pvAddr;
    (VOID)stLen;
    
    if (geteuid() != 0) {
        errno = EACCES;
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: munlock
** 功能描述: 解锁指定内存地址空间, 允许进行换页处理.
** 输　入  : pvAddr        起始地址
**           stLen         内存空间长度
** 输　出  : ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  munlock (const void  *pvAddr, size_t  stLen)
{
    (VOID)pvAddr;
    (VOID)stLen;
    
    if (geteuid() != 0) {
        errno = EACCES;
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: mlockall
** 功能描述: 锁定进程空间不进行换页处理.
** 输　入  : iFlag         锁定选项 MCL_CURRENT/MCL_FUTURE
** 输　出  : ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  mlockall (int  iFlag)
{
    (VOID)iFlag;
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: munlockall
** 功能描述: 解锁进程空间, 允许进行换页处理.
** 输　入  : NONE
** 输　出  : ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  munlockall (void)
{
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: mprotect
** 功能描述: 设置进程内指定的地址段 page flag
** 输　入  : pvAddr        起始地址
**           stLen         长度
**           iProt         新的属性
** 输　出  : ERROR_NONE or PX_ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  mprotect (void  *pvAddr, size_t  stLen, int  iProt)
{
#if LW_CFG_VMM_EN > 0
    ULONG   ulFlag = LW_VMM_FLAG_READ;
    
    
    if (!ALIGNED(pvAddr, LW_CFG_VMM_PAGE_SIZE) || stLen == 0) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if (iProt & (~(PROT_READ | PROT_WRITE | PROT_EXEC))) {
        errno = ENOTSUP;
        return  (PX_ERROR);
    }
    
    if (iProt) {
        if (iProt & PROT_WRITE) {
            ulFlag |= LW_VMM_FLAG_RDWR;                                 /*  可读写                      */
        }
        if (iProt & PROT_EXEC) {
            ulFlag |= LW_VMM_FLAG_EXEC;                                 /*  可执行                      */
        }
    } else {
        ulFlag |= LW_VMM_FLAG_FAIL;                                     /*  不允许访问                  */
    }
    
    if (API_VmmSetFlag(pvAddr, ulFlag) == ERROR_NONE) {                 /*  重新设置新的 flag           */
        return  (ERROR_NONE);
    } else {
        return  (PX_ERROR);
    }
    
#else
    return  (ERROR_NONE);
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
}
/*********************************************************************************************************
  mmap 控制块结构
*********************************************************************************************************/
typedef struct {
    LW_LIST_LINE            PMAPN_lineManage;                           /*  双向链表                    */
    PVOID                   PMAPN_pvAddr;                               /*  起始地址                    */
    size_t                  PMAPN_stLen;                                /*  内存长度                    */
    ULONG                   PMAPN_ulFlag;                               /*  内存属性                    */
    INT                     PMAPN_iFd;                                  /*  关联文件                    */
    mode_t                  PMAPN_mode;                                 /*  文件mode                    */
    off_t                   PMAPN_off;                                  /*  文件映射偏移量              */
    off_t                   PMAPN_offFSize;                             /*  文件大小                    */
    INT                     PMAPN_iFlag;                                /*  mmap iFlag 参数             */
    BOOL                    PMAPN_bBusy;                                /*  忙标志                      */
    BOOL                    PMAPN_bIsHeapMem;                           /*  是否为 heap 分配出的内存    */
    
    dev_t                   PMAPN_dev;                                  /*  文件描述符对应 dev          */
    ino64_t                 PMAPN_ino64;                                /*  文件描述符对应 inode        */
    
    LW_LIST_LINE_HEADER     PMAPN_plineUnmap;                           /*  已经 unmap 的区域           */
    size_t                  PMAPN_stTotalUnmap;                         /*  已经 unmap 的总大小         */
    
    pid_t                   PMAPN_pid;                                  /*  映射进程的进程号            */
    LW_RESOURCE_RAW         PMAPN_resraw;                               /*  资源管理节点                */
} __PX_MAP_NODE;

typedef struct {
    LW_LIST_LINE            PMAPA_lineManage;                           /*  双向链表                    */
                                                                        /*  此链表按地址从小到大排列    */
    PVOID                   PMAPN_pvAddr;                               /*  起始地址                    */
    size_t                  PMAPN_stLen;                                /*  内存长度                    */
} __PX_MAP_AREA;
/*********************************************************************************************************
  mmap 所有缓冲区链表
*********************************************************************************************************/
static LW_LIST_LINE_HEADER  _G_plineMMapHeader = LW_NULL;
/*********************************************************************************************************
** 函数名称: __mmapNodeFind
** 功能描述: 查找 mmap node
** 输　入  : pvAddr        地址
** 输　出  : mmap node
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static __PX_MAP_NODE  *__mmapNodeFind (void  *pvAddr)
{
    __PX_MAP_NODE      *pmapn;
    PLW_LIST_LINE       plineTemp;
    
    for (plineTemp  = _G_plineMMapHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
         
        pmapn = (__PX_MAP_NODE *)plineTemp;
        
        if (((addr_t)pvAddr >= (addr_t)pmapn->PMAPN_pvAddr) && 
            ((addr_t)pvAddr <  ((addr_t)pmapn->PMAPN_pvAddr + pmapn->PMAPN_stLen))) {
            return  (pmapn);
        }
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __mmapNodeFindShare
** 功能描述: 查找 mmap 可以 share 的节点.
** 输　入  : pmapnAbort         mmap node
**           ulAbortAddr        发生缺页中断的地址 (页面对齐)
**           pfuncCallback      回调函数
** 输　出  : 回调函数返回值
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0

static PVOID __mmapNodeFindShare (__PX_MAP_NODE  *pmapnAbort,
                                  addr_t          ulAbortAddr,
                                  PVOID         (*pfuncCallback)(PVOID  pvStartAddr, size_t  stOffset))
{
    __PX_MAP_NODE      *pmapn;
    PLW_LIST_LINE       plineTemp;
    PVOID               pvRet = LW_NULL;
    off_t               oft;                                            /*  一定是 VMM 页面对齐         */
    
    oft = ((off_t)(ulAbortAddr - (addr_t)pmapnAbort->PMAPN_pvAddr) + pmapnAbort->PMAPN_off);
    
    for (plineTemp  = _G_plineMMapHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
         
        pmapn = (__PX_MAP_NODE *)plineTemp;
        
        if (pmapn != pmapnAbort) {
            if ((pmapn->PMAPN_iFd      >= 0) &&
                (pmapnAbort->PMAPN_iFd >= 0) &&
                (pmapn->PMAPN_dev   == pmapnAbort->PMAPN_dev) &&
                (pmapn->PMAPN_ino64 == pmapnAbort->PMAPN_ino64)) {      /*  映射的文件相同              */
                
                
                if ((oft >= pmapn->PMAPN_off) &&
                    (oft <  (pmapn->PMAPN_off + pmapn->PMAPN_stLen))) { /*  范围以内                    */
                    
                    pvRet = pfuncCallback(pmapn->PMAPN_pvAddr,
                                          (size_t)(oft - pmapn->PMAPN_off));
                    if (pvRet) {
                        break;
                    }
                }
            }
        }
    }
    
    return  (pvRet);
}

#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
** 函数名称: __mmapNodeAreaInsert
** 功能描述: 将一个释放的区域记录插入 mmap node 区域表
** 输　入  : pmapn     mmap node
**           pmaparea  区域记录
** 输　出  : 如果遇到地址冲突, 返回 PX_ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __mmapNodeAreaInsert (__PX_MAP_NODE  *pmapn, __PX_MAP_AREA  *pmaparea)
{
             PLW_LIST_LINE       plineTemp;
             
    REGISTER __PX_MAP_AREA      *pmapareaTemp;
    REGISTER __PX_MAP_AREA      *pmapareaLeft;
    REGISTER __PX_MAP_AREA      *pmapareaRight;
    
    if (pmapn->PMAPN_plineUnmap == LW_NULL) {                           /*  还没有 unmap 过             */
        _List_Line_Add_Ahead(&pmaparea->PMAPA_lineManage, 
                             &pmapn->PMAPN_plineUnmap);                 /*  插入表头                    */
        return  (ERROR_NONE);
    }
    
    for (plineTemp  = pmapn->PMAPN_plineUnmap;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
    
        pmapareaTemp = (__PX_MAP_AREA *)plineTemp;
        
        if ((addr_t)pmapareaTemp->PMAPN_pvAddr > (addr_t)pmaparea->PMAPN_pvAddr) {
            
            pmapareaRight = pmapareaTemp;                               /*  此语句增强可阅读性...       */
            /*
             *  检测当前区域是否与右边区域有重叠
             */
            if (((addr_t)pmaparea->PMAPN_pvAddr + pmaparea->PMAPN_stLen) >
                (addr_t)pmapareaRight->PMAPN_pvAddr) {
                return  (PX_ERROR);                                     /*  有重叠                      */
            
            } else {
                _List_Line_Add_Left(&pmaparea->PMAPA_lineManage, 
                                    &pmapareaRight->PMAPA_lineManage);  /*  插到左边                    */
                return  (ERROR_NONE);
            }
        } else {
            pmapareaLeft = pmapareaTemp;                                /*  此语句增强可阅读性...       */
            /*
             *  检测是否有区域重叠
             */
            if (((addr_t)pmapareaLeft->PMAPN_pvAddr + pmapareaLeft->PMAPN_stLen) >
                (addr_t)pmaparea->PMAPN_pvAddr) {
                return  (PX_ERROR);                                     /*  有重叠                      */
            }
        }
    }
    
    /*
     *  当前节点的地址比所有的节点都大
     */
    if (((addr_t)pmaparea->PMAPN_pvAddr + pmaparea->PMAPN_stLen) >
        ((addr_t)pmapn->PMAPN_pvAddr + pmapn->PMAPN_stLen)) {           /*  不能超出 mmap 映射范围      */
        return  (PX_ERROR);
    
    } else {
        _List_Line_Add_Right(&pmaparea->PMAPA_lineManage,
                             &pmapareaTemp->PMAPA_lineManage);          /*  插入到最后一个节点右边      */
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: __mmapNodeFree
** 功能描述: 释放一个指定 mmap node 所有的区域管理内存
** 输　入  : pmapn     mmap node
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __mmapNodeAreaFree (__PX_MAP_NODE  *pmapn)
{
    REGISTER PLW_LIST_LINE  plineDel;
    
    while (pmapn->PMAPN_plineUnmap) {                                   /*  删除所有的区域节点          */
        plineDel = pmapn->PMAPN_plineUnmap;
        _List_Line_Del(plineDel, &pmapn->PMAPN_plineUnmap);
        __SHEAP_FREE(plineDel);
    }
}
/*********************************************************************************************************
** 函数名称: __mmapMallocAreaFill
** 功能描述: 缺页中断分配内存后, 将通过此函数填充文件数据 (注意, 此函数在 vmm lock 中执行!)
** 输　入  : pmapn              mmap node
**           ulDestPageAddr     拷贝目标地址 (页面对齐)
**           ulMapPageAddr      最终会被映射的目标地址 (页面对齐)
**           ulPageNum          新分配出的页面个数
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0

static INT  __mmapMallocAreaFill (__PX_MAP_NODE    *pmapn, 
                                  addr_t            ulDestPageAddr,
                                  addr_t            ulMapPageAddr, 
                                  ULONG             ulPageNum)
{
    off_t       offtRead;
    
    size_t      stReadLen;
    addr_t      ulMapStartAddr = (addr_t)pmapn->PMAPN_pvAddr;
    
    if (pmapn->PMAPN_pid != getpid()) {                                 /*  如果不是创建进程            */
        goto    __full_with_zero;
    }
    
    if (!S_ISREG(pmapn->PMAPN_mode)) {                                  /*  非数据文件类型              */
        goto    __full_with_zero;
    }
    
    if ((ulMapPageAddr < ulMapStartAddr) || 
        (ulMapPageAddr > (ulMapStartAddr + pmapn->PMAPN_stLen))) {      /*  致命错误, 页面内存地址错误  */
        goto    __full_with_zero;
    }
    
    offtRead  = (off_t)(ulMapPageAddr - ulMapStartAddr);                /*  内存地址偏移                */
    offtRead += pmapn->PMAPN_off;                                       /*  加上文件起始偏移            */
    
    stReadLen = (size_t)(ulPageNum * LW_CFG_VMM_PAGE_SIZE);             /*  需要获取的数据大小          */
    
    /*
     *  暂时不处理文件读取错误的情况. (注意, 最终是将文件内容拷入 ulDestPageAddr)
     */
    {
        INT      iZNum;
        ssize_t  sstNum = API_IosPRead(pmapn->PMAPN_iFd, 
                                       (PCHAR)ulDestPageAddr, stReadLen,
                                       offtRead);                       /*  读取文件内容                */
        
        sstNum = (sstNum >= 0) ? sstNum : 0;
        iZNum = (INT)(stReadLen - sstNum);
        
        if (iZNum > 0) {
            lib_bzero((PVOID)(ulDestPageAddr + sstNum), iZNum);         /*  未使用部分清零              */
        }
    }
    
    return  (ERROR_NONE);
    
__full_with_zero:
    lib_bzero((PVOID)ulDestPageAddr, 
              (INT)(ulPageNum * LW_CFG_VMM_PAGE_SIZE));                 /*  全部清零                    */
    
    return  (ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
** 函数名称: __mmapMalloc
** 功能描述: 分配内存
** 输　入  : pmapnode      mmap node
**           stLen         内存大小
**           pvArg         填充函数参数
**           iFlags        mmap flags
**           ulFlag        内存属性
** 输　出  : 内存地址
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static PVOID  __mmapMalloc (__PX_MAP_NODE  *pmapnode, size_t  stLen, PVOID  pvArg, 
                            INT  iFlags, ULONG  ulFlag)
{
#if LW_CFG_VMM_EN > 0
    PVOID    pvMem = API_VmmMallocAreaEx(stLen, __mmapMallocAreaFill, pvArg, iFlags, ulFlag);
    
    if (pvMem == LW_NULL) {
        pvMem =  __SHEAP_ALLOC_ALIGN(stLen, LW_CFG_VMM_PAGE_SIZE);
        pmapnode->PMAPN_bIsHeapMem = LW_TRUE;
    } else {
        pmapnode->PMAPN_bIsHeapMem = LW_FALSE;
        API_VmmSetFindShare(pvMem, __mmapNodeFindShare, pvArg);
    }

#else
    PVOID    pvMem = __SHEAP_ALLOC_ALIGN(stLen, LW_CFG_VMM_PAGE_SIZE);
    
    pmapnode->PMAPN_bIsHeapMem = LW_TRUE;
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */

    return  (pvMem);
}
/*********************************************************************************************************
** 函数名称: __mmapFree
** 功能描述: 内存释放
** 输　入  : pmapnode      mmap node
**           pvAddr        内存地址
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __mmapFree (__PX_MAP_NODE  *pmapnode, PVOID  pvAddr)
{
#if LW_CFG_VMM_EN > 0
    if (pmapnode->PMAPN_bIsHeapMem == LW_FALSE) {
        API_VmmFreeArea(pvAddr);
    } else {
        __SHEAP_FREE(pvAddr);
    }
#else
    __SHEAP_FREE(pvAddr);
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
}
/*********************************************************************************************************
** 函数名称: mmapfd
** 功能描述: 获得 mmap 区域的文件描述符
** 输　入  : pvAddr        地址
** 输　出  : 文件描述符
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  mmapfd (void  *pvAddr)
{
    __PX_MAP_NODE       *pmapnode;
    int                  iFd;
    
    __PX_LOCK();
    pmapnode = __mmapNodeFind(pvAddr);
    if (pmapnode == LW_NULL) {
        __PX_UNLOCK();
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    iFd = pmapnode->PMAPN_iFd;
    __PX_UNLOCK();
    
    return  (iFd);
}
/*********************************************************************************************************
** 函数名称: mmap
** 功能描述: 内存文件映射函数, 详见: http://www.opengroup.org/onlinepubs/000095399/functions/mmap.html
** 输　入  : pvAddr        起始地址 (这里必须为 NULL, 系统将自动分配内存)
**           stLen         映射长度
**           iProt         页面属性
**           iFlag         映射标志
**           iFd           文件描述符
**           off           文件偏移量
** 输　出  : 文件映射后的内存地址
** 全局变量: 
** 调用模块: 
** 注  意  : 如果是 REG 文件, 也首先使用驱动程序的 mmap. 共享内存设备的 mmap 直接会完成映射.
                                           API 函数
*********************************************************************************************************/
LW_API  
void  *mmap (void  *pvAddr, size_t  stLen, int  iProt, int  iFlag, int  iFd, off_t  off)
{
    __PX_MAP_NODE       *pmapnode;
    struct stat64        stat64Fd;
    ULONG                ulFlag = LW_VMM_FLAG_READ;
    
#if LW_CFG_VMM_EN > 0
    LW_DEV_MMAP_AREA     dmap;
    INT                  iError;
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
    
    INT                  iErrLevel = 0;

    if (pvAddr != LW_NULL) {                                            /*  这里必须为 NULL             */
        errno = EINVAL;
        return  (MAP_FAILED);
    }
    
    if (iProt & (~(PROT_READ | PROT_WRITE | PROT_EXEC))) {
        errno = ENOTSUP;
        return  (MAP_FAILED);
    }
    
    if (iProt & PROT_WRITE) {
        ulFlag |= LW_VMM_FLAG_RDWR;                                     /*  可读写                      */
    }
    
    if (iProt & PROT_EXEC) {
        ulFlag |= LW_VMM_FLAG_EXEC;                                     /*  可执行                      */
    }
    
    if (iFlag & MAP_FIXED) {                                            /*  不支持 FIX                  */
        errno = ENOTSUP;
        return  (MAP_FAILED);
    }
    
    if (!ALIGNED(off, LW_CFG_VMM_PAGE_SIZE) || stLen == 0) {            /*  off 必须页对齐              */
        errno = EINVAL;
        return  (MAP_FAILED);
    }
    
    if (!(iFlag & MAP_ANONYMOUS)) {                                     /*  与文件描述符相关            */
        if (fstat64(iFd, &stat64Fd) < 0) {                              /*  获得文件 stat               */
            errno = EBADF;
            return  (MAP_FAILED);
        }
        
        if (off > stat64Fd.st_size) {                                   /*  off 越界                    */
            errno = ENXIO;
            return  (MAP_FAILED);
        }
        
    } else {
        iFd = PX_ERROR;
        stat64Fd.st_mode = (mode_t)0;                                   /*  文件描述符无关              */
    }
    
    if (iFlag & MAP_SHARED) {                                           /*  多进程共享                  */
#if LW_CFG_CACHE_EN > 0
        if (API_CacheLocation(DATA_CACHE) == CACHE_LOCATION_VIVT) {     /*  处理器为虚拟地址 CACHE      */
            ulFlag &= ~(LW_VMM_FLAG_CACHEABLE | LW_VMM_FLAG_BUFFERABLE);/*  共享内存不允许 CACHE        */
        }
#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */
    }
    
    pmapnode = (__PX_MAP_NODE *)__SHEAP_ALLOC(sizeof(__PX_MAP_NODE));   /*  创建控制块                  */
    if (pmapnode == LW_NULL) {
        errno = ENOMEM;
        return  (MAP_FAILED);
    }
    
    pmapnode->PMAPN_pvAddr = __mmapMalloc(pmapnode, stLen, (PVOID)pmapnode, iFlag, ulFlag);
    if (pmapnode->PMAPN_pvAddr == LW_NULL) {                            /*  申请映射内存                */
        errno     = ENOMEM;
        iErrLevel = 1;
        goto    __error_handle;
    }
    
    pmapnode->PMAPN_stLen    = stLen;
    pmapnode->PMAPN_ulFlag   = ulFlag;
    pmapnode->PMAPN_iFd      = iFd;
    pmapnode->PMAPN_mode     = stat64Fd.st_mode;
    pmapnode->PMAPN_off      = off;
    pmapnode->PMAPN_offFSize = stat64Fd.st_size;
    pmapnode->PMAPN_iFlag    = iFlag;
    pmapnode->PMAPN_bBusy    = LW_FALSE;
    
    pmapnode->PMAPN_dev      = stat64Fd.st_dev;
    pmapnode->PMAPN_ino64    = stat64Fd.st_ino;
    
    pmapnode->PMAPN_plineUnmap   = LW_NULL;                             /*  没有 unmap                  */
    pmapnode->PMAPN_stTotalUnmap = 0;
    
    if (!(iFlag & MAP_ANONYMOUS)) {                                     /*  与文件描述符相关            */
    
        API_IosFdRefInc(iFd);                                           /*  对文件描述符引用 ++         */
    
        if (S_ISREG(stat64Fd.st_mode)) {                                /*  普通数据文件                */
            if (pmapnode->PMAPN_bIsHeapMem) {                           /*  非缺页中断内存              */
                pread(iFd, (PVOID)pmapnode->PMAPN_pvAddr, stLen, off);  /*  读取文件内容                */
            
            } else {                                                    /*  使用缺页中断                */
#if LW_CFG_VMM_EN > 0
                dmap.DMAP_pvAddr   = pmapnode->PMAPN_pvAddr;
                dmap.DMAP_stLen    = pmapnode->PMAPN_stLen;
                dmap.DMAP_offPages = (pmapnode->PMAPN_off >> LW_CFG_VMM_PAGE_SHIFT);
                dmap.DMAP_ulFlag   = ulFlag;
                
                iError = API_IosMmap(iFd, &dmap);                       /*  尝试调用设备驱动            */
                if ((iError < ERROR_NONE) && 
                    (errno != ERROR_IOS_DRIVER_NOT_SUP)) {              /*  驱动程序报告错误            */
                    iErrLevel = 2;
                    goto    __error_handle;
                }
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
            }
        } else {                                                        /*  非 REG 文件                 */
            if (pmapnode->PMAPN_bIsHeapMem) {
                errno     = ENOSYS;
                iErrLevel = 2;
                goto    __error_handle;
            }
#if LW_CFG_VMM_EN > 0
            dmap.DMAP_pvAddr   = pmapnode->PMAPN_pvAddr;
            dmap.DMAP_stLen    = pmapnode->PMAPN_stLen;
            dmap.DMAP_offPages = (pmapnode->PMAPN_off >> LW_CFG_VMM_PAGE_SHIFT);
            dmap.DMAP_ulFlag   = ulFlag;
            
            iError = API_IosMmap(iFd, &dmap);                           /*  调用设备驱动                */
            if ((iError < ERROR_NONE) && 
                (errno != ERROR_IOS_DRIVER_NOT_SUP)) {                  /*  驱动程序报告错误            */
                iErrLevel = 2;
                goto    __error_handle;
            }
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
        }
    }
    
    __PX_LOCK();
    _List_Line_Add_Ahead(&pmapnode->PMAPN_lineManage, 
                         &_G_plineMMapHeader);                          /*  加入管理链表                */
    __PX_UNLOCK();
    
    pmapnode->PMAPN_pid = getpid();                                     /*  获得当前进程 pid            */
    __resAddRawHook(&pmapnode->PMAPN_resraw, (VOIDFUNCPTR)munmap, 
                    pmapnode->PMAPN_pvAddr, (PVOID)stLen, 0, 0, 0, 0);  /*  加入资源管理器              */

    MONITOR_EVT_LONG5(MONITOR_EVENT_ID_VMM, MONITOR_EVENT_VMM_MMAP,
                      pmapnode->PMAPN_pvAddr, 
                      iFd, stLen, iProt, iFlag, LW_NULL);

    return  (pmapnode->PMAPN_pvAddr);
    
__error_handle:
    if (iErrLevel > 1) {
        API_IosFdRefDec(iFd);                                           /*  对文件描述符引用 --         */
        __mmapFree(pmapnode, pmapnode->PMAPN_pvAddr);
    }
    if (iErrLevel > 0) {
        __SHEAP_FREE(pmapnode);
    }
    
    return  (MAP_FAILED);
}
/*********************************************************************************************************
** 函数名称: mmap64 (sylixos 内部 off_t 本身就是 64bit)
** 功能描述: 内存文件映射函数, 详见: http://www.opengroup.org/onlinepubs/000095399/functions/mmap.html
** 输　入  : pvAddr        起始地址 (这里必须为 NULL, 系统将自动分配内存)
**           stLen         映射长度
**           iProt         页面属性
**           iFlag         映射标志
**           iFd           文件描述符
**           off           文件偏移量
** 输　出  : 文件映射后的内存地址
** 全局变量: 
** 调用模块: 
** 注  意  : 如果是 REG 文件, 也首先使用驱动程序的 mmap. 共享内存设备的 mmap 直接会完成映射.
                                           API 函数
*********************************************************************************************************/
LW_API  
void  *mmap64 (void  *pvAddr, size_t  stLen, int  iProt, int  iFlag, int  iFd, off64_t  off)
{
    return  (mmap(pvAddr, stLen, iProt, iFlag, iFd, (off_t)off));
}
/*********************************************************************************************************
** 函数名称: munmap
** 功能描述: 取消内存文件映射
** 输　入  : pvAddr        起始地址
**           stLen         映射长度
** 输　出  : ERROR_NONE or PX_ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  munmap (void  *pvAddr, size_t  stLen)
{
    __PX_MAP_NODE       *pmapnode;
    __PX_MAP_AREA       *pmaparea;
    
#if LW_CFG_VMM_EN > 0
    LW_DEV_MMAP_AREA     dmap;
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
    
    
    if (!ALIGNED(pvAddr, LW_CFG_VMM_PAGE_SIZE) || stLen == 0) {         /*  pvAddr 必须页对齐           */
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    pmaparea = (__PX_MAP_AREA *)__SHEAP_ALLOC(sizeof(__PX_MAP_AREA));   /*  创建内存区域节点            */
    if (pmaparea == LW_NULL) {
        errno = ENOMEM;
        return  (PX_ERROR);
    }
    
    __PX_LOCK();
    pmapnode = __mmapNodeFind(pvAddr);
    if (pmapnode == LW_NULL) {
        __PX_UNLOCK();
        __SHEAP_FREE(pmaparea);
        errno = EINVAL;
        return  (PX_ERROR);
    }
    if (pmapnode->PMAPN_bBusy) {
        __PX_UNLOCK();
        __SHEAP_FREE(pmaparea);
        errno = EBUSY;
        return  (PX_ERROR);
    }
    
    pmaparea->PMAPN_pvAddr = pvAddr;
    pmaparea->PMAPN_stLen  = stLen;
    
    if (__mmapNodeAreaInsert(pmapnode, pmaparea) < ERROR_NONE) {
        __PX_UNLOCK();
        __SHEAP_FREE(pmaparea);
        errno = EINVAL;                                                 /*  解除映射的内存区域错误      */
        return  (PX_ERROR);
    }
    
    pmapnode->PMAPN_stTotalUnmap += stLen;
    
    if (pmapnode->PMAPN_stTotalUnmap >= pmapnode->PMAPN_stLen) {        /*  所有的内存都是 unmap 了     */
        _List_Line_Del(&pmapnode->PMAPN_lineManage, 
                       &_G_plineMMapHeader);                            /*  从管理链表中删除            */
        __PX_UNLOCK();
        
        if ((pmapnode->PMAPN_iFd >= 0) &&
            (pmapnode->PMAPN_pid == getpid())) {                        /*  注意: 不是创建进程不减少引用*/
            
#if LW_CFG_VMM_EN > 0
            dmap.DMAP_pvAddr   = pmapnode->PMAPN_pvAddr;
            dmap.DMAP_stLen    = pmapnode->PMAPN_stLen;
            dmap.DMAP_offPages = (pmapnode->PMAPN_off >> LW_CFG_VMM_PAGE_SHIFT);
            dmap.DMAP_ulFlag   = pmapnode->PMAPN_ulFlag;
            API_IosUnmap(pmapnode->PMAPN_iFd, &dmap);                   /*  调用设备驱动                */
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
            
            API_IosFdRefDec(pmapnode->PMAPN_iFd);                       /*  对文件描述符引用 --         */
        }
        
        __mmapNodeAreaFree(pmapnode);                                   /*  释放区域管理内存            */
        
        __mmapFree(pmapnode, pmapnode->PMAPN_pvAddr);                   /*  释放内存                    */
        
#if LW_CFG_MODULELOADER_EN > 0
        __resDelRawHook(&pmapnode->PMAPN_resraw);
#endif                                                                  /*  LW_CFG_MODULELOADER_EN      */
        
        __SHEAP_FREE(pmapnode);
        
        MONITOR_EVT_LONG2(MONITOR_EVENT_ID_VMM, MONITOR_EVENT_VMM_MUNMAP,
                          pvAddr, stLen, LW_NULL);
        
        return  (ERROR_NONE);
    
    } else {
        pmapnode->PMAPN_bBusy = LW_TRUE;                                /*  设置为忙状态                */
        __PX_UNLOCK();
        
#if LW_CFG_VMM_EN > 0
        API_VmmInvalidateArea(pmapnode->PMAPN_pvAddr, pvAddr, stLen);   /*  释放相关区域物理内存        */
                                                                        /*  SylixOS 暂不支持虚拟空间拆散*/
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
        pmapnode->PMAPN_bBusy = LW_FALSE;
        
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: msync
** 功能描述: 将内存中映射的文件数据回写文件
** 输　入  : pvAddr        起始地址
**           stLen         映射长度
**           iFlag         回写属性
** 输　出  : ERROR_NONE or PX_ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  msync (void  *pvAddr, size_t  stLen, int  iFlag)
{
    __PX_MAP_NODE       *pmapnode;
    off_t                off;
    size_t               stWriteLen;
    
    if (!ALIGNED(pvAddr, LW_CFG_VMM_PAGE_SIZE) || stLen == 0) {         /*  pvAddr 必须页对齐           */
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    __PX_LOCK();
    pmapnode = __mmapNodeFind(pvAddr);
    if (pmapnode == LW_NULL) {
        __PX_UNLOCK();
        errno = EINVAL;
        return  (PX_ERROR);
    }
    if (pmapnode->PMAPN_bBusy) {
        __PX_UNLOCK();
        errno = EBUSY;
        return  (PX_ERROR);
    }
    if (!S_ISREG(pmapnode->PMAPN_mode)) {
        __PX_UNLOCK();
        return  (ERROR_NONE);                                           /*  非数据文件直接退出          */
    }
    if (((addr_t)pvAddr + stLen) > 
        ((addr_t)pmapnode->PMAPN_pvAddr + pmapnode->PMAPN_stLen)) {
        __PX_UNLOCK();
        errno = ENOMEM;                                                 /*  需要同步的内存越界          */
        return  (PX_ERROR);
    }
    pmapnode->PMAPN_bBusy = LW_TRUE;                                    /*  设置为忙状态                */
    __PX_UNLOCK();
    
    off = pmapnode->PMAPN_off 
        + (off_t)((addr_t)pvAddr - (addr_t)pmapnode->PMAPN_pvAddr);     /*  计算写入/无效文件偏移量     */
        
    stWriteLen = (size_t)(pmapnode->PMAPN_offFSize - off);
    stWriteLen = (stWriteLen > stLen) ? stLen : stWriteLen;             /*  确定写入/无效文件的长度     */
    
    if (!(iFlag & MS_INVALIDATE)) {
                 INT        i;
        REGISTER ULONG      ulPageNum = (ULONG) (stWriteLen >> LW_CFG_VMM_PAGE_SHIFT);
        REGISTER size_t     stExcess  = (size_t)(stWriteLen & ~LW_CFG_VMM_PAGE_MASK);
                 addr_t     ulAddr    = (addr_t)pvAddr;
#if LW_CFG_VMM_EN > 0
                 ULONG      ulFlag;
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
                 
        for (i = 0; i < ulPageNum; i++) {
#if LW_CFG_VMM_EN > 0
            if ((API_VmmGetFlag((PVOID)ulAddr, &ulFlag) == ERROR_NONE) &&
                (ulFlag & LW_VMM_FLAG_WRITABLE)) {                      /*  内存必须有效才可写          */
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
                if (pwrite(pmapnode->PMAPN_iFd, (CPVOID)ulAddr, 
                           LW_CFG_VMM_PAGE_SIZE, off) != 
                           LW_CFG_VMM_PAGE_SIZE) {                      /*  写入文件                    */
                    pmapnode->PMAPN_bBusy = LW_FALSE;
                    return  (PX_ERROR);
                }
#if LW_CFG_VMM_EN > 0
            }
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
            ulAddr += LW_CFG_VMM_PAGE_SIZE;
            off    += LW_CFG_VMM_PAGE_SIZE;
        }
        
        if (stExcess) {
#if LW_CFG_VMM_EN > 0
            if ((API_VmmGetFlag((PVOID)ulAddr, &ulFlag) == ERROR_NONE) &&
                (ulFlag & LW_VMM_FLAG_WRITABLE)) {                      /*  内存必须有效才可写          */
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
                if (pwrite(pmapnode->PMAPN_iFd, (CPVOID)ulAddr, 
                           stExcess, off) != stExcess) {                /*  写入文件                    */
                    pmapnode->PMAPN_bBusy = LW_FALSE;
                    return  (PX_ERROR);
                }
#if LW_CFG_VMM_EN > 0
            }
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
        }
                 
        if (iFlag & MS_SYNC) {
            ioctl(pmapnode->PMAPN_iFd, FIOSYNC, 0);
        }
    } else {                                                            /*  需要读出文件内容            */
    
        if (pmapnode->PMAPN_bIsHeapMem) {                               /*  heap 分配内存               */
            caddr_t     pcAddr  = (caddr_t)pvAddr;
            size_t      stTotal = 0;
            ssize_t     sstReadNum;
            off_t       oftSeek = off;
            
            while (stTotal < stWriteLen) {
                sstReadNum = pread(pmapnode->PMAPN_iFd, &pcAddr[stTotal], (stWriteLen - stTotal), oftSeek);
                if (sstReadNum <= 0) {
                    break;
                }
                stTotal += (size_t)sstReadNum;
                oftSeek += (off_t)sstReadNum;
            }
            
            if (stTotal < stWriteLen) {
                pmapnode->PMAPN_bBusy = LW_FALSE;
                errno = EIO;
                return  (PX_ERROR);
            }
        } else {                                                        /*  缺页中断式非配内存          */
#if LW_CFG_VMM_EN > 0
            API_VmmInvalidateArea(pmapnode->PMAPN_pvAddr, 
                                  pvAddr, 
                                  stLen);                               /*  释放对应的物理内存          */
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
        }
    }

    pmapnode->PMAPN_bBusy = LW_FALSE;
    
    MONITOR_EVT_LONG3(MONITOR_EVENT_ID_VMM, MONITOR_EVENT_VMM_MSYNC,
                      pvAddr, stLen, iFlag, LW_NULL);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: mmapShow
** 功能描述: 显示当前系统映射的所有文件内存.
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
VOID  mmapShow (VOID)
{
    const CHAR          cMmapInfoHdr[] = 
    "  ADDR     SIZE        OFFSET      WRITE SHARE  PID   FD\n"
    "-------- -------- ---------------- ----- ----- ----- ----\n";
    
    __PX_MAP_NODE      *pmapn;
    PLW_LIST_LINE       plineTemp;
    PCHAR               pcWrite;
    PCHAR               pcShare;
    
    printf(cMmapInfoHdr);
    
    __PX_LOCK();
    for (plineTemp  = _G_plineMMapHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
        
        pmapn = (__PX_MAP_NODE *)plineTemp;
        
        if (pmapn->PMAPN_ulFlag & LW_VMM_FLAG_WRITABLE) {
            pcWrite = "true ";
        } else {
            pcWrite = "false";
        }
        
        if (pmapn->PMAPN_iFlag & MAP_SHARED) {
            pcShare = "true ";
        } else {
            pcShare = "false";
        }
        
        printf("%08lx %8lx %16llx %s %s %5d %4d\n", 
               (addr_t)pmapn->PMAPN_pvAddr,
               (ULONG)pmapn->PMAPN_stLen,
               pmapn->PMAPN_off,
               pcWrite,
               pcShare,
               pmapn->PMAPN_pid,
               pmapn->PMAPN_iFd);
    }
    __PX_UNLOCK();
    
    printf("\n");
}
/*********************************************************************************************************
** 函数名称: shm_open
** 功能描述: establishes a connection between a shared memory object and a file descriptor.
** 输　入  : name      file name
**           oflag     create flag like open()
**           mode      create mode like open()
** 输　出  : filedesc
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  shm_open (const char *name, int oflag, mode_t mode)
{
    CHAR    cFullName[MAX_FILENAME_LENGTH] = "/dev/shm/";
    
    if (!name || (*name == PX_EOS)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if (name[0] == PX_ROOT) {
        lib_strlcat(cFullName, &name[1], MAX_FILENAME_LENGTH);
    } else {
        lib_strlcat(cFullName, name, MAX_FILENAME_LENGTH);
    }
    
    return  (open(cFullName, oflag, mode));
}
/*********************************************************************************************************
** 函数名称: shm_unlink
** 功能描述: removes the name of the shared memory object named by the string pointed to by name.
** 输　入  : name      file name
** 输　出  : filedesc
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  shm_unlink (const char *name)
{
    CHAR    cFullName[MAX_FILENAME_LENGTH] = "/dev/shm/";
    
    if (!name || (*name == PX_EOS)) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    if (name[0] == PX_ROOT) {
        lib_strlcat(cFullName, &name[1], MAX_FILENAME_LENGTH);
    } else {
        lib_strlcat(cFullName, name, MAX_FILENAME_LENGTH);
    }
    
    return  (unlink(cFullName));
}
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
                                                                        /*  LW_CFG_DEVICE_EN > 0        */
/*********************************************************************************************************
  END
*********************************************************************************************************/
