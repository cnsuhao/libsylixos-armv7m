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
** 文   件   名: diskCacheThread.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 03 月 23 日
**
** 描        述: 磁盘高速缓冲控制器背景回写任务.
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
  经验数据
*********************************************************************************************************/
#define __LW_DISKCACHE_BG_SECONDS       2                               /*  回写周期                    */
#define __LW_DISKCACHE_BG_MINSECTOR     64                              /*  每次最少回写的扇区数        */
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
LW_OBJECT_HANDLE        _G_ulDiskCacheListLock  = 0ul;                  /*  链表锁                      */
PLW_LIST_LINE           _G_plineDiskCacheHeader = LW_NULL;              /*  链表头                      */
static PLW_LIST_LINE    _G_plineDiskCacheOp     = LW_NULL;              /*  操作链表节点                */
/*********************************************************************************************************
  锁操作
*********************************************************************************************************/
#define __LW_DISKCACHE_LIST_LOCK()      \
        API_SemaphoreMPend(_G_ulDiskCacheListLock, LW_OPTION_WAIT_INFINITE)
#define __LW_DISKCACHE_LIST_UNLOCK()    \
        API_SemaphoreMPost(_G_ulDiskCacheListLock)
/*********************************************************************************************************
** 函数名称: __diskCacheThread
** 功能描述: 磁盘高速缓冲控制器背景回写任务
** 输　入  : pvArg             启动参数
** 输　出  : NULL
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PVOID  __diskCacheThread (PVOID  pvArg)
{
    PLW_DISKCACHE_CB   pdiskcDiskCache;
    ULONG              ulNSector;
    
    (VOID)pvArg;
    
    for (;;) {
        API_TimeSSleep(__LW_DISKCACHE_BG_SECONDS);                      /*  近似延时                    */
        
        __LW_DISKCACHE_LIST_LOCK();
        _G_plineDiskCacheOp = _G_plineDiskCacheHeader;
        while (_G_plineDiskCacheOp) {
            pdiskcDiskCache = _LIST_ENTRY(_G_plineDiskCacheOp, 
                                          LW_DISKCACHE_CB, 
                                          DISKC_lineManage);
            _G_plineDiskCacheOp = _list_line_get_next(_G_plineDiskCacheOp);
            
            __LW_DISKCACHE_LIST_UNLOCK();                               /*  释放链表操作使用权          */
            
            ulNSector = __MAX(__LW_DISKCACHE_BG_MINSECTOR, 
                              (pdiskcDiskCache->DISKC_ulDirtyCounter / 2));
                                                                        /*  计算回写磁盘的扇区数        */
            pdiskcDiskCache->DISKC_blkdCache.BLKD_pfuncBlkIoctl(pdiskcDiskCache, 
                LW_BLKD_DISKCACHE_RAMFLUSH, (LONG)ulNSector);           /*  回写磁盘                    */
            
            __LW_DISKCACHE_LIST_LOCK();                                 /*  获取链表操作使用权          */
        }
        __LW_DISKCACHE_LIST_UNLOCK();
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __diskCacheListAdd
** 功能描述: 将 DISK CACHE 块加入回写链
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __diskCacheListAdd (PLW_DISKCACHE_CB   pdiskcDiskCache)
{
    __LW_DISKCACHE_LIST_LOCK();
    _List_Line_Add_Ahead(&pdiskcDiskCache->DISKC_lineManage,
                         &_G_plineDiskCacheHeader);
    __LW_DISKCACHE_LIST_UNLOCK();
}
/*********************************************************************************************************
** 函数名称: __diskCacheListDel
** 功能描述: 从回写链中将 DISK CACHE 移除
** 输　入  : pdiskcDiskCache    磁盘 CACHE 控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __diskCacheListDel (PLW_DISKCACHE_CB   pdiskcDiskCache)
{
    __LW_DISKCACHE_LIST_LOCK();
    if (&pdiskcDiskCache->DISKC_lineManage == _G_plineDiskCacheOp) {
        _G_plineDiskCacheOp = _list_line_get_next(_G_plineDiskCacheOp);
    }
    _List_Line_Del(&pdiskcDiskCache->DISKC_lineManage,
                   &_G_plineDiskCacheHeader);
    __LW_DISKCACHE_LIST_UNLOCK();
}
#endif                                                                  /*  (LW_CFG_MAX_VOLUMES > 0)    */
                                                                        /*  (LW_CFG_DISKCACHE_EN > 0)   */
/*********************************************************************************************************
  END
*********************************************************************************************************/
