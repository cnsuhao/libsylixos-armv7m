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
** 文   件   名: diskio.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 09 月 26 日
**
** 描        述: FAT 文件系统与 BLOCK 设备接口
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "diskio.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if LW_CFG_MAX_VOLUMES > 0
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
INT  __blockIoDevRead(INT     iIndex, 
                      VOID   *pvBuffer, 
                      ULONG   ulStartSector, 
                      ULONG   ulSectorCount);
INT  __blockIoDevWrite(INT     iIndex, 
                       VOID   *pvBuffer, 
                       ULONG   ulStartSector, 
                       ULONG   ulSectorCount);
INT  __blockIoDevReset(INT     iIndex);
INT  __blockIoDevStatus(INT     iIndex);
INT  __blockIoDevIoctl(INT  iIndex, INT  iCmd, LONG  lArg);
/*********************************************************************************************************
** 函数名称: disk_initialize
** 功能描述: 初始化块设备
** 输　入  : ucDriver      卷序号
** 输　出  : VOID
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
DSTATUS  disk_initialize (BYTE  ucDriver)
{
    REGISTER INT    iError = __blockIoDevIoctl((INT)ucDriver, FIODISKINIT, 0);
    
    if (iError < 0) {
        return  ((BYTE)(STA_NOINIT | STA_NODISK));
    } else {
        return  ((BYTE)ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: disk_status
** 功能描述: 获得块设备状态
** 输　入  : ucDriver      卷序号
** 输　出  : ERROR_NONE 表示块设备当前正常, 其他表示不正常.
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
DSTATUS  disk_status (BYTE  ucDriver)
{
    return  ((BYTE)__blockIoDevStatus((INT)ucDriver)); 
}
/*********************************************************************************************************
** 函数名称: disk_status
** 功能描述: 读一个块设备
** 输　入  : ucDriver          卷序号
**           ucBuffer          缓冲区
**           dwSectorNumber    起始扇区号
**           ucSectorCount     扇区数量
** 输　出  : DRESULT
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
DRESULT disk_read (BYTE  ucDriver, BYTE  *ucBuffer, DWORD   dwSectorNumber, BYTE  ucSectorCount)
{
    REGISTER INT    iError;
    
    iError = __blockIoDevRead((INT)ucDriver, 
                              (PVOID)ucBuffer, 
                              (ULONG)dwSectorNumber,
                              (ULONG)ucSectorCount);
    if (iError >= ERROR_NONE) {
        return  (RES_OK);
    } else {
        return  (RES_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: disk_write
** 功能描述: 写一个块设备
** 输　入  : ucDriver          卷序号
**           ucBuffer          缓冲区
**           dwSectorNumber    起始扇区号
**           ucSectorCount     扇区数量
** 输　出  : DRESULT
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
DRESULT disk_write (BYTE  ucDriver, const BYTE  *ucBuffer, DWORD   dwSectorNumber, BYTE  ucSectorCount)
{
    REGISTER INT    iError;
    
    iError = __blockIoDevWrite((INT)ucDriver, 
                               (PVOID)ucBuffer, 
                               (ULONG)dwSectorNumber,
                               (ULONG)ucSectorCount);
    if (iError >= ERROR_NONE) {
        return  (RES_OK);
    } else {
        return  (RES_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: disk_write
** 功能描述: 写一个块设备
** 输　入  : ucDriver          卷序号
**           ucCmd             命令
**           pvArg             参数
** 输　出  : DRESULT
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
DRESULT  disk_ioctl (BYTE  ucDriver, BYTE ucCmd, void  *pvArg)
{
    REGISTER INT    iError;
    
    switch (ucCmd) {                                                    /*  转换命令                    */
    
    case CTRL_SYNC:
        return  (RES_OK);                                               /*  注意, 目前此条命令忽略      */
    
    case GET_SECTOR_COUNT:
        ucCmd = LW_BLKD_GET_SECNUM;
        break;
        
    case GET_SECTOR_SIZE:
        ucCmd = LW_BLKD_GET_SECSIZE;
        break;
        
    case GET_BLOCK_SIZE:
        ucCmd = LW_BLKD_GET_BLKSIZE;
        break;
        
    case CTRL_POWER:
        ucCmd = LW_BLKD_CTRL_POWER;
        break;
        
    case CTRL_LOCK:
        ucCmd = LW_BLKD_CTRL_LOCK;
        break;
        
    case CTRL_EJECT:
        ucCmd = LW_BLKD_CTRL_EJECT;
        break;
    }
    
    iError = __blockIoDevIoctl((INT)ucDriver, (INT)ucCmd, (LONG)pvArg);
    
    if (iError >= ERROR_NONE) {
        return  (RES_OK);
    } else {
        return  (RES_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: disk_timerproc
** 功能描述: I don't know!!!
** 输　入  : ucDriver          卷序号
**           ucCmd             命令
**           pvArg             参数
** 输　出  : DRESULT
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  disk_timerproc (void)
{
}
#endif                                                                  /*  LW_CFG_MAX_VOLUMES          */
/*********************************************************************************************************
  END
*********************************************************************************************************/

