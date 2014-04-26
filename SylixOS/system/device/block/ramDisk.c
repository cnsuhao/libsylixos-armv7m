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
** 文   件   名: ramDisk.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 09 月 26 日
**
** 描        述: RAM DISK 驱动程序.

** BUG:
2009.11.03  初始化时 BLKD_bDiskChange 为 LW_FALSE.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if LW_CFG_MAX_VOLUMES > 0
/*********************************************************************************************************
  内部类型定义
*********************************************************************************************************/
typedef struct {
    LW_BLK_DEV      RAMD_blkdRam;                                       /*  块设备                      */
    PVOID           RAMD_pvMem;                                         /*  内存基地址                  */
} LW_RAM_DISK;
typedef LW_RAM_DISK *PLW_RAM_DISK;
/*********************************************************************************************************
  内部函数
*********************************************************************************************************/
static INT   __ramDiskReset(PLW_RAM_DISK  pramd);
static INT   __ramDiskStatusChk(PLW_RAM_DISK  pramd);
static INT   __ramDiskIoctl(PLW_RAM_DISK  pramd, INT  iCmd, LONG  lArg);
static INT   __ramDiskWrt(PLW_RAM_DISK  pramd, 
                          VOID         *pvBuffer, 
                          ULONG         ulStartSector, 
                          ULONG         ulSectorCount);
static INT   __ramDiskRd(PLW_RAM_DISK  pramd, 
                         VOID         *pvBuffer, 
                         ULONG         ulStartSector, 
                         ULONG         ulSectorCount);
/*********************************************************************************************************
** 函数名称: API_RamDiskCreate
** 功能描述: 创建一个内存盘.
** 输　入  : pvDiskAddr        磁盘内存起始点
**           ullDiskSize       磁盘大小
**           ppblkdRam         内存盘驱动控制块地址
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_RamDiskCreate (PVOID  pvDiskAddr, UINT64  ullDiskSize, PLW_BLK_DEV  *ppblkdRam)
{
    REGISTER PLW_RAM_DISK       pramd;
    REGISTER PLW_BLK_DEV        pblkd;
    
    if (ullDiskSize < LW_CFG_MB_SIZE) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    if (!pvDiskAddr || !ppblkdRam) {
        _ErrorHandle(EINVAL);
        return  (EINVAL);
    }
    
    pramd = (PLW_RAM_DISK)__SHEAP_ALLOC(sizeof(LW_RAM_DISK));
    if (!pramd) {
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (ERROR_SYSTEM_LOW_MEMORY);
    }
    lib_bzero(pramd, sizeof(LW_RAM_DISK));
    
    pblkd = &pramd->RAMD_blkdRam;
    
    pblkd->BLKD_pfuncBlkRd        = __ramDiskRd;
    pblkd->BLKD_pfuncBlkWrt       = __ramDiskWrt;
    pblkd->BLKD_pfuncBlkIoctl     = __ramDiskIoctl;
    pblkd->BLKD_pfuncBlkReset     = __ramDiskReset;
    pblkd->BLKD_pfuncBlkStatusChk = __ramDiskStatusChk;
    pblkd->BLKD_ulNSector         = (ULONG)(ullDiskSize / 512ul);
    pblkd->BLKD_ulBytesPerSector  = 512ul;
    pblkd->BLKD_ulBytesPerBlock   = 512ul;
    pblkd->BLKD_bRemovable        = LW_FALSE;
    pblkd->BLKD_bDiskChange       = LW_FALSE;
    pblkd->BLKD_iRetry            = 1;
    pblkd->BLKD_iFlag             = O_RDWR;
    
    pblkd->BLKD_iLogic            = 0;                                  /*  物理设备                    */
    pblkd->BLKD_uiLinkCounter     = 0;
    pblkd->BLKD_pvLink            = LW_NULL;
    
    pblkd->BLKD_uiPowerCounter    = 0;
    pblkd->BLKD_uiInitCounter     = 0;
    
    pramd->RAMD_pvMem = pvDiskAddr;

    *ppblkdRam = &pramd->RAMD_blkdRam;                                  /*  保存控制块                  */
    
#if LW_CFG_ERRORMESSAGE_EN > 0 || LW_CFG_LOGMESSAGE_EN > 0
    {
        CHAR    cString[20];
        _DebugHandle(__LOGMESSAGE_LEVEL, "ram disk");
        _DebugHandle(__LOGMESSAGE_LEVEL, " size : ");
        sprintf(cString, "0x%x", (INT)ullDiskSize);
        _DebugHandle(__LOGMESSAGE_LEVEL, cString);
        sprintf(cString, "0x%x", (INT)pvDiskAddr);
        _DebugHandle(__LOGMESSAGE_LEVEL, " base : ");
        _DebugHandle(__LOGMESSAGE_LEVEL, cString);
        _DebugHandle(__LOGMESSAGE_LEVEL, " has been create.\r\n");
    }
#endif                                                                  /*  LW_CFG_ERRORMESSAGE_EN > 0  */
                                                                        /*  LW_CFG_LOGMESSAGE_EN > 0    */
    _ErrorHandle(ERROR_NONE);
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_RamDiskDelete
** 功能描述: 删除一个内存盘. 
** 输　入  : pblkdRam          内存盘驱动控制块
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
** 注  意  : 删除内存盘之前首先要使用 remove 函数卸载卷, 
             例如:
                    BLK_DEV   *pblkdRam;
                    
                    ramDiskCreate(..., &pblkdRam);
                    fatFsDevCreate("/ram0", pblkdRam);
                    ...
                    unlink("/ram0");
                    ramDiskDelete(pblkdRam);
                    
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_RamDiskDelete (PLW_BLK_DEV  pblkdRam)
{
    if (pblkdRam) {
        __SHEAP_FREE(pblkdRam);
        _ErrorHandle(ERROR_NONE);
        return  (ERROR_NONE);
    } else {
        _ErrorHandle(ERROR_IOS_DEVICE_NOT_FOUND);
        return  (PX_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: __ramDiskReset
** 功能描述: 复位一个内存盘.
** 输　入  : pramd             内存盘控制块
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __ramDiskReset (PLW_RAM_DISK  pramd)
{
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __ramDiskStatusChk
** 功能描述: 检测一个内存盘.
** 输　入  : pramd             内存盘控制块
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __ramDiskStatusChk (PLW_RAM_DISK  pramd)
{
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __ramDiskIoctl
** 功能描述: 控制一个内存盘.
** 输　入  : pramd             内存盘控制块
**           iCmd              控制命令
**           lArg              控制参数
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __ramDiskIoctl (PLW_RAM_DISK  pramd, INT  iCmd, LONG  lArg)
{
    switch (iCmd) {
    
    /*
     *  必须要支持的 4 个命令
     */
    case FIOSYNC:
    case FIOFLUSH:                                                      /*  将缓存写入磁盘              */
        break;
    
    case FIOUNMOUNT:                                                    /*  卸载卷                      */
        break;
        
    case FIODISKINIT:                                                   /*  初始化磁盘                  */
        break;
    
    /*
     *  低级格式化
     */    
    case FIODISKFORMAT:                                                 /*  格式化卷                    */
        return  (PX_ERROR);                                             /*  不支持低级格式化            */
    
    /*
     *  FatFs 扩展命令
     */
    case LW_BLKD_CTRL_POWER:
    case LW_BLKD_CTRL_LOCK:
    case LW_BLKD_CTRL_EJECT:
        break;
    
    default:
        _ErrorHandle(ERROR_IO_UNKNOWN_REQUEST);
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __ramDiskWrt
** 功能描述: 写一个内存盘.
** 输　入  : pramd             内存盘控制块
**           pvBuffer          缓冲区
**           ulStartSector     起始扇区号
**           ulSectorCount     扇区数量
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __ramDiskWrt (PLW_RAM_DISK  pramd, 
                          VOID         *pvBuffer, 
                          ULONG         ulStartSector, 
                          ULONG         ulSectorCount)
{
    REGISTER PBYTE      pucStartMem = ((PBYTE)pramd->RAMD_pvMem
                                    + (ulStartSector * 512));
                                    
    lib_memcpy(pucStartMem, pvBuffer, (INT)(512 * ulSectorCount));
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __ramDiskRd
** 功能描述: 读一个内存盘.
** 输　入  : pramd             内存盘控制块
**           pvBuffer          缓冲区
**           ulStartSector     起始扇区号
**           ulSectorCount     扇区数量
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __ramDiskRd (PLW_RAM_DISK  pramd, 
                         VOID         *pvBuffer, 
                         ULONG         ulStartSector, 
                         ULONG         ulSectorCount)
{
    REGISTER PBYTE      pucStartMem = ((PBYTE)pramd->RAMD_pvMem
                                    + (ulStartSector * 512));
                                    
    lib_memcpy(pvBuffer, pucStartMem, (INT)(512 * ulSectorCount));
    
    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_MAX_VOLUMES          */
/*********************************************************************************************************
  END
*********************************************************************************************************/
