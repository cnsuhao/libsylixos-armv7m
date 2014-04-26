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
** 文   件   名: diskCacheLib.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 11 月 26 日
**
** 描        述: 磁盘高速缓冲控制器内部操作库.

** BUG
2008.12.16  __diskCacheFlushInvalidate2() 函数指定的次数为回写的脏扇区次数.
2009.03.16  加入读写旁路的支持.
2009.03.18  FIOCANCEL 将 CACHE 停止, 没有回写的数据全部丢弃. 内存回到初始状态.
2009.07.22  增加对 FIOSYNC 的支持.
2009.11.03  FIOCANCEL 命令应该传往磁盘驱动, 使其放弃所有数据.
            FIODISKCHANGE 与 FIOCANCEL 操作相同.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "diskCache.h"
#include "diskCacheLib.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_MAX_VOLUMES > 0) && (LW_CFG_DISKCACHE_EN > 0)
/*********************************************************************************************************
  内部宏操作
*********************************************************************************************************/
#define __LW_DISKCACHE_DISK_READ(pdiskcDiskCache)               \
        pdiskcDiskCache->DISKC_pblkdDisk->BLKD_pfuncBlkRd
#define __LW_DISKCACHE_DISK_WRITE(pdiskcDiskCache)              \
        pdiskcDiskCache->DISKC_pblkdDisk->BLKD_pfuncBlkWrt
#define __LW_DISKCACHE_DISK_IOCTL(pdiskcDiskCache)              \
        pdiskcDiskCache->DISKC_pblkdDisk->BLKD_pfuncBlkIoctl
#define __LW_DISKCACHE_DISK_RESET(pdiskcDiskCache)              \
        pdiskcDiskCache->DISKC_pblkdDisk->BLKD_pfuncBlkReset
#define __LW_DISKCACHE_DISK_STATUS(pdiskcDiskCache)             \
        pdiskcDiskCache->DISKC_pblkdDisk->BLKD_pfuncBlkStatusChk
/*********************************************************************************************************
** 函数名称: __diskCacheMemReset
** 功能描述: 重新初始化磁盘 CACHE 缓冲区内存管理部分
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheMemReset (PLW_DISKCACHE_CB   pdiskcDiskCache)
{
    REGISTER INT                    i;
    REGISTER PLW_DISKCACHE_NODE     pdiskn = (PLW_DISKCACHE_NODE)pdiskcDiskCache->DISKC_pcCacheNodeMem;
    REGISTER PCHAR                  pcData = (PCHAR)pdiskcDiskCache->DISKC_pcCacheMem;
    
    pdiskcDiskCache->DISKC_ulDirtyCounter = 0;                          /*  没有需要回写的扇区          */
    
    pdiskcDiskCache->DISKC_pringLRUHeader = LW_NULL;                    /*  初始化 LRU 表               */
    lib_bzero(pdiskcDiskCache->DISKC_pplineHash,
              (sizeof(PVOID) * pdiskcDiskCache->DISKC_iHashSize));      /*  初始化 HASH 表              */
    
    for (i = 0; i < pdiskcDiskCache->DISKC_ulNCacheNode; i++) {
        pdiskn->DISKN_ulSectorNo = (ULONG)PX_ERROR;
        pdiskn->DISKN_iStatus    = 0;
        pdiskn->DISKN_pcData     = pcData;
        
        _List_Ring_Add_Last(&pdiskn->DISKN_ringLRU, 
                            &pdiskcDiskCache->DISKC_pringLRUHeader);    /*  插入 LRU 表                 */
        _LIST_LINE_INIT_IN_CODE(pdiskn->DISKN_lineHash);                /*  初始化 HASH 表              */
    
        pdiskn++;
        pcData += pdiskcDiskCache->DISKC_ulBytesPerSector;
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __diskCacheMemInit
** 功能描述: 初始化磁盘 CACHE 缓冲区内存管理部分
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           pvDiskCacheMem     缓冲区
**           ulBytesPerSector   扇区大小
**           ulNSector          扇区数量
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheMemInit (PLW_DISKCACHE_CB   pdiskcDiskCache,
                         VOID              *pvDiskCacheMem,
                         ULONG              ulBytesPerSector,
                         ULONG              ulNSector)
{
    REGISTER PCHAR                  pcData = (PCHAR)pvDiskCacheMem;

    pdiskcDiskCache->DISKC_ulNCacheNode = ulNSector;
    pdiskcDiskCache->DISKC_pcCacheMem   = (caddr_t)pcData;              /*  CACHE 缓冲区                */
    
    pdiskcDiskCache->DISKC_pcCacheNodeMem = (caddr_t)__SHEAP_ALLOC(sizeof(LW_DISKCACHE_NODE) * 
                                                                   (size_t)ulNSector);
    if (pdiskcDiskCache->DISKC_pcCacheNodeMem == LW_NULL) {
        return  (PX_ERROR);
    }
    
    return  (__diskCacheMemReset(pdiskcDiskCache));
}
/*********************************************************************************************************
** 函数名称: __diskCacheHashAdd
** 功能描述: 将指定 CACHE 节点加入到相关的 HASH 表中
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           pdiskn             需要插入的 CACHE 节点
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __diskCacheHashAdd (PLW_DISKCACHE_CB   pdiskcDiskCache, PLW_DISKCACHE_NODE  pdiskn)
{
    REGISTER PLW_LIST_LINE      *pplineHashEntry;
    
    pplineHashEntry = &pdiskcDiskCache->DISKC_pplineHash[
                       pdiskn->DISKN_ulSectorNo % 
                       pdiskcDiskCache->DISKC_iHashSize];               /*  获得 HASH 表入口            */
                     
    _List_Line_Add_Ahead(&pdiskn->DISKN_lineHash, pplineHashEntry);     /*  插入 HASH 表                */
}
/*********************************************************************************************************
** 函数名称: __diskCacheHashRemove
** 功能描述: 将指定 CACHE 节点加入到相关的 HASH 表中
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           pdiskn             需要插入的 CACHE 节点
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __diskCacheHashRemove (PLW_DISKCACHE_CB   pdiskcDiskCache, PLW_DISKCACHE_NODE  pdiskn)
{
    REGISTER PLW_LIST_LINE      *pplineHashEntry;
    
    pplineHashEntry = &pdiskcDiskCache->DISKC_pplineHash[
                       pdiskn->DISKN_ulSectorNo % 
                       pdiskcDiskCache->DISKC_iHashSize];               /*  获得 HASH 表入口            */
                       
    _List_Line_Del(&pdiskn->DISKN_lineHash, pplineHashEntry);
    
    pdiskn->DISKN_ulSectorNo = (ULONG)PX_ERROR;
    pdiskn->DISKN_iStatus    = 0;                                       /*  无脏位于有效位              */
}
/*********************************************************************************************************
** 函数名称: __diskCacheHashFind
** 功能描述: 从 HASH 表中寻找指定属性的扇区
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           ulSectorNo         扇区的编号
** 输　出  : 寻找到的节点
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_DISKCACHE_NODE  __diskCacheHashFind (PLW_DISKCACHE_CB   pdiskcDiskCache, ULONG  ulSectorNo)
{
    REGISTER PLW_LIST_LINE          plineHash;
    REGISTER PLW_DISKCACHE_NODE     pdiskn;
    
    plineHash = pdiskcDiskCache->DISKC_pplineHash[
                ulSectorNo % 
                pdiskcDiskCache->DISKC_iHashSize];                      /*  获得 HASH 表入口            */
                       
    for (; plineHash != LW_NULL; plineHash = _list_line_get_next(plineHash)) {
        pdiskn = _LIST_ENTRY(plineHash, LW_DISKCACHE_NODE, DISKN_lineHash);
        if (pdiskn->DISKN_ulSectorNo == ulSectorNo) {
            return  (pdiskn);                                           /*  寻找到                      */
        }
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __diskCacheLRUFind
** 功能描述: 从 LRU 表中寻找指定属性的扇区
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           ulSectorNo         扇区的编号
** 输　出  : 寻找到的节点
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_DISKCACHE_NODE  __diskCacheLRUFind (PLW_DISKCACHE_CB   pdiskcDiskCache, ULONG  ulSectorNo)
{
             PLW_LIST_RING          pringLRUHeader = pdiskcDiskCache->DISKC_pringLRUHeader;
    REGISTER PLW_LIST_RING          pringTemp;
    REGISTER PLW_DISKCACHE_NODE     pdiskn;
    
    if (_LIST_RING_IS_EMPTY(pringLRUHeader)) {
        return  (LW_NULL);
    }
    
    pdiskn = (PLW_DISKCACHE_NODE)pringLRUHeader;
    if (pdiskn->DISKN_ulSectorNo == ulSectorNo) {
        return  (pdiskn);                                               /*  寻找到                      */
    }
    
    for (pringTemp  = _list_ring_get_next(pringLRUHeader);
         pringTemp != pringLRUHeader;
         pringTemp  = _list_ring_get_next(pringTemp)) {                 /*  遍历 LRU 表                 */
         
        pdiskn = (PLW_DISKCACHE_NODE)pringTemp;
        if (pdiskn->DISKN_ulSectorNo == ulSectorNo) {
            return  (pdiskn);                                           /*  寻找到                      */
        }
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __diskCacheSortListAdd
** 功能描述: 将指定 CACHE 节点加入到一个经过排序的链表中
** 输　入  : pringListHeader    链表表头
**           pdiskn             需要插入的 CACHE 节点
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __diskCacheSortListAdd (PLW_LIST_RING  *ppringListHeader, PLW_DISKCACHE_NODE  pdiskn)
{
    REGISTER PLW_LIST_RING          pringTemp;
             PLW_LIST_RING          pringDummyHeader;
    REGISTER PLW_DISKCACHE_NODE     pdisknTemp;

    if (_LIST_RING_IS_EMPTY(*ppringListHeader)) {
        _List_Ring_Add_Ahead(&pdiskn->DISKN_ringLRU, 
                             ppringListHeader);
        return;
    }
    
    pdisknTemp = (PLW_DISKCACHE_NODE)(*ppringListHeader);               /*  LRU 表为 CACHE 节点首个元素 */
    if (pdiskn->DISKN_ulSectorNo < pdisknTemp->DISKN_ulSectorNo) {
        _List_Ring_Add_Ahead(&pdiskn->DISKN_ringLRU, 
                             ppringListHeader);                         /*  直接插入表头                */
        return;
    }
    
    for (pringTemp  = _list_ring_get_next(*ppringListHeader);
         pringTemp != *ppringListHeader;
         pringTemp  = _list_ring_get_next(pringTemp)) {                 /*  从第二个节点线性插入        */
        
        pdisknTemp = (PLW_DISKCACHE_NODE)pringTemp;
        if (pdiskn->DISKN_ulSectorNo < pdisknTemp->DISKN_ulSectorNo) {
            pringDummyHeader = pringTemp;
            _List_Ring_Add_Last(&pdiskn->DISKN_ringLRU,
                                &pringDummyHeader);                     /*  插入到合适的位置            */
            return;
        }
    }
    
    _List_Ring_Add_Last(&pdiskn->DISKN_ringLRU,
                        ppringListHeader);                              /*  插入到链表最后              */
}
/*********************************************************************************************************
** 函数名称: __diskCacheFlushList
** 功能描述: 将指定链表内的 CACHE 节点扇区全部回写磁盘
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           pringFlushHeader   链表头
**           bMakeInvalidate    是否在回写后将相关节点设为无效状态
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheFlushList (PLW_DISKCACHE_CB   pdiskcDiskCache, 
                           PLW_LIST_RING      pringFlushHeader,
                           BOOL               bMakeInvalidate)
{
    REGISTER INT                    i;

             PLW_DISKCACHE_NODE     pdiskn;
    REGISTER PLW_DISKCACHE_NODE     pdisknContinue;
             PLW_LIST_RING          pringTemp;
             
    REGISTER INT                    iBurstCount;
             PCHAR                  pcBurstBuffer;
             INT                    iRetVal;
             BOOL                   bHasError = LW_FALSE;
    
    for (pringTemp  = pringFlushHeader;
         pringTemp != LW_NULL;
         pringTemp  = pringFlushHeader) {                               /*  直到链表为空为止            */
        
        pdiskn = (PLW_DISKCACHE_NODE)pringTemp;                         /*  DISKN_ringLRU 是第一个元素  */
        pdisknContinue = pdiskn;
        
        for (iBurstCount = 1; 
             iBurstCount < pdiskcDiskCache->DISKC_iMaxBurstSector;
             iBurstCount++) {
            
            pdisknContinue = 
                (PLW_DISKCACHE_NODE)_list_ring_get_next(&pdisknContinue->DISKN_ringLRU);
            if (pdisknContinue->DISKN_ulSectorNo != 
                (pdiskn->DISKN_ulSectorNo + iBurstCount)) {             /*  判断是否为连续扇区          */
                break;
            }
        }
        
        if (iBurstCount == 1) {                                         /*  不能进行猝发操作            */
            iRetVal = __LW_DISKCACHE_DISK_WRITE(pdiskcDiskCache)(
                                    pdiskcDiskCache->DISKC_pblkdDisk, 
                                    pdiskn->DISKN_pcData,
                                    pdiskn->DISKN_ulSectorNo,
                                    1);                                 /*  写入单扇区                  */
        } else {                                                        /*  可以使用猝发写入            */
            pcBurstBuffer  = (PCHAR)pdiskcDiskCache->DISKC_pcBurstBuffer;
            pdisknContinue = pdiskn;
            
            for (i = 0; i < iBurstCount; i++) {                         /*  装载猝发传输数据            */
                lib_memcpy(pcBurstBuffer, 
                       pdisknContinue->DISKN_pcData, 
                       (INT)pdiskcDiskCache->DISKC_ulBytesPerSector);   /*  拷贝数据                    */
                           
                pdisknContinue = 
                    (PLW_DISKCACHE_NODE)_list_ring_get_next(&pdisknContinue->DISKN_ringLRU);
                pcBurstBuffer += pdiskcDiskCache->DISKC_ulBytesPerSector;
            }
            
            iRetVal = __LW_DISKCACHE_DISK_WRITE(pdiskcDiskCache)(
                                    pdiskcDiskCache->DISKC_pblkdDisk, 
                                    pdiskcDiskCache->DISKC_pcBurstBuffer,
                                    pdiskn->DISKN_ulSectorNo,
                                    iBurstCount);                       /*  猝发写入多个扇区            */
        }
        
        if (iRetVal < 0) {
            bHasError       = LW_TRUE;
            bMakeInvalidate = LW_TRUE;                                  /*  回写失败, 需要无效设备      */
        }
        
        for (i = 0; i < iBurstCount; i++) {                             /*  开始处理这些回写的扇区记录  */
            
            __LW_DISKCACHE_CLR_DIRTY(pdiskn);
            
            if (bMakeInvalidate) {
                __diskCacheHashRemove(pdiskcDiskCache, pdiskn);         /*  从有效的 HASH 表中删除      */
            }
            
            pdisknContinue = 
                (PLW_DISKCACHE_NODE)_list_ring_get_next(&pdiskn->DISKN_ringLRU);
                
            _List_Ring_Del(&pdiskn->DISKN_ringLRU, 
                           &pringFlushHeader);                          /*  从临时回写链表中删除        */
            _List_Ring_Add_Last(&pdiskn->DISKN_ringLRU, 
                           &pdiskcDiskCache->DISKC_pringLRUHeader);     /*  加入到磁盘 CACHE LRU 表中   */
                                 
            pdiskn = pdisknContinue;                                    /*  下一个连续节点              */
        }
        
        pdiskcDiskCache->DISKC_ulDirtyCounter -= iBurstCount;           /*  重新计算需要回写的扇区数    */
    }
    
    if (bHasError) {
        return  (PX_ERROR);
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: __diskCacheFlushInvalidate
** 功能描述: 将指定磁盘 CACHE 的指定扇区范围回写并且按寻求设置为无效.
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           ulStartSector      起始扇区
**           ulEndSector        结束扇区
**           bMakeFlush         是否进行 FLUSH 操作
**           bMakeInvalidate    是否进行 INVALIDATE 操作
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheFlushInvalidate (PLW_DISKCACHE_CB   pdiskcDiskCache,
                                 ULONG              ulStartSector,
                                 ULONG              ulEndSector,
                                 BOOL               bMakeFlush,
                                 BOOL               bMakeInvalidate)
{
             INT                    i;
    REGISTER PLW_DISKCACHE_NODE     pdiskn;
    REGISTER PLW_DISKCACHE_NODE     pdisknPrev;
             PLW_LIST_RING          pringFlushHeader = LW_NULL;
                                                                        /*  从最近最久没有使用的开始    */
    pdiskn = (PLW_DISKCACHE_NODE)_list_ring_get_prev(pdiskcDiskCache->DISKC_pringLRUHeader);
    
    for (i = 0; i < pdiskcDiskCache->DISKC_ulNCacheNode; i++) {
        
        pdisknPrev = (PLW_DISKCACHE_NODE)_list_ring_get_prev(&pdiskn->DISKN_ringLRU);
        
        if ((pdiskn->DISKN_ulSectorNo > ulEndSector) ||
            (pdiskn->DISKN_ulSectorNo < ulStartSector)) {               /*  不在设定的范围内            */
            goto    __check_again;
        }
        
        if (__LW_DISKCACHE_IS_DIRTY(pdiskn)) {                          /*  检查是需要回写              */
            if (bMakeFlush) {
                _List_Ring_Del(&pdiskn->DISKN_ringLRU, 
                               &pdiskcDiskCache->DISKC_pringLRUHeader);
                __diskCacheSortListAdd(&pringFlushHeader, pdiskn);      /*  加入到排序等待回写队列      */
            }
            goto    __check_again;
        }
        
        if (__LW_DISKCACHE_IS_VALID(pdiskn)) {                          /*  检查是否有效                */
            if (bMakeInvalidate) {
                __diskCacheHashRemove(pdiskcDiskCache, pdiskn);         /*  从有效的 HASH 表中删除      */
            }
        }
        
__check_again:
        pdiskn = pdisknPrev;                                            /*  遍历 LRU 表                 */
    }
    
    if (!_LIST_RING_IS_EMPTY(pringFlushHeader)) {                       /*  是否需要回写                */
        return  (__diskCacheFlushList(pdiskcDiskCache, pringFlushHeader, bMakeInvalidate));
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: __diskCacheFlushInvalidate2
** 功能描述: 将指定磁盘 CACHE 的指定扇区范围回写并且按寻求设置为无效. (方法2)
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           iLRUSectorNum      最久未使用的扇区数量
**           bMakeFlush         是否进行 FLUSH 操作
**           bMakeInvalidate    是否进行 INVALIDATE 操作
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheFlushInvalidate2 (PLW_DISKCACHE_CB   pdiskcDiskCache,
                                  INT                iLRUSectorNum,
                                  BOOL               bMakeFlush,
                                  BOOL               bMakeInvalidate)
{
             INT                    i = 0;
             
    REGISTER PLW_DISKCACHE_NODE     pdiskn;
    REGISTER PLW_DISKCACHE_NODE     pdisknPrev;
             PLW_LIST_RING          pringFlushHeader = LW_NULL;
             
    if (iLRUSectorNum < 1) {
        return  (ERROR_NONE);                                           /*  不需要处理                  */
    }
                                                                        /*  从最近最久没有使用的开始    */
    pdiskn = (PLW_DISKCACHE_NODE)_list_ring_get_prev(pdiskcDiskCache->DISKC_pringLRUHeader);
    
    for (i = 0; 
         (i < pdiskcDiskCache->DISKC_ulNCacheNode) && (iLRUSectorNum > 0);
         i++) {
    
        pdisknPrev = (PLW_DISKCACHE_NODE)_list_ring_get_prev(&pdiskn->DISKN_ringLRU);
        
        if (__LW_DISKCACHE_IS_DIRTY(pdiskn)) {                          /*  检查是需要回写              */
            if (bMakeFlush) {
                _List_Ring_Del(&pdiskn->DISKN_ringLRU, 
                               &pdiskcDiskCache->DISKC_pringLRUHeader);
                __diskCacheSortListAdd(&pringFlushHeader, pdiskn);      /*  加入到排序等待回写队列      */
            }
            iLRUSectorNum--;
            goto    __check_again;
        }
        
        if (__LW_DISKCACHE_IS_VALID(pdiskn)) {                          /*  检查是否有效                */
            if (bMakeInvalidate) {
                __diskCacheHashRemove(pdiskcDiskCache, pdiskn);         /*  从有效的 HASH 表中删除      */
                iLRUSectorNum--;
            }
        }
    
__check_again:
        pdiskn = pdisknPrev;                                            /*  遍历 LRU 表                 */
    }
    
    if (!_LIST_RING_IS_EMPTY(pringFlushHeader)) {                       /*  是否需要回写                */
        return  (__diskCacheFlushList(pdiskcDiskCache, pringFlushHeader, bMakeInvalidate));
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: __diskCacheNodeFind
** 功能描述: 寻找一个指定的 CACHE 块
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           ulSectorNo         扇区号
** 输　出  : 寻找到的节点
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_DISKCACHE_NODE  __diskCacheNodeFind (PLW_DISKCACHE_CB   pdiskcDiskCache, ULONG  ulSectorNo)
{
    REGISTER PLW_DISKCACHE_NODE         pdiskn;
    
    pdiskn = __diskCacheHashFind(pdiskcDiskCache, ulSectorNo);          /*  首先从有效的 HASH 表中查找  */
    if (pdiskn) {
        return  (pdiskn);                                               /*  HASH 表命中                 */
    }
    
    pdiskn = __diskCacheLRUFind(pdiskcDiskCache, ulSectorNo);           /*  开始从 LRU 表中搜索         */
    
    return  (pdiskn);
}
/*********************************************************************************************************
** 函数名称: __diskCacheNodeAlloc
** 功能描述: 创建一个 CACHE 块
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           ulSectorNo         扇区号
**           iExpectation       期望预开辟的多余的扇区数量 (未用, 使用 DISKC_iMaxBurstSector)
** 输　出  : 创建到的节点
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_DISKCACHE_NODE  __diskCacheNodeAlloc (PLW_DISKCACHE_CB   pdiskcDiskCache, 
                                          ULONG              ulSectorNo,
                                          INT                iExpectation)
{
    REGISTER PLW_LIST_RING          pringTemp;
    REGISTER PLW_DISKCACHE_NODE     pdiskn;
    
__check_again:                                                          /*  从最近最久没有使用的开始    */
    for (pringTemp  = _list_ring_get_prev(pdiskcDiskCache->DISKC_pringLRUHeader);
         pringTemp != pdiskcDiskCache->DISKC_pringLRUHeader;
         pringTemp  = _list_ring_get_prev(pringTemp)) {
        
        pdiskn = (PLW_DISKCACHE_NODE)pringTemp;
        if (__LW_DISKCACHE_IS_DIRTY(pdiskn) == 0) {                     /*  不需要回写的数据块          */
            break;
        }
    }
    
    if (pringTemp == pdiskcDiskCache->DISKC_pringLRUHeader) {           /*  没有合适的控制块            */
        pdiskn = (PLW_DISKCACHE_NODE)pringTemp;
        if (__LW_DISKCACHE_IS_DIRTY(pdiskn)) {                          /*  表头也不可使用              */
            REGISTER INT   iWriteNum = pdiskcDiskCache->DISKC_iMaxBurstSector;
            __diskCacheFlushInvalidate2(pdiskcDiskCache, 
                                        iWriteNum,
                                        LW_TRUE, LW_FALSE);             /*  回写一些扇区的数据          */
            goto    __check_again;
        }
    }

    if (__LW_DISKCACHE_IS_VALID(pdiskn)) {                              /*  是否为有效扇区              */
        __diskCacheHashRemove(pdiskcDiskCache, pdiskn);                 /*  从有效表中删除              */
    }

    pdiskn->DISKN_ulSectorNo = ulSectorNo;
    __LW_DISKCACHE_SET_VALID(pdiskn);
    
    __diskCacheHashAdd(pdiskcDiskCache, pdiskn);
    
    return  (pdiskn);
}
/*********************************************************************************************************
** 函数名称: __diskCacheNodeReadData
** 功能描述: 从磁盘中读取数据填写到 CACHE 块中.
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           pdiskn             CACHE 块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheNodeReadData (PLW_DISKCACHE_CB  pdiskcDiskCache, PLW_DISKCACHE_NODE  pdiskn)
{
    REGISTER INT        i;
    REGISTER INT        iNSector;
             INT        iRetVal;
             ULONG      ulStartSector = pdiskn->DISKN_ulSectorNo;
             PCHAR      pcData;
             
    iNSector = (INT)__MIN((ULONG)pdiskcDiskCache->DISKC_iMaxBurstSector, 
                     (ULONG)((pdiskcDiskCache->DISKC_ulNCacheNode - 
                      pdiskcDiskCache->DISKC_ulDirtyCounter) / 2));     /*  获得读扇区的个数            */
                      
    iNSector = (INT)__MIN((ULONG)iNSector,                              /*  进行无符号数比较            */
                     (ULONG)(pdiskcDiskCache->DISKC_ulEndStector - 
                      pdiskn->DISKN_ulSectorNo));                       /*  不能超越最后一个扇区        */
                      
    if (iNSector <= 0) {
        goto    __read_one;                                             /*  仅读取一个扇区              */
    }

    iRetVal = __LW_DISKCACHE_DISK_READ(pdiskcDiskCache)(
                                       pdiskcDiskCache->DISKC_pblkdDisk, 
                                       pdiskcDiskCache->DISKC_pcBurstBuffer,
                                       pdiskn->DISKN_ulSectorNo,
                                       iNSector);                       /*  连续读取多个扇区的数据      */
    if (iRetVal < 0) {
__read_one:
        iRetVal = __LW_DISKCACHE_DISK_READ(pdiskcDiskCache)(
                                           pdiskcDiskCache->DISKC_pblkdDisk, 
                                           pdiskn->DISKN_pcData,
                                           pdiskn->DISKN_ulSectorNo,
                                           1);                          /*  仅读取一个扇区              */
        return  (iRetVal);
    }
    
    i      = 0;
    pcData = pdiskcDiskCache->DISKC_pcBurstBuffer;
    do {
        lib_memcpy(pdiskn->DISKN_pcData, pcData, 
                   (UINT)pdiskcDiskCache->DISKC_ulBytesPerSector);      /*  拷贝数据                    */
        
        _List_Ring_Del(&pdiskn->DISKN_ringLRU, 
                       &pdiskcDiskCache->DISKC_pringLRUHeader);         /*  重新确定 LRU 表位置         */
        _List_Ring_Add_Ahead(&pdiskn->DISKN_ringLRU, 
                       &pdiskcDiskCache->DISKC_pringLRUHeader);
    
        i++;
        pcData += pdiskcDiskCache->DISKC_ulBytesPerSector;
        
        if (i < iNSector) {                                             /*  还需要复制数据              */
            pdiskn = __diskCacheNodeFind(pdiskcDiskCache, (ulStartSector + i));
            if (pdiskn) {                                               /*  下一个扇区数据有效          */
                break;
            }
            pdiskn = __diskCacheNodeAlloc(pdiskcDiskCache, 
                                          (ulStartSector + i), 
                                          (iNSector - i));              /*  重新开辟一个空的节点        */
            if (pdiskn == LW_NULL) {
                break;
            }
        }
    } while (i < iNSector);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __diskCacheNodeGet
** 功能描述: 获取一个经过指定处理的 CACHE 节点
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           ulSectorNo         扇区号
**           iExpectation       期望预开辟的多余的扇区数量
**           bIsRead            是否为读操作
** 输　出  : 节点处理过后的节点
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_DISKCACHE_NODE  __diskCacheNodeGet (PLW_DISKCACHE_CB   pdiskcDiskCache, 
                                        ULONG              ulSectorNo,
                                        INT                iExpectation,
                                        BOOL               bIsRead)
{
    REGISTER PLW_DISKCACHE_NODE     pdiskn;
             BOOL                   bIsNewNode = LW_FALSE;
             INT                    iError;
    
    pdiskn = __diskCacheNodeFind(pdiskcDiskCache, ulSectorNo);
    if (pdiskn) {                                                       /*  直接命中                    */
        goto    __data_op;
    }
    
    pdiskn = __diskCacheNodeAlloc(pdiskcDiskCache, ulSectorNo, iExpectation);
    if (pdiskn == LW_NULL) {                                            /*  开辟新节点失败              */
        return  (LW_NULL);
    }
    bIsNewNode = LW_TRUE;
    
__data_op:
    if (bIsRead) {                                                      /*  读取数据                    */
        if (bIsNewNode) {                                               /*  需要从磁盘读取数据          */
            iError = __diskCacheNodeReadData(pdiskcDiskCache, pdiskn);
            if (iError < 0) {                                           /*  读取错误                    */
                __diskCacheHashRemove(pdiskcDiskCache, pdiskn);
                return  (LW_NULL);
            }
        }
    } else {                                                            /*  写数据                      */
        if (__LW_DISKCACHE_IS_DIRTY(pdiskn) == 0) {
            __LW_DISKCACHE_SET_DIRTY(pdiskn);                           /*  设置脏位标志                */
            pdiskcDiskCache->DISKC_ulDirtyCounter++;
        }
    }
    
    pdiskcDiskCache->DISKC_disknLuck = pdiskn;                          /*  设置幸运节点                */
    
    _List_Ring_Del(&pdiskn->DISKN_ringLRU, 
                   &pdiskcDiskCache->DISKC_pringLRUHeader);             /*  重新确定 LRU 表位置         */
    _List_Ring_Add_Ahead(&pdiskn->DISKN_ringLRU, 
                   &pdiskcDiskCache->DISKC_pringLRUHeader);
                   
    return  (pdiskn);
}
/*********************************************************************************************************
** 函数名称: __diskCacheRead
** 功能描述: 通过磁盘 CACHE 读取磁盘的指定扇区
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           pvBuffer           缓冲区
**           ulStartSector      起始扇区
**           ulSectorCount      连续扇区数量
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheRead (PLW_DISKCACHE_CB   pdiskcDiskCache,
                      VOID              *pvBuffer, 
                      ULONG              ulStartSector, 
                      ULONG              ulSectorCount)
{
             INT                    i;
             INT                    iError = ERROR_NONE;
    REGISTER PLW_DISKCACHE_NODE     pdiskn;
             PCHAR                  pcData = (PCHAR)pvBuffer;

    __LW_DISKCACHE_LOCK(pdiskcDiskCache);                               /*  互斥访问                    */
    if (pdiskcDiskCache->DISKC_iCacheOpt & LW_DISKCACHE_OPT_ENABLE) {
        for (i = 0; i < ulSectorCount; i++) {
            
            pdiskn = __diskCacheNodeGet(pdiskcDiskCache, 
                                        (ulStartSector + i), 
                                        (INT)(ulSectorCount - i),
                                        LW_TRUE);
            if (pdiskn == LW_NULL) {
                iError =  PX_ERROR;
                break;
            }
            
            lib_memcpy(pcData, pdiskn->DISKN_pcData,
                       (UINT)pdiskcDiskCache->DISKC_ulBytesPerSector);  /*  拷贝数据                    */
                       
            pcData += pdiskcDiskCache->DISKC_ulBytesPerSector;
        }
    } else {
        iError = __LW_DISKCACHE_DISK_READ(pdiskcDiskCache)(
                                          pdiskcDiskCache->DISKC_pblkdDisk, 
                                          pvBuffer,
                                          ulStartSector,
                                          ulSectorCount);               /*  连续读取多个扇区的数据      */
    }
    __LW_DISKCACHE_UNLOCK(pdiskcDiskCache);                             /*  解锁                        */

    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __diskCacheWrite
** 功能描述: 通过磁盘 CACHE 写入磁盘的指定扇区
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
**           pvBuffer           缓冲区
**           ulStartSector      起始扇区
**           ulSectorCount      连续扇区数量
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheWrite (PLW_DISKCACHE_CB   pdiskcDiskCache,
                       VOID              *pvBuffer, 
                       ULONG              ulStartSector, 
                       ULONG              ulSectorCount)
{
             INT                    i;
             INT                    iError = ERROR_NONE;
    REGISTER PLW_DISKCACHE_NODE     pdiskn;
             PCHAR                  pcData = (PCHAR)pvBuffer;
             
    __LW_DISKCACHE_LOCK(pdiskcDiskCache);                               /*  互斥访问                    */
    if (pdiskcDiskCache->DISKC_iCacheOpt & LW_DISKCACHE_OPT_ENABLE) {
        for (i = 0; i < ulSectorCount; i++) {
            
            pdiskn = __diskCacheNodeGet(pdiskcDiskCache, 
                                        (ulStartSector + i), 
                                        (INT)(ulSectorCount - i),
                                        LW_FALSE);
                                        
            if (pdiskn == LW_NULL) {
                iError =  PX_ERROR;
                break;
            }
            
            if (__LW_DISKCACHE_IS_DIRTY(pdiskn) == 0) {
                __LW_DISKCACHE_SET_DIRTY(pdiskn);                       /*  设置脏位标志                */
                pdiskcDiskCache->DISKC_ulDirtyCounter++;
            }
            
            lib_memcpy(pdiskn->DISKN_pcData, pcData, 
                       (UINT)pdiskcDiskCache->DISKC_ulBytesPerSector);  /*  写入数据                    */
                       
            pcData += pdiskcDiskCache->DISKC_ulBytesPerSector;
        }
    } else {
        iError = __LW_DISKCACHE_DISK_WRITE(pdiskcDiskCache)(
                                           pdiskcDiskCache->DISKC_pblkdDisk, 
                                           pvBuffer,
                                           ulStartSector,
                                           ulSectorCount);              /*  写入多个扇区                */
    }
    __LW_DISKCACHE_UNLOCK(pdiskcDiskCache);                             /*  解锁                        */

    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __diskCacheIoctl
** 功能描述: 通过磁盘 CACHE 控制一个磁盘
** 输　入  : pdiskcDiskCache   磁盘 CACHE 控制块
**           iCmd              控制命令
**           lArg              控制参数
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheIoctl (PLW_DISKCACHE_CB   pdiskcDiskCache, INT  iCmd, LONG  lArg)
{
    REGISTER INT        iError = PX_ERROR;

    __LW_DISKCACHE_LOCK(pdiskcDiskCache);                               /*  互斥访问                    */

    switch (iCmd) {
    
    case LW_BLKD_DISKCACHE_GET_OPT:
        lArg = (LONG)pdiskcDiskCache->DISKC_iCacheOpt;
        __LW_DISKCACHE_UNLOCK(pdiskcDiskCache);                         /*  解锁                        */
        _ErrorHandle(ERROR_NONE);
        return  (ERROR_NONE);
        
    case LW_BLKD_DISKCACHE_SET_OPT:
        if ((pdiskcDiskCache->DISKC_iCacheOpt & LW_DISKCACHE_OPT_ENABLE) &&
            (!(lArg & LW_DISKCACHE_OPT_ENABLE))) {                      /*  需要回写磁盘                */
            __diskCacheFlushInvalidate(pdiskcDiskCache,
                                       0, (ULONG)PX_ERROR,
                                       LW_TRUE, LW_TRUE);               /*  全部回写并使无效            */
        }
        pdiskcDiskCache->DISKC_iCacheOpt = (INT)lArg;
        __LW_DISKCACHE_UNLOCK(pdiskcDiskCache);                         /*  解锁                        */
        _ErrorHandle(ERROR_NONE);
        return  (ERROR_NONE);
        
    case LW_BLKD_DISKCACHE_INVALID:                                     /*  全部不命中                  */
        iError = ERROR_NONE;
        __diskCacheFlushInvalidate(pdiskcDiskCache,
                                   0, (ULONG)PX_ERROR,
                                   LW_TRUE, LW_TRUE);
        break;
        
    case LW_BLKD_DISKCACHE_RAMFLUSH:                                    /*  随机回写                    */
        iError = ERROR_NONE;
        __diskCacheFlushInvalidate2(pdiskcDiskCache, 
                                    (INT)lArg,
                                    LW_TRUE, LW_FALSE);
        break;
        
    case FIOSYNC:                                                       /*  同步磁盘                    */
    case FIODATASYNC:
    case FIOFLUSH:                                                      /*  全部回写                    */
        iError = ERROR_NONE;
        __diskCacheFlushInvalidate(pdiskcDiskCache,
                                   0, (ULONG)PX_ERROR,
                                   LW_TRUE, LW_FALSE);
        break;
    
    case FIODISKCHANGE:                                                 /*  磁盘发生改变                */
        pdiskcDiskCache->DISKC_blkdCache.BLKD_bDiskChange = LW_TRUE;
    case FIOCANCEL:                                                     /*  停止 CACHE, 复位内存,不回写 */
    case FIOUNMOUNT:                                                    /*  卸载卷                      */
        iError = ERROR_NONE;
        __diskCacheMemReset(pdiskcDiskCache);                           /*  重新初始化 CACHE 内存       */
        _ErrorHandle(ERROR_NONE);
        break;
    }
    
    __LW_DISKCACHE_UNLOCK(pdiskcDiskCache);                             /*  解锁                        */
    
    if (iError == ERROR_NONE) {
        __LW_DISKCACHE_DISK_IOCTL(pdiskcDiskCache)(pdiskcDiskCache->DISKC_pblkdDisk, 
                                                   iCmd,
                                                   lArg);
    } else {
        iError = __LW_DISKCACHE_DISK_IOCTL(pdiskcDiskCache)(pdiskcDiskCache->DISKC_pblkdDisk, 
                                                            iCmd,
                                                            lArg);
    }
    
    if (iCmd == FIODISKINIT) {                                          /*  需要确定磁盘的最后一个扇区  */
        ULONG           ulNDiskSector = (ULONG)PX_ERROR;
        PLW_BLK_DEV     pblkdDisk     = pdiskcDiskCache->DISKC_pblkdDisk;
        
        if (pblkdDisk->BLKD_ulNSector) {
            ulNDiskSector = pblkdDisk->BLKD_ulNSector;
        } else {
            pblkdDisk->BLKD_pfuncBlkIoctl(pblkdDisk, 
                                          LW_BLKD_GET_SECNUM, 
                                          (LONG)&ulNDiskSector);
        }
        pdiskcDiskCache->DISKC_ulEndStector = ulNDiskSector - 1;        /*  获得最后一个扇区的编号      */
    }
    
    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __diskCacheReset
** 功能描述: 通过磁盘 CACHE 复位一个磁盘(初始化)
** 输　入  : pdiskcDiskCache   磁盘 CACHE 控制块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheReset (PLW_DISKCACHE_CB   pdiskcDiskCache)
{
    __diskCacheIoctl(pdiskcDiskCache, FIOFLUSH, 0);                     /*  CACHE 回写磁盘              */

    return  (__LW_DISKCACHE_DISK_RESET(pdiskcDiskCache)(pdiskcDiskCache->DISKC_pblkdDisk));
}
/*********************************************************************************************************
** 函数名称: __diskCacheStatusChk
** 功能描述: 通过磁盘 CACHE 检查一个磁盘
** 输　入  : pdiskcDiskCache   磁盘 CACHE 控制块
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __diskCacheStatusChk (PLW_DISKCACHE_CB   pdiskcDiskCache)
{
    return  (__LW_DISKCACHE_DISK_STATUS(pdiskcDiskCache)(pdiskcDiskCache->DISKC_pblkdDisk));
}
#endif                                                                  /*  (LW_CFG_MAX_VOLUMES > 0)    */
                                                                        /*  (LW_CFG_DISKCACHE_EN > 0)   */
/*********************************************************************************************************
  END
*********************************************************************************************************/
