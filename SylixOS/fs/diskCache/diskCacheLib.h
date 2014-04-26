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
** 文件创建日期: 2008 年 10 月 10 日
**
** 描        述: 磁盘高速缓冲管理库. 
*********************************************************************************************************/

#ifndef __DISKCACHELIB_H
#define __DISKCACHELIB_H

/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_MAX_VOLUMES > 0) && (LW_CFG_DISKCACHE_EN > 0)
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
extern LW_OBJECT_HANDLE     _G_ulDiskCacheListLock;                     /*  链表锁                      */
extern PLW_LIST_LINE        _G_plineDiskCacheHeader;                    /*  链表头                      */
/*********************************************************************************************************
  DISK CACHE NODE LOCK
*********************************************************************************************************/
#define __LW_DISKCACHE_LOCK(pdiskc) do {                                                    \
            API_SemaphoreMPend(pdiskc->DISKC_hDiskCacheLock, LW_OPTION_WAIT_INFINITE);      \
        } while (0)
#define __LW_DISKCACHE_UNLOCK(pdiskc)   do {                                                \
            API_SemaphoreMPost(pdiskc->DISKC_hDiskCacheLock);                               \
        } while (0)
/*********************************************************************************************************
  DISK CACHE NODE OP
*********************************************************************************************************/
#define __LW_DISKCACHE_NODE_READ                0                       /*  节点读                      */
#define __LW_DISKCACHE_NODE_WRITE               1                       /*  节点写                      */
/*********************************************************************************************************
  DISK CACHE NODE MACRO
*********************************************************************************************************/
#define __LW_DISKCACHE_IS_VALID(pdiskn)         ((pdiskn)->DISKN_iStatus & 0x01)
#define __LW_DISKCACHE_IS_DIRTY(pdiskn)         ((pdiskn)->DISKN_iStatus & 0x02)

#define __LW_DISKCACHE_SET_VALID(pdiskn)        ((pdiskn)->DISKN_iStatus |= 0x01)
#define __LW_DISKCACHE_SET_DIRTY(pdiskn)        ((pdiskn)->DISKN_iStatus |= 0x02)

#define __LW_DISKCACHE_CLR_VALID(pdiskn)        ((pdiskn)->DISKN_iStatus &= ~0x01)
#define __LW_DISKCACHE_CLR_DIRTY(pdiskn)        ((pdiskn)->DISKN_iStatus &= ~0x02)
/*********************************************************************************************************
  DISK CACHE NODE
*********************************************************************************************************/
typedef struct {
    LW_LIST_RING            DISKN_ringLRU;                              /*  LRU 表节点                  */
    LW_LIST_LINE            DISKN_lineHash;                             /*  HASH 表节点                 */
    
    ULONG                   DISKN_ulSectorNo;                           /*  缓冲的扇区号                */
    INT                     DISKN_iStatus;                              /*  节点状态                    */
    caddr_t                 DISKN_pcData;                               /*  扇区数据缓冲                */
} LW_DISKCACHE_NODE;
typedef LW_DISKCACHE_NODE  *PLW_DISKCACHE_NODE;
/*********************************************************************************************************
  DISK CACHE NODE
*********************************************************************************************************/
typedef struct {
    LW_BLK_DEV              DISKC_blkdCache;                            /*  DISK CACHE 的 BLK IO 控制块 */
    PLW_BLK_DEV             DISKC_pblkdDisk;                            /*  被缓冲 BLK IO 控制块地址    */
    LW_LIST_LINE            DISKC_lineManage;                           /*  背景线程管理链表            */
    
    LW_OBJECT_HANDLE        DISKC_hDiskCacheLock;                       /*  DISK CACHE 操作锁           */
    INT                     DISKC_iCacheOpt;                            /*  CACHE 工作选项              */
    
    ULONG                   DISKC_ulEndStector;                         /*  最后一个扇区的编号          */
    ULONG                   DISKC_ulDirtyCounter;                       /*  需要回写的扇区数量          */
    ULONG                   DISKC_ulBytesPerSector;                     /*  每一扇区字节数量            */
    
    INT                     DISKC_iMaxBurstSector;                      /*  最大猝发读写扇区数量        */
    caddr_t                 DISKC_pcBurstBuffer;                        /*  猝发读写缓冲区              */
    
    PLW_LIST_RING           DISKC_pringLRUHeader;                       /*  LRU 表头                    */
    PLW_LIST_LINE          *DISKC_pplineHash;                           /*  HASH 表池                   */
    INT                     DISKC_iHashSize;                            /*  HASH 表大小                 */
    
    ULONG                   DISKC_ulNCacheNode;                         /*  CACHE 缓冲的节点数          */
    caddr_t                 DISKC_pcCacheNodeMem;                       /*  CACHE 节点链表首地址        */
    caddr_t                 DISKC_pcCacheMem;                           /*  CACHE 缓冲区                */
    PLW_DISKCACHE_NODE      DISKC_disknLuck;                            /*  幸运扇区节点                */
} LW_DISKCACHE_CB;
typedef LW_DISKCACHE_CB    *PLW_DISKCACHE_CB;
/*********************************************************************************************************
  内部函数
*********************************************************************************************************/
PVOID   __diskCacheThread(PVOID  pvArg);
VOID    __diskCacheListAdd(PLW_DISKCACHE_CB   pdiskcDiskCache);
VOID    __diskCacheListDel(PLW_DISKCACHE_CB   pdiskcDiskCache);
#endif                                                                  /*  LW_CFG_MAX_VOLUMES > 0      */
                                                                        /*  LW_CFG_DISKCACHE_EN > 0     */
#endif                                                                  /*  __DISKCACHELIB_H            */
/*********************************************************************************************************
  END
*********************************************************************************************************/
