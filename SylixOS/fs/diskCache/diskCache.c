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
** 文   件   名: diskCache.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 11 月 26 日
**
** 描        述: 磁盘高速缓冲控制器.

** BUG
2008.12.03  创建时不需要检测 iMaxBurstSector 的大小.
2009.03.11  修改 hash 表的大小. 素数.
2009.03.22  DISK CACHE 初始化为物理磁盘驱动, 所以 DISK CACHE 必须与物理设备一一对应.
2009.03.23  加入对北京回写任务的支持.
2009.03.25  获得设备扇区大小前需要初始化磁盘.
2009.11.03  加入对移动设备的处理.
2009.12.11  加入 /proc 文件初始化.
2014.07.29  猝发读写缓冲一定使用页对其的内存操作, 否则可能造成底层 DMA 与 CACHE 一致性操作问题(invalidate)
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "diskCacheLib.h"
#include "diskCache.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_MAX_VOLUMES > 0) && (LW_CFG_DISKCACHE_EN > 0)
/*********************************************************************************************************
  一个物理 CF 卡使用 DISK CACHE 结构示例:
  
CF card disk volume:
  +------------+
  |   /cf/0    |
  |    FAT     |
  +------------+
        \
         \______________________
                                \
disk cache block device:         \
                           +------------+
                           | DISK CACHE |
                           |   BLK_DEV  |
                           +------------+
                                 \
                                  \________________________
                                                           \
physical block device:                                      \
                                                       +------------+
                                                       |  PHYSICAL  |
                                                       |   BLK_DEV  |
                                                       +------------+
*********************************************************************************************************/
/*********************************************************************************************************
  相关参数表
*********************************************************************************************************/
static const INT    _G_iDiskCacheHashSize[][2] = {
/*********************************************************************************************************
    CACHE SIZE      HASH SIZE
     (sector)        (entry)
*********************************************************************************************************/
{         16,            7,         },
{         32,           13,         },
{         64,           41,         },
{        128,           97,         },
{        256,          191,         },
{        512,          397,         },
{       1024,          853,         },
{       2048,         1559,         },
{          0,         1999,         }
};
/*********************************************************************************************************
  背景线程属性
*********************************************************************************************************/
static LW_CLASS_THREADATTR      _G_threadattrDiskCacheAttr = {
    LW_NULL,
    LW_CFG_THREAD_DEFAULT_GUARD_SIZE,
    LW_CFG_THREAD_DISKCACHE_STK_SIZE,
    LW_PRIO_T_CACHE,
    LW_CFG_DISKCACHE_OPTION | LW_OPTION_OBJECT_GLOBAL,                  /*  内核全局线程                */
    LW_NULL,
    LW_NULL,
};
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
INT  __diskCacheMemInit(PLW_DISKCACHE_CB   pdiskcDiskCache,
                        VOID              *pvDiskCacheMem,
                        ULONG              ulBytesPerSector,
                        ULONG              ulNSector);
/*********************************************************************************************************
  驱动函数声明
*********************************************************************************************************/
INT  __diskCacheRead(PLW_DISKCACHE_CB   pdiskcDiskCache,
                     VOID              *pvBuffer, 
                     ULONG              ulStartSector, 
                     ULONG              ulSectorCount);
INT  __diskCacheWrite(PLW_DISKCACHE_CB   pdiskcDiskCache,
                      VOID              *pvBuffer, 
                      ULONG              ulStartSector, 
                      ULONG              ulSectorCount);
INT  __diskCacheIoctl(PLW_DISKCACHE_CB   pdiskcDiskCache, INT  iCmd, LONG  lArg);
INT  __diskCacheReset(PLW_DISKCACHE_CB   pdiskcDiskCache);
INT  __diskCacheStatusChk(PLW_DISKCACHE_CB   pdiskcDiskCache);
/*********************************************************************************************************
  proc 初始化函数声明
*********************************************************************************************************/
#if LW_CFG_PROCFS_EN > 0
VOID  __procFsDiskCacheInit(VOID);
#endif                                                                  /*  LW_CFG_PROCFS_EN > 0        */
/*********************************************************************************************************
** 函数名称: API_DiskCacheCreate
** 功能描述: 创建一个磁盘 CACHE 块设备.
** 输　入  : pblkdDisk          需要 CACHE 的块设备
**           pvDiskCacheMem     磁盘 CACHE 缓冲区的内存起始地址
**           stMemSize          磁盘 CACHE 缓冲区大小
**           iMaxBurstSector    磁盘猝发读写的最大扇区数
**           ppblkDiskCache     创建出来了 CACHE 块设备.
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ULONG  API_DiskCacheCreate (PLW_BLK_DEV   pblkdDisk, 
                            PVOID         pvDiskCacheMem, 
                            size_t        stMemSize, 
                            INT           iMaxBurstSector,
                            PLW_BLK_DEV  *ppblkDiskCache)
{
             INT                i;
             INT                iError;
             INT                iErrLevel = 0;

    REGISTER PLW_DISKCACHE_CB   pdiskcDiskCache;
             ULONG              ulBytesPerSector;
             ULONG              ulNSector;
    REGISTER INT                iHashSize;
    
    static   LW_OBJECT_HANDLE   ulDiskCacheBGThread = 0ul;
    
    /*
     *  创建回写锁
     */
    if (!_G_ulDiskCacheListLock) {
        _G_ulDiskCacheListLock = API_SemaphoreMCreate("diskcachelist_lock", 
                                                      LW_PRIO_DEF_CEILING,
                                                      LW_OPTION_WAIT_PRIORITY | 
                                                      LW_OPTION_DELETE_SAFE | 
                                                      LW_OPTION_INHERIT_PRIORITY |
                                                      LW_OPTION_OBJECT_GLOBAL,
                                                      LW_NULL);
        if (!_G_ulDiskCacheListLock) {
            return  (API_GetLastError());
        }
    }
    
    /*
     *  创建背景回写线程, 创建 /proc 中文件节点.
     */
    if (!ulDiskCacheBGThread) {
        ulDiskCacheBGThread = API_ThreadCreate("t_diskcache", 
                                               __diskCacheThread,
                                               &_G_threadattrDiskCacheAttr,
                                               LW_NULL);
        if (!ulDiskCacheBGThread) {
            return  (API_GetLastError());
        }

#if LW_CFG_PROCFS_EN > 0
        __procFsDiskCacheInit();                                        /*  创建 /proc 文件节点         */
#endif                                                                  /*  LW_CFG_PROCFS_EN > 0        */
    }
    
    if (!pblkdDisk || !pvDiskCacheMem || 
        !stMemSize || !ppblkDiskCache) {                                /*  判断参数是否异常            */
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    if (pblkdDisk->BLKD_pfuncBlkIoctl &&
        pblkdDisk->BLKD_pfuncBlkReset) {                                /*  初始化磁盘                  */
        pblkdDisk->BLKD_pfuncBlkIoctl(pblkdDisk, LW_BLKD_CTRL_POWER, LW_BLKD_POWER_ON);
        pblkdDisk->BLKD_pfuncBlkReset(pblkdDisk);
        pblkdDisk->BLKD_pfuncBlkIoctl(pblkdDisk, FIODISKINIT, 0);
    }
    
    if (pblkdDisk->BLKD_ulBytesPerSector) {
        ulBytesPerSector = pblkdDisk->BLKD_ulBytesPerSector;            /*  直接获取扇区大小            */
    } else if (pblkdDisk->BLKD_pfuncBlkIoctl) {
        if (pblkdDisk->BLKD_pfuncBlkIoctl(pblkdDisk, 
                                          LW_BLKD_GET_SECSIZE, 
                                          (LONG)&ulBytesPerSector)) {   /*  通过 IOCTL 获得扇区大小     */
            return  (ERROR_IO_DEVICE_ERROR);
        }
    } else {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pblkdDisk ioctl error.\r\n");
        _ErrorHandle(ERROR_IO_NO_DRIVER);                               /*  pblkdDisk 错误              */
        return  (ERROR_IO_NO_DRIVER);
    }
    
    if (stMemSize < ulBytesPerSector) {                                 /*  无法缓冲一个扇区            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "cache buffer too small.\r\n");
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    ulNSector = stMemSize / ulBytesPerSector;                           /*  可以缓冲的扇区数量          */
    
    /*
     *  开辟控制块内存
     */
    pdiskcDiskCache = (PLW_DISKCACHE_CB)__SHEAP_ALLOC(sizeof(LW_DISKCACHE_CB));
    if (pdiskcDiskCache == LW_NULL) {
        iErrLevel = 1;
        goto    __error_handle;
    }
    lib_bzero(pdiskcDiskCache, sizeof(LW_DISKCACHE_CB));
    
    pdiskcDiskCache->DISKC_pblkdDisk = pblkdDisk;
    pdiskcDiskCache->DISKC_iCacheOpt = LW_DISKCACHE_OPT_ENABLE;
    
    /*
     *  创建操作互斥信号量
     */
    pdiskcDiskCache->DISKC_hDiskCacheLock = API_SemaphoreMCreate("diskcache_lock", 
                                            LW_PRIO_DEF_CEILING,
                                            LW_OPTION_WAIT_PRIORITY | LW_OPTION_DELETE_SAFE | 
                                            LW_OPTION_INHERIT_PRIORITY | LW_OPTION_OBJECT_GLOBAL,
                                            LW_NULL);
    if (!pdiskcDiskCache->DISKC_hDiskCacheLock) {
        __SHEAP_FREE(pdiskcDiskCache);
        return  (API_GetLastError());
    }
    
    /*
     *  开辟猝发缓冲内存
     */
    pdiskcDiskCache->DISKC_pcBurstBuffer = 
        (caddr_t)__SHEAP_ALLOC_ALIGN((size_t)(iMaxBurstSector * ulBytesPerSector),
        LW_CFG_VMM_PAGE_SIZE);                                          /*  SHEAP 物理虚拟地址必须一致  */
    if (pdiskcDiskCache->DISKC_pcBurstBuffer == LW_NULL) {
        iErrLevel = 2;
        goto    __error_handle;
    }
    pdiskcDiskCache->DISKC_ulEndStector     = (ULONG)PX_ERROR;
    pdiskcDiskCache->DISKC_ulBytesPerSector = ulBytesPerSector;
    pdiskcDiskCache->DISKC_iMaxBurstSector  = iMaxBurstSector;
    
    /*
     *  确定 HASH 表的大小
     */
    for (i = 0; ; i++) {
        if (_G_iDiskCacheHashSize[i][0] == 0) {
            iHashSize = _G_iDiskCacheHashSize[i][1];                    /*  最大表大小                  */
            break;
        } else {
            if (ulNSector >= _G_iDiskCacheHashSize[i][0]) {
                continue;
            } else {
                iHashSize = _G_iDiskCacheHashSize[i][1];                /*  确定                        */
                break;
            }
        } 
    }
    
    pdiskcDiskCache->DISKC_pplineHash = 
        (PLW_LIST_LINE *)__SHEAP_ALLOC(sizeof(PVOID) * (size_t)iHashSize);
    if (!pdiskcDiskCache->DISKC_pplineHash) {
        iErrLevel = 3;
        goto    __error_handle;
    }
    pdiskcDiskCache->DISKC_iHashSize = iHashSize;
    
    /*
     *  开辟缓冲内存池
     */
    iError = __diskCacheMemInit(pdiskcDiskCache, pvDiskCacheMem, ulBytesPerSector, ulNSector);
    if (iError < 0) {
        iErrLevel = 4;
        goto    __error_handle;
    }
    pdiskcDiskCache->DISKC_disknLuck = LW_NULL;
    
    /*
     *  初始化 BLK_DEV 基本信息
     */
    pdiskcDiskCache->DISKC_blkdCache = *pdiskcDiskCache->DISKC_pblkdDisk;
                                                                        /*  拷贝需要同步的信息          */
    pdiskcDiskCache->DISKC_blkdCache.BLKD_pfuncBlkRd        = __diskCacheRead;
    pdiskcDiskCache->DISKC_blkdCache.BLKD_pfuncBlkWrt       = __diskCacheWrite;
    pdiskcDiskCache->DISKC_blkdCache.BLKD_pfuncBlkIoctl     = __diskCacheIoctl;
    pdiskcDiskCache->DISKC_blkdCache.BLKD_pfuncBlkReset     = __diskCacheReset;
    pdiskcDiskCache->DISKC_blkdCache.BLKD_pfuncBlkStatusChk = __diskCacheStatusChk;
    
    pdiskcDiskCache->DISKC_blkdCache.BLKD_iLogic        = 0;            /*  CACHE 与物理设备一一对应    */
    pdiskcDiskCache->DISKC_blkdCache.BLKD_uiLinkCounter = 0;
    pdiskcDiskCache->DISKC_blkdCache.BLKD_pvLink        = (PVOID)pdiskcDiskCache->DISKC_pblkdDisk;
    
    pdiskcDiskCache->DISKC_blkdCache.BLKD_uiPowerCounter = 0;           /*  must be 0                   */
    pdiskcDiskCache->DISKC_blkdCache.BLKD_uiInitCounter  = 0;
    
    __diskCacheListAdd(pdiskcDiskCache);                                /*  加入背景线程                */
    
    *ppblkDiskCache = (PLW_BLK_DEV)pdiskcDiskCache;                     /*  保存控制块地址              */
    
    return  (ERROR_NONE);
    
__error_handle:
    if (iErrLevel > 3) {
        __SHEAP_FREE(pdiskcDiskCache->DISKC_pplineHash);
    }
    if (iErrLevel > 2) {
        __SHEAP_FREE(pdiskcDiskCache->DISKC_pcBurstBuffer);
    }
    if (iErrLevel > 1) {
        API_SemaphoreMDelete(&pdiskcDiskCache->DISKC_hDiskCacheLock);
        __SHEAP_FREE(pdiskcDiskCache);
    }
    
    _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
    _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
    return  (ERROR_SYSTEM_LOW_MEMORY);
}
/*********************************************************************************************************
** 函数名称: API_DiskCacheDelete
** 功能描述: 删除一个磁盘 CACHE 块设备.
** 输　入  : pblkdDiskCache     磁盘 CACHE 的块设备
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
** 注  意  : 删除磁盘之前首先要使用 remove 函数卸载卷, 
             例如:
                    BLK_DEV   *pblkdCf;
                    BLK_DEV   *pblkdCfCache;
                
                    cfDiskCreate(..., &pblkdCf);
                    diskCacheCreate(pblkdCf, ..., &pblkdCfCache)
                    fatFsDevCreate("/cf0", pblkdCfCache);
                    ...
                    unlink("/cf0");
                    diskCacheDelete(pblkdCfCache);
                    cfDiskCreate(pblkdCf);
                    
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_DiskCacheDelete (PLW_BLK_DEV   pblkdDiskCache)
{
    REGISTER PLW_DISKCACHE_CB   pdiskcDiskCache = (PLW_DISKCACHE_CB)pblkdDiskCache;

    if (pblkdDiskCache) {
        __LW_DISKCACHE_LOCK(pdiskcDiskCache);                           /*  等待使用权                  */
        
        __diskCacheListDel(pdiskcDiskCache);                            /*  退出背景线程                */
        
        API_SemaphoreMDelete(&pdiskcDiskCache->DISKC_hDiskCacheLock);
        __SHEAP_FREE(pdiskcDiskCache->DISKC_pcCacheNodeMem);
        __SHEAP_FREE(pdiskcDiskCache->DISKC_pplineHash);
        __SHEAP_FREE(pdiskcDiskCache->DISKC_pcBurstBuffer);
        __SHEAP_FREE(pdiskcDiskCache);
        
        return  (ERROR_NONE);
    
    } else {
        _ErrorHandle(ERROR_IOS_DEVICE_NOT_FOUND);
        return  (PX_ERROR);
    }
}
#endif                                                                  /*  (LW_CFG_MAX_VOLUMES > 0)    */
                                                                        /*  (LW_CFG_DISKCACHE_EN > 0)   */
/*********************************************************************************************************
  END
*********************************************************************************************************/
