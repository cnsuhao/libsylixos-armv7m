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
** 文   件   名: coresdLib.h
**
** 创   建   人: Zeng.Bo (曾波)
**
** 文件创建日期: 2010 年 11 月 23 日
**
** 描        述: sd卡特殊操作接口头文件

** BUG:
2010.11.27 增加了几个API.
2010.03.30 增加__sdCoreDevMmcSetRelativeAddr(),以支持MMC卡.
*********************************************************************************************************/

#ifndef __CORESD_LIB_H
#define __CORESD_LIB_H

INT __sdCoreDecodeCID(LW_SDDEV_CID  *psdcidDec, UINT32 *pRawCID, UINT8 ucType);
INT __sdCoreDecodeCSD(LW_SDDEV_CSD  *psdcsdDec, UINT32 *pRawCSD, UINT8 ucType);
INT __sdCoreDevReset(PLW_SDCORE_DEVICE psdcoredevice);
INT __sdCoreDevSendIfCond(PLW_SDCORE_DEVICE psdcoredevice);
INT __sdCoreDevSendAppOpCond(PLW_SDCORE_DEVICE  psdcoredevice,
                             UINT32             uiOCR,
                             LW_SDDEV_OCR      *puiOutOCR,
                             UINT8             *pucType);
INT __sdCoreDevSendRelativeAddr(PLW_SDCORE_DEVICE psdcoredevice, UINT32 *puiRCA);
INT __sdCoreDevSendAllCID(PLW_SDCORE_DEVICE psdcoredevice, LW_SDDEV_CID *psdcid);
INT __sdCoreDevSendAllCSD(PLW_SDCORE_DEVICE psdcoredevice, LW_SDDEV_CSD *psdcsd);
INT __sdCoreDevSelect(PLW_SDCORE_DEVICE psdcoredevice);
INT __sdCoreDevDeSelect(PLW_SDCORE_DEVICE psdcoredevice);
INT __sdCoreDevSetBusWidth(PLW_SDCORE_DEVICE psdcoredevice, INT iBusWidth);
INT __sdCoreDevSetBlkLen(PLW_SDCORE_DEVICE psdcoredevice, INT iBlkLen);
INT __sdCoreDevGetStatus(PLW_SDCORE_DEVICE psdcoredevice, UINT32 *puiStatus);
INT __sdCoreDevSetPreBlkLen(PLW_SDCORE_DEVICE psdcoredevice, INT iPreBlkLen);
INT __sdCoreDevIsBlockAddr(PLW_SDCORE_DEVICE psdcoredevice, BOOL *pbResult);

INT __sdCoreDevSpiClkDely(PLW_SDCORE_DEVICE psdcoredevice, INT iClkConts);
INT __sdCoreDevSpiCrcEn(PLW_SDCORE_DEVICE psdcoredevice, BOOL bEnable);

INT __sdCoreDevMmcSetRelativeAddr(PLW_SDCORE_DEVICE psdcoredevice, UINT32 uiRCA);

#endif                                                                  /*  __CORESD_LIB_H              */
/*********************************************************************************************************
  END
*********************************************************************************************************/
