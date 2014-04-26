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
** 文   件   名: sdDev.h
**
** 创   建   人: Zeng.Bo (曾波)
**
** 文件创建日期: 2010 年 11 月 22 日
**
** 描        述: sd 总线挂载的设备结构.

** BUG:
2011.01.18  增加SD设备获取函数.
*********************************************************************************************************/

#ifndef __SDDEV_H
#define __SDDEV_H

#include "sdBus.h"

/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if LW_CFG_DEVICE_EN > 0

/*********************************************************************************************************
  SD设备 OCR寄存器及其位定义
*********************************************************************************************************/

typedef UINT32              LW_SDDEV_OCR;
typedef LW_SDDEV_OCR       *PLW_SDDEV_OCR;

#define SD_VDD_165_195      0x00000080                                  /*  VDD voltage 1.65 - 1.95     */
#define SD_VDD_20_21        0x00000100                                  /*  VDD voltage 2.0 ~ 2.1       */
#define SD_VDD_21_22        0x00000200                                  /*  VDD voltage 2.1 ~ 2.2       */
#define SD_VDD_22_23        0x00000400                                  /*  VDD voltage 2.2 ~ 2.3       */
#define SD_VDD_23_24        0x00000800                                  /*  VDD voltage 2.3 ~ 2.4       */
#define SD_VDD_24_25        0x00001000                                  /*  VDD voltage 2.4 ~ 2.5       */
#define SD_VDD_25_26        0x00002000                                  /*  VDD voltage 2.5 ~ 2.6       */
#define SD_VDD_26_27        0x00004000                                  /*  VDD voltage 2.6 ~ 2.7       */
                                                                        /*  以上在记忆卡中未定义        */
#define SD_VDD_27_28        0x00008000                                  /*  VDD voltage 2.7 ~ 2.8       */
#define SD_VDD_28_29        0x00010000                                  /*  VDD voltage 2.8 ~ 2.9       */
#define SD_VDD_29_30        0x00020000                                  /*  VDD voltage 2.9 ~ 3.0       */
#define SD_VDD_30_31        0x00040000                                  /*  VDD voltage 3.0 ~ 3.1       */
#define SD_VDD_31_32        0x00080000                                  /*  VDD voltage 3.1 ~ 3.2       */
#define SD_VDD_32_33        0x00100000                                  /*  VDD voltage 3.2 ~ 3.3       */
#define SD_VDD_33_34        0x00200000                                  /*  VDD voltage 3.3 ~ 3.4       */
#define SD_VDD_34_35        0x00400000                                  /*  VDD voltage 3.4 ~ 3.5       */
#define SD_VDD_35_36        0x00800000                                  /*  VDD voltage 3.5 ~ 3.6       */
#define SD_OCR_BUSY         0x80000000                                  /*  busy bit                    */
#define SD_OCR_HCS          0x40000000
#define SD_OCR_S18A         0x01000000
#define SD_OCR_VDD_MSK      0x00ffff80
#define SD_OCR_MEM_VDD_MSK  0x00ff8000                                  /*  2.7vdd ~ 3.6vdd mask        */

/*********************************************************************************************************
       CID寄存器
*********************************************************************************************************/

typedef struct lw_sddev_cid {
    UINT8       DEVCID_ucMainFid;
    UINT16      DEVCID_usOemId;
    INT8        DEVCID_pucProductName[5];
    INT8        DEVCID_ucProductVsn;
    UINT32      DEVCID_uiSerialNum;

    UINT32      DEVCID_uiYear;
    UINT8       DEVCID_ucMonth;
} LW_SDDEV_CID, *PLW_SDDEV_CID;

/*********************************************************************************************************
       CSD寄存器
*********************************************************************************************************/

typedef struct lw_sddev_csd {
    UINT8       DEVCSD_ucStructure;
#define CSD_STRUCT_VER_1_0  0                                           /*  CSD Ver 1.0 - 1.2           */
#define CSD_STRUCT_VER_1_1  1                                           /*  CSD Ver 1.4 - 2.2           */
#define CSD_STRUCT_VER_2_0  1                                           /*  CSD Ver 2.0                 */
#define CSD_STRUCT_VER_1_2  2                                           /*  CSD Ver 3.1~3.31 4.0~4.1    */
#define CSD_STRUCT_EXT_CSD  3                                           /*  CSD Ver in EXT_CSD          */

    UINT32      DEVCSD_uiTaccNs;
    UINT16      DEVCSD_usTaccClks;

    UINT32      DEVCSD_uiTranSpeed;
    UINT8       DEVCSD_ucR2W_Factor;

    UINT8       DEVCSD_ucVddMin;
    UINT8       DEVCSD_ucVddMax;

    UINT8       DEVCSD_ucReadBlkLenBits;
    UINT8       DEVCSD_ucWriteBlkLenBits;

    BOOL        DEVCSD_bEraseEnable;
    UINT8       DEVCSD_ucEraseBlkLen;
    UINT8       DEVCSD_ucSectorSize;

    BOOL        DEVCSD_bReadMissAlign;
    BOOL        DEVCSD_bWriteMissAlign;
    BOOL        DEVCSD_bReadBlkPartial;
    BOOL        DEVCSD_bWriteBlkPartial;

    UINT16      DEVCSD_usCmdclass;
#define CCC_BASIC           (1 << 0)                                    /*  (0) Basic functions         */
#define CCC_STREAM_READ     (1 << 1)                                    /*  (1) Stream read commands    */
#define CCC_BLOCK_READ      (1 << 2)                                    /*  (2) Block read commands     */
#define CCC_STREAM_WRITE    (1 << 3)                                    /*  (3) Stream write commands   */
#define CCC_BLOCK_WRITE     (1 << 4)                                    /*  (4) Block write commands    */
#define CCC_ERASE           (1 << 5)                                    /*  (5) Ability to erase blocks */
#define CCC_WRITE_PROT      (1 << 6)                                    /*  (6) able write protect blk  */
#define CCC_LOCK_DEV        (1 << 7)                                    /*  (7) able lock down card     */
#define CCC_APP_SPEC        (1 << 8)                                    /*  (8) Application specific    */
#define CCC_IO_MODE         (1 << 9)                                    /*  (9) I/O mode                */
#define CCC_SWITCH          (1 << 10)                                   /*  (10) High speed switch      */

    UINT32      DEVCSD_uiCapacity;                                      /*  卡的容量,这里指示的是卡的块 */
                                                                        /*  个数.                       */
} LW_SDDEV_CSD, *PLW_SDDEV_CSD;

/*********************************************************************************************************
  SCR寄存器
*********************************************************************************************************/

typedef struct lw_sddev_scr {
    UINT8       DEVSCR_ucSdaVsn;
    UINT8       DEVSCR_ucBusWidth;
#define SD_SCR_BUS_WIDTH_1  (1 << 0)
#define SD_SCR_BUS_WIDTH_4  (1 << 2)

} LW_SDDEV_SCR, *PLW_SDDEV_SCR;

/*********************************************************************************************************
  SD 设备类型
*********************************************************************************************************/

typedef struct lw_sd_device {
    PLW_SD_ADAPTER      SDDEV_psdAdapter;                               /*  从属的SD适配器              */
    LW_LIST_LINE        SDDEV_lineManage;                               /*  设备挂载链                  */
    atomic_t            SDDEV_atomicUsageCnt;                           /*  设备使用计数                */

    UINT8               SDDEV_ucType;
#define SDDEV_TYPE_MMC      0
#define SDDEV_TYPE_SDSC     1
#define SDDEV_TYPE_SDHC     2
#define SDDEV_TYPE_SDXC     3
#define SDDEV_TYPE_SDIO     4
#define SDDEV_TYPE_COMM     5
#define SDDEV_TYPE_MAXVAL   5                                           /*  用于参数判断                */

    UINT32              SDDEV_uiRCA;                                    /*  设备本地地址                */
    UINT32              SDDEV_uiState;                                  /*  设备状态位标                */
#define SD_STATE_EXIST      (1 << 0)
#define SD_STATE_WRTP       (1 << 1)
#define SD_STATE_BAD        (1 << 2)
#define SD_STATE_READONLY   (1 << 3)

    LW_SDDEV_CID        SDDEV_cid;
    LW_SDDEV_CSD        SDDEV_csd;
    LW_SDDEV_SCR        SDDEV_scr;
    CHAR                SDDEV_pDevName[LW_CFG_OBJECT_NAME_SIZE];
    PVOID               SDDEV_pvUsr;                                    /*  设备用户数据                */

} LW_SD_DEVICE, *PLW_SD_DEVICE;

/*********************************************************************************************************
  驱动程序可以使用的 API
  以下 API 只能在驱动程序中使用, 应用程序仅仅可以看到已经过 io 系统的 sd 设备.
*********************************************************************************************************/

LW_API INT              API_SdLibInit(VOID);

/*********************************************************************************************************
  SD 适配器基本操作
*********************************************************************************************************/

LW_API INT              API_SdAdapterCreate(CPCHAR pcName, PLW_SD_FUNCS psdfunc);
LW_API INT              API_SdAdapterDelete(CPCHAR pcName);
LW_API PLW_SD_ADAPTER   API_SdAdapterGet(CPCHAR pcName);

/*********************************************************************************************************
  SD 设备基本操作
*********************************************************************************************************/

LW_API PLW_SD_DEVICE    API_SdDeviceCreate(CPCHAR pcAdapterName, CPCHAR pcDeviceName);
LW_API INT              API_SdDeviceDelete(PLW_SD_DEVICE    psddevice);
LW_API PLW_SD_DEVICE    API_SdDeviceGet(CPCHAR pcAdapterName, CPCHAR pcDeviceName);

LW_API INT              API_SdDeviceUsageInc(PLW_SD_DEVICE    psddevice);
LW_API INT              API_SdDeviceUsageDec(PLW_SD_DEVICE    psddevice);
LW_API INT              API_SdDeviceUsageGet(PLW_SD_DEVICE    psddevice);

/*********************************************************************************************************
  SD 设备传输控制操作
*********************************************************************************************************/

LW_API INT              API_SdDeviceTransfer(PLW_SD_DEVICE   psddevice,
                                             PLW_SD_MESSAGE  psdmsg,
                                             INT             iNum);
LW_API INT              API_SdDeviceCtl(PLW_SD_DEVICE  psddevice,
                                        INT            iCmd,
                                        LONG           lArg);

#endif                                                                  /*  LW_CFG_DEVICE_EN            */
#endif                                                                  /*  __SDDEV_H                   */
/*********************************************************************************************************
  END
*********************************************************************************************************/
