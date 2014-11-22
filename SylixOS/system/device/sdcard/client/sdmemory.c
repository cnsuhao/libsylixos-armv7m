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
** 文   件   名: sdmemory.c
**
** 创   建   人: Zeng.Bo (曾波)
**
** 文件创建日期: 2010 年 11 月 25 日
**
** 描        述: sd 记忆卡客户应用驱动源文件

** BUG:
2010.12.08  优化了 __sdmemTestBusy() 函数.
2010.12.08  SD 设备结构中加入块寻址标记,以支持 SDHC 卡.
2011.01.12  增加对 SPI 的支持.
2011.03.25  修改 __sdMemIoctl() 函数, 增加设备状态检测.
2011.03.25  修改 API_SdMemDevCreate(), 用于底层驱动安装上层的回调.
2011.04.03  将 API_SdMemDevShowInfo() 改为 API_SdMemDevShow() 统一 SylxiOS Show 函数.
2011.04.03  更改 block io 层回调函数的参数.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_SDCARD_EN > 0)
#include "sdmemory.h"
#include "../core/sdcore.h"
#include "../core/sdcoreLib.h"
#include "../core/sdstd.h"
/*********************************************************************************************************
  sd 块设备内部结构
*********************************************************************************************************/
typedef struct __sd_blk_dev {
    LW_BLK_DEV            SDBLKDEV_blkDev;
    PLW_SDCORE_DEVICE     SDBLKDEV_pcoreDev;
    BOOL                  SDBLKDEV_bIsBlockAddr;                        /*  是否是块寻址                */

    /*
     * 增加SDM后, 为了保持API不变，增加以下成员
     */
    BOOL                  SDBLKDEV_bCoreDevSelf;                       /*  coredev 是自己创建(非SDM给)  */
} __SD_BLK_DEV, *__PSD_BLK_DEV;
/*********************************************************************************************************
  内部宏
*********************************************************************************************************/
#define __SDMEM_BLKADDR(pdev)       (pdev->SDBLKDEV_bIsBlockAddr)
#define __SD_CID_PNAME(iN)          (sddevcid.DEVCID_pucProductName[iN])

#define __SD_DEV_RETRY              4
#define __SD_MILLION                1000000
/*********************************************************************************************************
  忙检测函数的等待类型
*********************************************************************************************************/
#define __SD_BUSY_TYPE_READ         0
#define __SD_BUSY_TYPE_RDYDATA      1
#define __SD_BUSY_TYPE_PROG         2
#define __SD_BUSY_TYPE_ERASE        3
#define __SD_BUSY_RETRY             0x3fffffff
#define __SD_TIMEOUT_SEC            2                                   /*  2秒超时为一个经验值         */

#define __SD_CARD_STATUS_MSK        (0x0f << 9)
#define __SD_CARD_STATUS_PRG        (0x07 << 9)                         /*  数据正在编程                */
#define __SD_CARD_STATUS_RDYDATA    (0x01 << 8)
/*********************************************************************************************************
  私有函数声明
*********************************************************************************************************/
static INT __sdMemTestBusy(PLW_SDCORE_DEVICE psdcoredevice, INT iType);
static INT __sdMemInit(PLW_SDCORE_DEVICE psdcoredevice);
static INT __sdMemWrtSingleBlk(PLW_SDCORE_DEVICE  psdcoredevice,
                               UINT8             *pucBuf,
                               UINT32             uiStartBlk);
static INT __sdMemWrtMultiBlk(PLW_SDCORE_DEVICE  psdcoredevice,
                              UINT8             *pucBuf,
                              UINT32             uiStartBlk,
                              UINT32             uiNBlks);
static INT __sdMemRdSingleBlk(PLW_SDCORE_DEVICE  psdcoredevice,
                              UINT8             *pucBuf,
                              UINT32             uiStartBlk);
static INT __sdMemRdMultiBlk(PLW_SDCORE_DEVICE  psdcoredevice,
                             UINT8             *pucBuf,
                             UINT32             uiStartBlk,
                             UINT32             uiNBlks);

static INT __sdMemBlkWrt(__PSD_BLK_DEV   psdblkdevice,
                        VOID            *pvWrtBuffer,
                        ULONG            ulStartBlk,
                        ULONG            ulBlkCount);
static INT __sdMemBlkRd(__PSD_BLK_DEV   psdblkdevice,
                        VOID           *pvRdBuffer,
                        ULONG           ulStartBlk,
                        ULONG           ulBlkCount);
static INT __sdMemIoctl(__PSD_BLK_DEV    psdblkdevice,
                        INT              iCmd,
                        LONG             lArg);
static INT __sdMemStatus(__PSD_BLK_DEV   psdblkdevice);
static INT __sdMemReset(__PSD_BLK_DEV    psdblkdevice);
/*********************************************************************************************************
** 函数名称: API_SdMemDevCreate
** 功能描述: 创建一个SD记忆卡设备
** 输    入: iAdapterType     设备挂接的适配器类型(spi 或 sd)
**           pcAdapterName    挂接的适配器名称
**           pcDeviceName     设备名称
**           psdmemchan       通道
** 输    出: NONE
** 返    回: 成功,返回设备块设备指针,否则返回LW_NULL
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API PLW_BLK_DEV API_SdMemDevCreate (INT                       iAdapterType,
                                       CPCHAR                    pcAdapterName,
                                       CPCHAR                    pcDeviceName,
                                       PLW_SDMEM_CHAN            psdmemchan)
{
    PLW_SDCORE_DEVICE   psdcoredevice   = LW_NULL;
    __PSD_BLK_DEV       psdblkdevice    = LW_NULL;
    PLW_BLK_DEV         pblkdevice      = LW_NULL;
    PLW_SDCORE_CHAN     psdcorechan     = LW_NULL;
    BOOL                bCoreDevSelf    = TRUE;

    LW_SDDEV_CSD        sddevcsd;
    BOOL                bBlkAddr;
    INT                 iError;

    /*
     * 增加了SDM后, 约定：当适配器名称和设备名称为空时, 表示coredev 由 sdm创建
     * 此时, psdmemchan 指向对应的coredev
     */
    if (!pcAdapterName && !pcDeviceName) {
        psdcoredevice = (PLW_SDCORE_DEVICE)psdmemchan;
        bCoreDevSelf = FALSE;

    } else {
        psdcorechan   = (PLW_SDCORE_CHAN)psdmemchan;
        psdcoredevice = API_SdCoreDevCreate(iAdapterType,
                                            pcAdapterName,
                                            pcDeviceName,
                                            psdcorechan);
    }

    if (!psdcoredevice) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "create coredevice failed.\r\n");
        return  (LW_NULL);
    }

    /*
     * 初始化这个设备(针对记忆卡)
     */
    iError = __sdMemInit(psdcoredevice);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "do memory initialize failed.\r\n");

        if (bCoreDevSelf) {
            API_SdCoreDevDelete(psdcoredevice);
        }

        return  (LW_NULL);
    }

    psdblkdevice  = (__PSD_BLK_DEV)__SHEAP_ALLOC(sizeof(__SD_BLK_DEV));
    if (!psdblkdevice) {
        if (bCoreDevSelf) {
            API_SdCoreDevDelete(psdcoredevice);
        }

        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (LW_NULL);

    }

    iError = API_SdCoreDevCsdView(psdcoredevice, &sddevcsd);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "view csd of device failed.\r\n");
        __SHEAP_FREE(psdblkdevice);

        if (bCoreDevSelf) {
            API_SdCoreDevDelete(psdcoredevice);
        }

        return  (LW_NULL);
    }

    iError = __sdCoreDevIsBlockAddr(psdcoredevice, &bBlkAddr);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "unkonwn address access way.\r\n");
        __SHEAP_FREE(psdblkdevice);

        if (bCoreDevSelf) {
            API_SdCoreDevDelete(psdcoredevice);
        }

        return  (LW_NULL);
    }

    psdblkdevice->SDBLKDEV_bIsBlockAddr  = bBlkAddr;                    /*  设置寻址方式                */
    psdblkdevice->SDBLKDEV_pcoreDev      = psdcoredevice;               /*  连接核心设备                */
    psdblkdevice->SDBLKDEV_bCoreDevSelf = bCoreDevSelf;

    pblkdevice = &psdblkdevice->SDBLKDEV_blkDev;

    pblkdevice->BLKD_pfuncBlkRd        = __sdMemBlkRd;
    pblkdevice->BLKD_pfuncBlkWrt       = __sdMemBlkWrt;
    pblkdevice->BLKD_pfuncBlkIoctl     = __sdMemIoctl;
    pblkdevice->BLKD_pfuncBlkReset     = __sdMemReset;
    pblkdevice->BLKD_pfuncBlkStatusChk = __sdMemStatus;

    pblkdevice->BLKD_ulNSector         = sddevcsd.DEVCSD_uiCapacity;

    pblkdevice->BLKD_ulBytesPerSector  = SD_MEM_DEFAULT_BLKSIZE;
    pblkdevice->BLKD_ulBytesPerBlock   = SD_MEM_DEFAULT_BLKSIZE;

    pblkdevice->BLKD_bRemovable        = LW_TRUE;
    pblkdevice->BLKD_bDiskChange       = LW_FALSE;                      /*  媒质没有改变                */
    pblkdevice->BLKD_iRetry            = __SD_DEV_RETRY;                /*  重试次数                    */
    pblkdevice->BLKD_iFlag             = O_RDWR;                        /*  读写属性                    */
    pblkdevice->BLKD_iLogic            = 0;
    pblkdevice->BLKD_uiLinkCounter     = 0;
    pblkdevice->BLKD_pvLink            = LW_NULL;
    pblkdevice->BLKD_uiPowerCounter    = 0;
    pblkdevice->BLKD_uiInitCounter     = 0;

    return  (pblkdevice);
}
/*********************************************************************************************************
** 函数名称: API_SdMemDevDelete
** 功能描述: 删除一个SD记忆卡设备
** 输    入: pblkdevice 块设备结构指针
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API INT  API_SdMemDevDelete (PLW_BLK_DEV pblkdevice)
{
    PLW_SDCORE_DEVICE   psdcoredevice   = LW_NULL;
    __PSD_BLK_DEV       psdblkdevice    = LW_NULL;
    INT                 iError;

    if (!pblkdevice) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdblkdevice  = (__PSD_BLK_DEV)pblkdevice;
    psdcoredevice = psdblkdevice->SDBLKDEV_pcoreDev;

    if (psdcoredevice && psdblkdevice->SDBLKDEV_bCoreDevSelf) {
        iError = API_SdCoreDevDelete(psdcoredevice);                    /*  先删除core设备              */
        if (iError != ERROR_NONE) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "delet coredevice failed.\r\n");
            _ErrorHandle(EINVAL);
            return  (PX_ERROR);
        }
    }

    __SHEAP_FREE(psdblkdevice);                                         /*  再释放sd blk                */

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdMemDevShow
** 功能描述: 打印SD设备信息
** 输    入: PLW_BLK_DEV 块设备结构指针
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API INT  API_SdMemDevShow (PLW_BLK_DEV pblkdevice)
{
    PLW_SDCORE_DEVICE   psdcoredevice   = LW_NULL;
    __PSD_BLK_DEV       psdblkdevice    = LW_NULL;
    LW_SDDEV_CSD        sddevcsd;
    LW_SDDEV_CID        sddevcid;

    UINT32              uiCapMod;
    UINT64              ullCap;
    UINT8               ucType;

    if (!pblkdevice) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdblkdevice   = (__PSD_BLK_DEV)pblkdevice;
    psdcoredevice = psdblkdevice->SDBLKDEV_pcoreDev;

    if (!psdcoredevice) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "no core device.\r\n");
        return  (PX_ERROR);
    }

    API_SdCoreDevCsdView(psdcoredevice, &sddevcsd);
    API_SdCoreDevCidView(psdcoredevice, &sddevcid);
    API_SdCoreDevTypeView(psdcoredevice, &ucType);

    ullCap   = (UINT64)sddevcsd.DEVCSD_uiCapacity * ((UINT64)1 << sddevcsd.DEVCSD_ucReadBlkLenBits);
    uiCapMod = ullCap % LW_CFG_MB_SIZE;

    printf("\nSD MEMORY INFO >>\n");
    printf("Manufacturer :  0x%02x\n", sddevcid.DEVCID_ucMainFid);
    if (ucType == SDDEV_TYPE_MMC) {
        printf("OEM ID       :  %08x\n", sddevcid.DEVCID_usOemId);
    } else {
        printf("OEM ID       :  %c%c\n", sddevcid.DEVCID_usOemId >> 8,
                                         sddevcid.DEVCID_usOemId & 0xff);
    }
    printf("Product Name :  %c%c%c%c%c\n",
                                   __SD_CID_PNAME(0),
                                   __SD_CID_PNAME(1),
                                   __SD_CID_PNAME(2),
                                   __SD_CID_PNAME(3),
                                   __SD_CID_PNAME(4));
    printf("Product Vsn  :  v%d.%d\n", sddevcid.DEVCID_ucProductVsn >> 4,
                                       sddevcid.DEVCID_ucProductVsn & 0xf);
    printf("Serial Num   :  %x\n", sddevcid.DEVCID_uiSerialNum);
    printf("Time         :  %d/%02d\n", sddevcid.DEVCID_uiYear, sddevcid.DEVCID_ucMonth);
    printf("Max Speed    :  %d.%06d MB/s\n", sddevcsd.DEVCSD_uiTranSpeed / __SD_MILLION,
                                           sddevcsd.DEVCSD_uiTranSpeed % __SD_MILLION);
    printf("Capacity     :  %u.%03u MB\n", (UINT32)(ullCap / LW_CFG_MB_SIZE), uiCapMod / 1000);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdMemTestBusy
** 功能描述: 忙检测函数.在写时使用
** 输    入: psdcoredevice  核心设备
**           iType          检测类型
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemTestBusy (PLW_SDCORE_DEVICE psdcoredevice, INT iType)
{
    INT     iError;
    UINT    uiSta;
    INT     iRetry = 0;

    struct timespec   tvOld;
    struct timespec   tvNow;

    lib_clock_gettime(CLOCK_MONOTONIC, &tvOld);

    while (iRetry++ < __SD_BUSY_RETRY) {
        iError = __sdCoreDevGetStatus(psdcoredevice, &uiSta);

        if (iError != ERROR_NONE) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "get device status failed.\r\n");
            break;
        }

        if ((uiSta & __SD_CARD_STATUS_MSK) != __SD_CARD_STATUS_PRG) {
            return   (ERROR_NONE);
        }

        lib_clock_gettime(CLOCK_MONOTONIC, &tvNow);
        if ((tvNow.tv_sec - tvOld.tv_sec) >= __SD_TIMEOUT_SEC) {        /*  超时退出                    */
            _DebugHandle(__ERRORMESSAGE_LEVEL, "timeout.\r\n");
            break;
        }
    }

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __sdMemInit
** 功能描述: SD记忆卡初始化
** 输    入: psdcoredevice  核心设备
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemInit (PLW_SDCORE_DEVICE psdcoredevice)
{
    INT             iError;
    CHAR           *pcError;
    UINT8           ucType;
    UINT32          uiOCR;
    UINT32          uiRCA;
    LW_SDDEV_OCR    sddevocr;
    LW_SDDEV_CID    sddevcid;
    LW_SDDEV_CSD    sddevcsd;

    switch (psdcoredevice->COREDEV_iAdapterType) {

    case SDADAPTER_TYPE_SD:

        API_SdCoreDevCtl(psdcoredevice, SDBUS_CTRL_POWERON, 0);

        iError = API_SdCoreDevCtl(psdcoredevice,
                                  SDBUS_CTRL_SETCLK,
                                  SDARG_SETCLK_LOW);                    /*  初始化时 低速时钟           */

        if (iError != ERROR_NONE) {
            pcError = "set clock to normal failed.\r\n";
            goto __error_handle;
        }

        SD_DELAYMS(10) ;

        iError = __sdCoreDevReset(psdcoredevice);                       /*  cmd0 go idle                */
        if (iError != ERROR_NONE) {
            pcError = "device reset failed.\r\n";
            goto __error_handle;
        }

        iError = API_SdCoreDevCtl(psdcoredevice,
                                  SDBUS_CTRL_GETOCR,
                                  (LONG)&uiOCR);

        iError = __sdCoreDevSendIfCond(psdcoredevice);                  /*  cmd8 (v2.0以上的卡必须这个) */
                                                                        /*  v2.0以下的卡无应答,忽略错误 */

        if(iError == ERROR_NONE) {                                      /*  SDHC卡初始化支持            */
            uiOCR |= SD_OCR_HCS ;
        }

        iError = __sdCoreDevSendAppOpCond(psdcoredevice,
                                          uiOCR,
                                          &sddevocr,
                                          &ucType);                     /*  acmd41                      */
        if (iError != ERROR_NONE) {
            pcError = "device don't support the ocr.\r\n";
            goto __error_handle;
        }

        API_SdCoreDevTypeSet(psdcoredevice, ucType);                    /*  设置type域                  */

        iError = __sdCoreDevSendAllCID(psdcoredevice, &sddevcid);       /*  cmd2                        */
        if (iError != ERROR_NONE) {
            pcError = "get device cid failed.\r\n";
            goto __error_handle;
        }
        if (ucType == SDDEV_TYPE_MMC) {
            uiRCA = 0x01;
            iError = __sdCoreDevMmcSetRelativeAddr(psdcoredevice, uiRCA);
        } else {
            iError = __sdCoreDevSendRelativeAddr(psdcoredevice, &uiRCA);/*  cmd3                        */

        }if (iError != ERROR_NONE) {
            pcError = "get device rca failed.\r\n";
            goto __error_handle;
        }

        API_SdCoreDevRcaSet(psdcoredevice, uiRCA);                      /*  设置RCA域                   */

        iError = __sdCoreDevSendAllCSD(psdcoredevice, &sddevcsd);
        if (iError != ERROR_NONE) {
            pcError = "get device csd failed.\r\n";
            goto __error_handle;
        }

        sddevcsd.DEVCSD_uiCapacity       <<= sddevcsd.DEVCSD_ucReadBlkLenBits - \
                                             SD_MEM_DEFAULT_BLKSIZE_NBITS;
        sddevcsd.DEVCSD_ucReadBlkLenBits   = SD_MEM_DEFAULT_BLKSIZE_NBITS;
        sddevcsd.DEVCSD_ucWriteBlkLenBits  = SD_MEM_DEFAULT_BLKSIZE_NBITS;

        API_SdCoreDevCsdSet(psdcoredevice, &sddevcsd);                  /*  设置CSD域                   */
        API_SdCoreDevCidSet(psdcoredevice, &sddevcid);                  /*  设置CID域                   */

        iError = API_SdCoreDevCtl(psdcoredevice,
                                  SDBUS_CTRL_SETCLK,
                                  SDARG_SETCLK_NORMAL);                 /*  时钟设置到全速              */
        /*
         * TODO: 根据卡类型设置为高速
         */
        if (iError != ERROR_NONE) {
            pcError = "set clock to max failed.\r\n";
            goto __error_handle;
        }

        /*
         * 以下进入传输模式
         */
        iError = __sdCoreDevSetBlkLen(psdcoredevice, SD_MEM_DEFAULT_BLKSIZE);
        if (iError != ERROR_NONE) {
            pcError = "set blklen failed.\r\n";
            goto __error_handle;
        }

        if (ucType != SDDEV_TYPE_MMC) {

            iError = __sdCoreDevSetBusWidth(psdcoredevice, SDARG_SETBUSWIDTH_4);
                                                                        /*  acmd6 set bus width         */
            if (iError != ERROR_NONE) {
                pcError = "set bus width error.\r\n";
                goto __error_handle;
            }
            API_SdCoreDevCtl(psdcoredevice, SDBUS_CTRL_SETBUSWIDTH, SDARG_SETBUSWIDTH_4);
        } else {                                                        /*  mmc 只有一位总线            */
            API_SdCoreDevCtl(psdcoredevice, SDBUS_CTRL_SETBUSWIDTH, SDARG_SETBUSWIDTH_1);
        }
        return  (ERROR_NONE);

    case SDADAPTER_TYPE_SPI:

        API_SdCoreDevCtl(psdcoredevice, SDBUS_CTRL_POWERON, 0);

        SD_DELAYMS(3);

        iError = API_SdCoreDevCtl(psdcoredevice,
                                  SDBUS_CTRL_SETCLK,
                                  SDARG_SETCLK_LOW);                    /*  初始化时 低速时钟           */

        if (iError != ERROR_NONE) {
            pcError = "set clock to normal failed.\r\n";
            goto __error_handle;
        }

        __sdCoreDevSpiClkDely(psdcoredevice, 100);                      /*  延时大于74个时钟            */

        iError = __sdCoreDevReset(psdcoredevice);                       /*  cmd0 go idle                */
        if (iError != ERROR_NONE) {
            pcError = "device reset failed.\r\n";
            goto __error_handle;
        }

        iError = API_SdCoreDevCtl(psdcoredevice,
                                  SDBUS_CTRL_GETOCR,
                                  (LONG)&uiOCR);

        iError = API_SdCoreSpiSendIfCond(psdcoredevice);                /*  cmd8 (v2.0以上的卡必须这个) */
                                                                        /*  v2.0以下的卡无应答,忽略错误 */

        uiOCR |= SD_OCR_HCS ;                                           /*  SDHC卡初始化支持            */

        iError = __sdCoreDevSendAppOpCond(psdcoredevice,
                                          uiOCR,
                                          &sddevocr,
                                          &ucType);                     /*  acmd41   +  cmd58(spi)      */
        if (iError != ERROR_NONE) {
            pcError = "device don't support the ocr.\r\n";
            goto __error_handle;
        }

        API_SdCoreDevTypeSet(psdcoredevice, ucType);                    /*  设置type域                  */

#if LW_CFG_SDCARD_CRC_EN > 0
        __sdCoreDevSpiCrcEn(psdcoredevice, LW_TRUE);                    /*  使能crc                     */
#endif

        iError = __sdCoreDevSetBlkLen(psdcoredevice, SD_MEM_DEFAULT_BLKSIZE);
        if (iError != ERROR_NONE) {
            pcError = "set blklen failed.\r\n";
            goto __error_handle;
        }

        iError = __sdCoreDevSendAllCID(psdcoredevice, &sddevcid);       /*  cmd2                        */
        if (iError != ERROR_NONE) {
            pcError = "get device cid failed.\r\n";
            goto __error_handle;
        }

        iError = __sdCoreDevSendAllCSD(psdcoredevice, &sddevcsd);
        if (iError != ERROR_NONE) {
            pcError = "get device csd failed.\r\n";
            goto __error_handle;
        }

        sddevcsd.DEVCSD_uiCapacity       <<= sddevcsd.DEVCSD_ucReadBlkLenBits - \
                                             SD_MEM_DEFAULT_BLKSIZE_NBITS;
        sddevcsd.DEVCSD_ucReadBlkLenBits   = SD_MEM_DEFAULT_BLKSIZE_NBITS;
        sddevcsd.DEVCSD_ucWriteBlkLenBits  = SD_MEM_DEFAULT_BLKSIZE_NBITS;

        API_SdCoreDevCsdSet(psdcoredevice, &sddevcsd);                  /*  设置CSD域                   */
        API_SdCoreDevCidSet(psdcoredevice, &sddevcid);                  /*  设置CID域                   */

        iError = API_SdCoreDevCtl(psdcoredevice,
                                  SDBUS_CTRL_SETCLK,
                                  SDARG_SETCLK_NORMAL);                 /*  设置为全速时钟              */
        if (iError != ERROR_NONE) {
            pcError = "set to high clock mode failed.\r\n";
        }
        return  (ERROR_NONE);

    default:
        pcError = "unknown adapter type.\r\n";
        break;
    }

__error_handle:
    _DebugHandle(__ERRORMESSAGE_LEVEL, pcError);

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __sdMemWrtSingleBlk
** 功能描述: SD记忆卡设备写单块
** 输    入: psdcoredevice 核心设备结构
**           pucBuf        写缓冲
**           uiStartBlk    起始地址
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemWrtSingleBlk (PLW_SDCORE_DEVICE  psdcoredevice,
                                UINT8             *pucBuf,
                                UINT32             uiStartBlk)
{
    LW_SD_MESSAGE   sdmsg;
    LW_SD_COMMAND   sdcmd;
    LW_SD_DATA      sddat;
    INT             iError;
    INT             iDevSta;

    iDevSta = API_SdCoreDevStaView(psdcoredevice);
    if (iDevSta != SD_DEVSTA_EXIST) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "device is not exist.\r\n");
        return  (PX_ERROR);
    }

    lib_bzero(&sdcmd, sizeof(sdcmd));
    lib_bzero(&sddat, sizeof(sddat));

    sdcmd.SDCMD_uiOpcode   = SD_WRITE_BLOCK;
    sdcmd.SDCMD_uiArg      = uiStartBlk;
    sdcmd.SDCMD_uiFlag     = SD_RSP_SPI_R1 | SD_RSP_R1 | SD_CMD_ADTC;   /*  命令                        */

    sddat.SDDAT_uiBlkNum   = 1;
    sddat.SDDAT_uiBlkSize  = SD_MEM_DEFAULT_BLKSIZE;
    sddat.SDDAT_uiFlags    = SD_DAT_WRITE;                              /*  数据                        */

    sdmsg.SDMSG_pucWrtBuffer = pucBuf;
    sdmsg.SDMSG_psdData      = &sddat;
    sdmsg.SDMSG_psdcmdCmd    = &sdcmd;
    sdmsg.SDMSG_psdcmdStop   = LW_NULL;
    sdmsg.SDMSG_pucRdBuffer  = LW_NULL;                                 /*  读缓冲为空                  */

    iError = API_SdCoreDevTransfer(psdcoredevice, &sdmsg, 1);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "request error.\r\n");
        return  (PX_ERROR);
    }

    iError = __sdMemTestBusy(psdcoredevice, __SD_BUSY_TYPE_PROG);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "check busy error.\r\n");
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdMemWrtMultiBlk
** 功能描述: SD记忆卡设备写多块
** 输    入: psdcoredevice 核心设备结构
**           pucBuf      写缓冲
**           uiStartBlk  起始地址
**           uiNBlks     块数量
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemWrtMultiBlk (PLW_SDCORE_DEVICE  psdcoredevice,
                               UINT8             *pucBuf,
                               UINT32             uiStartBlk,
                               UINT32             uiNBlks)
{
    LW_SD_MESSAGE   sdmsg;
    LW_SD_COMMAND   sdcmd;
    LW_SD_DATA      sddat;
    LW_SD_COMMAND   sdcmdStop;
    INT             iError;
    INT             iDevSta;

    iDevSta = API_SdCoreDevStaView(psdcoredevice);
    if (iDevSta != SD_DEVSTA_EXIST) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "device is not exist.\r\n");
        return  (PX_ERROR);
    }

    lib_bzero(&sdcmd, sizeof(sdcmd));
    lib_bzero(&sdcmdStop, sizeof(sdcmdStop));
    lib_bzero(&sddat, sizeof(sddat));

    sdcmd.SDCMD_uiOpcode  = SD_WRITE_MULTIPLE_BLOCK;
    sdcmd.SDCMD_uiArg     = uiStartBlk;
    sdcmd.SDCMD_uiFlag    = SD_RSP_SPI_R1 | SD_RSP_R1 | SD_CMD_ADTC;    /*  命令                        */

    sddat.SDDAT_uiBlkNum  = uiNBlks;
    sddat.SDDAT_uiBlkSize = SD_MEM_DEFAULT_BLKSIZE;
    sddat.SDDAT_uiFlags   = SD_DAT_WRITE;                               /*  数据                        */

    /*
     * 在多块写传输中,要发送停止命令.该命令在SD和SPI模式下不同
     */
    if (COREDEV_IS_SD(psdcoredevice)) {
        sdcmdStop.SDCMD_uiOpcode = SD_STOP_TRANSMISSION;
        sdcmdStop.SDCMD_uiFlag   = SD_RSP_SPI_R1B | SD_RSP_R1B | SD_CMD_AC;
        sdmsg.SDMSG_psdcmdStop   = &sdcmdStop;                          /*  停止命令                    */
    } else {
        sdmsg.SDMSG_psdcmdStop   = LW_NULL ;
    }

    sdmsg.SDMSG_pucWrtBuffer = pucBuf;
    sdmsg.SDMSG_psdData      = &sddat;
    sdmsg.SDMSG_psdcmdCmd    = &sdcmd;
    sdmsg.SDMSG_pucRdBuffer  = LW_NULL;                                 /*  读缓冲为空                  */

    iError = API_SdCoreDevTransfer(psdcoredevice, &sdmsg, 1);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "request error.\r\n");
        return  (PX_ERROR);
    }

    /*
     * SPI多块写,最后发送停止数据令牌
     */
    if (COREDEV_IS_SPI(psdcoredevice)) {
        API_SdCoreSpiMulWrtStop(psdcoredevice);
    }

    iError = __sdMemTestBusy(psdcoredevice, __SD_BUSY_TYPE_PROG);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "check busy error.\r\n");
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdMemRdSingleBlk
** 功能描述: SD记忆卡设备读单块
** 输    入: psdcoredevice 核心设备结构
**           pucBuf      读缓冲
**           uiStartBlk  起始地址
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemRdSingleBlk (PLW_SDCORE_DEVICE  psdcoredevice,
                               UINT8             *pucBuf,
                               UINT32             uiStartBlk)
{
    LW_SD_MESSAGE   sdmsg;
    LW_SD_COMMAND   sdcmd;
    LW_SD_DATA      sddat;
    INT             iError;
    INT             iDevSta;

    iDevSta = API_SdCoreDevStaView(psdcoredevice);
    if (iDevSta != SD_DEVSTA_EXIST) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "device is not exist.\r\n");
        return  (PX_ERROR);
    }

    lib_bzero(&sdcmd, sizeof(sdcmd));
    lib_bzero(&sddat, sizeof(sddat));

    sdcmd.SDCMD_uiOpcode   = SD_READ_SINGLE_BLOCK;
    sdcmd.SDCMD_uiArg      = uiStartBlk;
    sdcmd.SDCMD_uiFlag     = SD_RSP_SPI_R1 | SD_RSP_R1 | SD_CMD_ADTC;   /*  命令                        */

    sddat.SDDAT_uiBlkNum   = 1;
    sddat.SDDAT_uiBlkSize  = SD_MEM_DEFAULT_BLKSIZE;
    sddat.SDDAT_uiFlags    = SD_DAT_READ;                               /*  数据                        */

    sdmsg.SDMSG_pucRdBuffer  = pucBuf;
    sdmsg.SDMSG_psdData      = &sddat;
    sdmsg.SDMSG_psdcmdCmd    = &sdcmd;
    sdmsg.SDMSG_psdcmdStop   = LW_NULL;
    sdmsg.SDMSG_pucWrtBuffer = LW_NULL;                                 /*  写缓冲为空                  */

    iError = API_SdCoreDevTransfer(psdcoredevice, &sdmsg, 1);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "request error.\r\n");
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdMemRdMultiBlk
** 功能描述: SD记忆卡设备读多块
** 输    入: psdcoredevice 核心设备结构
**           pucBuf      读缓冲
**           uiStartBlk  起始地址
**           uiNBlks     块数量
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemRdMultiBlk (PLW_SDCORE_DEVICE   psdcoredevice,
                              UINT8              *pucBuf,
                              UINT32              uiStartBlk,
                              UINT32              uiNBlks)
{
    LW_SD_MESSAGE   sdmsg;
    LW_SD_COMMAND   sdcmd;
    LW_SD_DATA      sddat;
    LW_SD_COMMAND   sdcmdStop;
    INT             iError;
    INT             iDevSta;

    iDevSta = API_SdCoreDevStaView(psdcoredevice);
    if (iDevSta != SD_DEVSTA_EXIST) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "device is not exist.\r\n");
        return  (PX_ERROR);
    }

    lib_bzero(&sdcmd, sizeof(sdcmd));
    lib_bzero(&sdcmdStop, sizeof(sdcmdStop));
    lib_bzero(&sddat, sizeof(sddat));

    sdcmd.SDCMD_uiOpcode  = SD_READ_MULTIPLE_BLOCK;
    sdcmd.SDCMD_uiArg     = uiStartBlk;
    sdcmd.SDCMD_uiFlag    = SD_RSP_SPI_R1 | SD_RSP_R1 | SD_CMD_ADTC;    /*  命令                        */

    sddat.SDDAT_uiBlkNum  = uiNBlks;
    sddat.SDDAT_uiBlkSize = SD_MEM_DEFAULT_BLKSIZE;
    sddat.SDDAT_uiFlags   = SD_DAT_READ ;                               /*  数据                        */

    /*
     * 在多块读操作中, SPI和SD模式的停止命令是相同的
     */
    sdcmdStop.SDCMD_uiOpcode = SD_STOP_TRANSMISSION;
    sdcmdStop.SDCMD_uiFlag   = SD_RSP_SPI_R1B | SD_RSP_R1B | SD_CMD_AC; /*  停止命令                    */

    sdmsg.SDMSG_pucRdBuffer  = pucBuf;
    sdmsg.SDMSG_psdData      = &sddat;
    sdmsg.SDMSG_psdcmdCmd    = &sdcmd;
    sdmsg.SDMSG_psdcmdStop   = &sdcmdStop;
    sdmsg.SDMSG_pucWrtBuffer = LW_NULL;                                 /*  写缓冲为空                  */

    iError = API_SdCoreDevTransfer(psdcoredevice, &sdmsg, 1);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "request error.\r\n");
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdMemBlkWrt
** 功能描述: SD记忆卡块设备写
** 输    入: psdblkdevice     块设备结构
**           pvWrtBuffer      写缓冲
**           ulStartBlk       起始地址
**           ulBlkCount       块数量
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemBlkWrt(__PSD_BLK_DEV   psdblkdevice,
                         VOID           *pvWrtBuffer,
                         ULONG           ulStartBlk,
                         ULONG           ulBlkCount)
{
    INT                iError;
    INT                iDevSta;
    PLW_SDCORE_DEVICE  psdcoredevice;
    LW_SDDEV_CSD       sddevcsd;

    if (!psdblkdevice || !pvWrtBuffer) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "parameter error.\r\n");
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdcoredevice = ((__PSD_BLK_DEV)psdblkdevice)->SDBLKDEV_pcoreDev;
    if (!psdcoredevice) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "no core device member.\r\n");
        return  (PX_ERROR);
    }

    iDevSta = API_SdCoreDevStaView(psdcoredevice);
    if (iDevSta != SD_DEVSTA_EXIST) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "device is not exist.\r\n");
        return  (PX_ERROR);
    }


    iError = __sdCoreDevSelect(psdcoredevice);                          /*  选择设备                    */
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "select device failed.\r\n");
        return  (PX_ERROR);
    }

    API_SdCoreDevCsdView(psdcoredevice, &sddevcsd);

    if ((ulStartBlk + ulBlkCount) > sddevcsd.DEVCSD_uiCapacity) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "block number out of range.\r\n");
        iError = PX_ERROR;
        goto __error_handle;
    }

    /*
     * 块地址转换
     */
    if (!__SDMEM_BLKADDR(psdblkdevice)) {
        ulStartBlk = ulStartBlk << sddevcsd.DEVCSD_ucWriteBlkLenBits;
    }

    if (ulBlkCount <= 1) {
        iError = __sdMemWrtSingleBlk(psdcoredevice, (UINT8 *)pvWrtBuffer, (UINT32)ulStartBlk);
    } else {
        iError = __sdMemWrtMultiBlk(psdcoredevice, (UINT8 *)pvWrtBuffer,
                                    (UINT32)ulStartBlk, (UINT32)ulBlkCount);
    }

__error_handle:
    if (COREDEV_IS_SD(psdcoredevice)) {
        __sdCoreDevDeSelect(psdcoredevice);                                 /*  取消设备                    */
    }

    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __sdMemBlkWrt
** 功能描述: SD记忆卡块设备读
** 输    入: psdblkdevice   块设备结构
**           pvRdBuffer     读缓冲
**           ulStartBlk     起始地址
**           ulBlkCount     块数量
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemBlkRd (__PSD_BLK_DEV   psdblkdevice,
                         VOID           *pvRdBuffer,
                         ULONG           ulStartBlk,
                         ULONG           ulBlkCount)
{
    INT                iError;
    INT                iDevSta;
    PLW_SDCORE_DEVICE  psdcoredevice;
    LW_SDDEV_CSD       sddevcsd;

    if (!psdblkdevice || !pvRdBuffer) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "parameter error.\r\n");
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdcoredevice = ((__PSD_BLK_DEV)psdblkdevice)->SDBLKDEV_pcoreDev;
    if (!psdcoredevice) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "no core device.\r\n");
        return  (PX_ERROR);
    }

    iDevSta = API_SdCoreDevStaView(psdcoredevice);
    if (iDevSta != SD_DEVSTA_EXIST) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "device is not exist.\r\n");
        return  (PX_ERROR);
    }


    if (COREDEV_IS_SD(psdcoredevice)) {
        iError = __sdCoreDevSelect(psdcoredevice);                      /*  选择设备                    */
        if (iError != ERROR_NONE) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "select device failed.\r\n");
            return  (PX_ERROR);
        }
    }

    API_SdCoreDevCsdView(psdcoredevice, &sddevcsd);

    if ((ulStartBlk + ulBlkCount) > sddevcsd.DEVCSD_uiCapacity) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "block number is out of range.\r\n");
        iError = PX_ERROR;
        goto __error_handle;
    }

    /*
     * 块地址转换
     */
    if (!__SDMEM_BLKADDR(psdblkdevice)) {
        ulStartBlk = ulStartBlk << sddevcsd.DEVCSD_ucReadBlkLenBits;
    }


    if (ulBlkCount <= 1) {
        iError = __sdMemRdSingleBlk(psdcoredevice, (UINT8 *)pvRdBuffer, (UINT32)ulStartBlk);
    } else {
        iError = __sdMemRdMultiBlk(psdcoredevice, (UINT8 *)pvRdBuffer,
                                   (UINT32)ulStartBlk, (UINT32)ulBlkCount);
    }

__error_handle:
    if (COREDEV_IS_SD(psdcoredevice)) {
        __sdCoreDevDeSelect(psdcoredevice);                             /*  取消设备                    */
    }

    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __sdMemIoctl
** 功能描述: SD记忆卡块设备IO控制
** 输    入: psdblkdevice   块设备结构
**           iCmd           控制命令
**           lArg           参数
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemIoctl (__PSD_BLK_DEV    psdblkdevice,
                         INT              iCmd,
                         LONG             lArg)
{
    INT          iDevSta;
    LW_SDDEV_CSD sdcsd;

    if (!psdblkdevice) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    iDevSta = API_SdCoreDevStaView(psdblkdevice->SDBLKDEV_pcoreDev);
    if (iDevSta != SD_DEVSTA_EXIST) {
        return  (PX_ERROR);
    }

    switch (iCmd) {

    case FIOFLUSH:                                                      /*  将缓存回写到磁盘            */
    case FIOUNMOUNT:                                                    /*  卸载卷                      */
    case FIODISKINIT:                                                   /*  初始化设备                  */
    case FIODISKCHANGE:                                                 /*  磁盘媒质发生变化            */
        break;

    case LW_BLKD_GET_SECSIZE:
    case LW_BLKD_GET_BLKSIZE:
        *((LONG *)lArg) = SD_MEM_DEFAULT_BLKSIZE;
        break;

    case LW_BLKD_GET_SECNUM:
        API_SdCoreDevCsdView(psdblkdevice->SDBLKDEV_pcoreDev, &sdcsd);
        *((UINT32 *)lArg) = sdcsd.DEVCSD_uiCapacity;
        break;

    case FIOWTIMEOUT:
    case FIORTIMEOUT:
        break;

    default:
        return  (ENOSYS);
        break;
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdMemStatus
** 功能描述: SD记忆卡块设备状态函数
** 输    入: psdblkdevice  块设备结构
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemStatus (__PSD_BLK_DEV   psdblkdevice)
{
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdMemStatus
** 功能描述: SD记忆卡块设备复位
** 输    入: psdblkdevice  块设备结构
** 输    出: NONE
** 返    回: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdMemReset (__PSD_BLK_DEV    psdblkdevice)
{
    return  (ERROR_NONE);
}
#endif                                                                  /*  (LW_CFG_DEVICE_EN > 0)      */
                                                                        /*  (LW_CFG_SDCARD_EN > 0)      */
/*********************************************************************************************************
  END
*********************************************************************************************************/
