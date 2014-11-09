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
** 文   件   名: sdhci.h
**
** 创   建   人: Zeng.Bo (曾波)
**
** 文件创建日期: 2011 年 01 月 17 日
**
** 描        述: sd标准主控制器驱动源文件(SD Host Controller Simplified Specification Version 2.00).

** BUG:
2011.03.02  增加主控传输模式查看\设置函数.允许动态改变传输模式(主控上不存在设备的情况下).
2011.04.07  增加 SDMA 传输功能.
2011.04.07  考虑到 SD 控制器在不同平台上其寄存器可能在 IO 空间,也可能在内存空间,所以读写寄存器的
            6个函数申明为外部函数,由具体平台的驱动实现.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_SDCARD_EN > 0)
#include "sdhci.h"
/*********************************************************************************************************
  内部宏
*********************************************************************************************************/
#define __SDHCI_HOST_DEVNUM_MAX   1                                     /*  一个控制器支持的槽数量      */
#define __SDHCI_MAX_BASE_CLK      102400000                             /*  主控允许最大基时钟102.4Mhz  */
#define __SDHCI_MAX_CLK           48000000                              /*  最大时钟25Mhz               */
#define __SDHCI_NOR_CLK           100000                                /*  一般时钟400Khz              */

#define __SDHCI_CMD_TOUT_SEC      2                                     /*  命令超时物理时间(经验值)    */
#define __SDHCI_READ_TOUT_SEC     2                                     /*  读超时物理时间(经验值)      */
#define __SDHCI_WRITE_TOUT_SEC    2                                     /*  写超时物理时间(经验值)      */
#define __SDHCI_SDMARW_TOUT_SEC   2                                     /*  SDMA超时物理时间(经验值)    */

#define __SDHCI_CMD_RETRY         0x40000000                            /*  命令超时计数                */
#define __SDHCI_READ_RETRY        0x40000000                            /*  读超时计数                  */
#define __SDHCI_WRITE_RETRY       0x40000000                            /*  写超时计数                  */
#define __SDHCI_SDMARW_RETRY      50                                    /*  SDMA读写超时计数            */

#define __SDHCI_DMA_BOUND_LEN     (2 << (12 + __SDHCI_DMA_BOUND_NBITS))
#define __SDHCI_DMA_BOUND_NBITS   7                                     /*  系统连续内存边界位指示      */
                                                                        /*  当前设定值为 7 (最大):      */
                                                                        /*  2 << (12 + 7) = 512k        */
                                                                        /*  当数据搬移时超过512K边界,需 */
                                                                        /*  要更新DMA地址               */
#define __SDHCI_HOST_NAME(phs)    \
        (phs->SDHCIHS_psdadapter->SDADAPTER_busadapter.BUSADAPTER_cName)
/*********************************************************************************************************
  标准主控器功能描述结构
*********************************************************************************************************/
typedef struct __sdhci_capab {
    UINT32      SDHCICAP_uiBaseClkFreq;                                 /*  基时钟频率                  */
    UINT32      SDHCICAP_uiMaxBlkSize;                                  /*  支持的最大块长度            */
    UINT32      SDHCICAP_uiVoltage;                                     /*  电压支持情况                */

    BOOL        SDHCICAP_bCanSdma;                                      /*  是否支持SDMA                */
    BOOL        SDHCICAP_bCanAdma;                                      /*  是否支持ADMA                */
    BOOL        SDHCICAP_bCanHighSpeed;                                 /*  是否支持高速传输            */
    BOOL        SDHCICAP_bCanSusRes;                                    /*  是否支持挂起\恢复功能       */
}__SDHCI_CAPAB, *__PSDHCI_CAPAB;
/*********************************************************************************************************
  标准主控器HOST结构
*********************************************************************************************************/
typedef struct __sdhci_host {
    LW_SD_FUNCS         SDHCIHS_sdfunc;                                 /*  对应的驱动函数              */
    PLW_SD_ADAPTER      SDHCIHS_psdadapter;                             /*  对应的总线适配器            */
    LW_SDHCI_HOST_ATTR  SDHCIHS_hostattr;                               /*  主控属性                    */
    __SDHCI_CAPAB       SDHCIHS_sdhcicap;                               /*  主控功能                    */
    atomic_t            SDHCIHS_atomicDevCnt;                           /*  设备计数                    */

    UINT32              SDHCIHS_ucTransferMod;                          /*  主控使用的传输模式          */

#if LW_CFG_VMM_EN
    UINT8              *SDHCIHS_pucDmaBuf;                              /*  使用DMA传输开启cache时需要  */
#endif

    INT               (*SDHCIHS_pfuncMasterXfer)                        /*  主控当前使用的传输函数      */
                      (
                        struct __sdhci_host *psdhcihost,
                        PLW_SD_DEVICE        psddev,
                        PLW_SD_MESSAGE       psdmsg,
                        INT                  iNum
                      );
} __SDHCI_HOST, *__PSDHCI_HOST;
/*********************************************************************************************************
  标准主控器设备内部结构
*********************************************************************************************************/
typedef struct __sdhci_dev {
    PLW_SD_DEVICE       SDHCIDEV_psddevice;                             /*  对应的SD设备                */
    __PSDHCI_HOST       SDHCIDEV_psdhcihost;                            /*  对应的HOST                  */
    CHAR                SDHCIDEV_pcDevName[LW_CFG_OBJECT_NAME_SIZE];    /*  设备名称(与对应SD设备相同)  */
} __SDHCI_DEVICE, *__PSDHCI_DEVICE;
/*********************************************************************************************************
  私有函数申明
*********************************************************************************************************/
static VOID __sdhciHostCapDecode(PLW_SDHCI_HOST_ATTR psdhcihostattr, __PSDHCI_CAPAB psdhcicap);
/*********************************************************************************************************
  CALLBACK FOR SD BUS LAYER
*********************************************************************************************************/
static INT __sdhciTransfer(PLW_SD_ADAPTER      psdadapter,
                           PLW_SD_DEVICE       psddev,
                           PLW_SD_MESSAGE      psdmsg,
                           INT                 iNum);
static INT __sdhciIoCtl(PLW_SD_ADAPTER  psdadapter,
                        INT             iCmd,
                        LONG            lArg);
/*********************************************************************************************************
  FOR I\O CONTROL PRIVATE
*********************************************************************************************************/
static INT __sdhciClockSet(__PSDHCI_HOST     psdhcihost, UINT32 uiSetClk);
static INT __sdhciBusWidthSet(__PSDHCI_HOST  psdhcihost, UINT32 uiBusWidth);
static INT __sdhciPowerOn(__PSDHCI_HOST      psdhcihost);
static INT __sdhciPowerOff(__PSDHCI_HOST     psdhcihost);
static INT __sdhciPowerSetVol(__PSDHCI_HOST  psdhcihost,
                              UINT8          ucVol,
                              BOOL           uiVol);
/*********************************************************************************************************
  FOR TRANSFER PRIVATE
*********************************************************************************************************/
static INT __sdhciTransferNorm(__PSDHCI_HOST       psdhcihost,
                               PLW_SD_DEVICE       psddev,
                               PLW_SD_MESSAGE      psdmsg,
                               INT                 iNum);
static INT __sdhciTransferSdma(__PSDHCI_HOST       psdhcihost,
                               PLW_SD_DEVICE       psddev,
                               PLW_SD_MESSAGE      psdmsg,
                               INT                 iNum);
static INT __sdhciTransferAdma(__PSDHCI_HOST       psdhcihost,
                               PLW_SD_DEVICE       psddev,
                               PLW_SD_MESSAGE      psdmsg,
                               INT                 iNum);

static INT  __sdhciSendCmd(__PSDHCI_HOST   psdhcihost,
                           PLW_SD_COMMAND  psdcmd,
                           PLW_SD_DATA     psddat);

static VOID __sdhciTransferModSet(__PSDHCI_HOST   psdhcihost, PLW_SD_DATA    psddat);
static VOID __sdhciDataPrepareNorm(__PSDHCI_HOST  psdhcihost, PLW_SD_DATA    psddat);
static VOID __sdhciDataPrepareSdma(__PSDHCI_HOST  psdhcihost, PLW_SD_MESSAGE psdmsg);
static VOID __sdhciDataPrepareAdma(__PSDHCI_HOST  psdhcihost, PLW_SD_DATA    psddat);

static INT __sdhciDataReadNorm(__PSDHCI_HOST  psdhcihost,
                               UINT32         uiBlkSize,
                               UINT32         uiBlkNum,
                               PUCHAR         pucRdBuff);
static INT __sdhciDataWriteNorm(__PSDHCI_HOST  psdhcihost,
                                UINT32         uiBlkSize,
                                UINT32         uiBlkNum,
                                PUCHAR         pucWrtBuff);

static INT __sdhciDataFinishSdma(__PSDHCI_HOST   psdhcihost, PLW_SD_MESSAGE psdmsg);

static VOID __sdhciTransferIntSet(__PSDHCI_HOST  psdhcihost);
static VOID __sdhciIntDisAndEn(__PSDHCI_HOST     psdhcihost,
                               UINT32            uiDisMask,
                               UINT32            uiEnMask);
/*********************************************************************************************************
  FOR DEBUG
*********************************************************************************************************/
#ifdef __SYLIXOS_DEBUG
static VOID __sdhciPreStaShow(PLW_SDHCI_HOST_ATTR psdhcihostattr);
static VOID __sdhciIntStaShow(PLW_SDHCI_HOST_ATTR psdhcihostattr);
#else
#define     __sdhciPreStaShow(x)
#define     __sdhciIntStaShow(x)
#endif                                                                  /*  __SYLIXOS_DEBUG             */
/*********************************************************************************************************
  INLINE FUNCTIONS
*********************************************************************************************************/
static LW_INLINE INT __sdhciCmdRespType (PLW_SD_COMMAND psdcmd)
{
    UINT32  uiRespFlag = SD_RESP_TYPE(psdcmd);
    INT     iType      = 0;

    if (!(uiRespFlag & SD_RSP_PRESENT)) {
        iType = SDHCI_CMD_RESP_TYPE_NONE;
    } else if (uiRespFlag & SD_RSP_136) {
        iType = SDHCI_CMD_RESP_TYPE_LONG;
    } else if (uiRespFlag & SD_RSP_BUSY) {
        iType = SDHCI_CMD_RESP_TYPE_SHORT_BUSY;
    } else {
        iType = SDHCI_CMD_RESP_TYPE_SHORT;
    }

    return  (iType);
}
static LW_INLINE VOID __sdhciSdmaAddrUpdate (__PSDHCI_HOST psdhcihost, LONG lSysAddr)
{
    SDHCI_WRITEL(&psdhcihost->SDHCIHS_hostattr,
                 SDHCI_SYS_SDMA,
                 (UINT32)lSysAddr);
}
static LW_INLINE VOID __sdhciHostRest (__PSDHCI_HOST psdhcihost)
{
    SDHCI_WRITEB(&psdhcihost->SDHCIHS_hostattr,
                 SDHCI_SOFTWARE_RESET,
                 (SDHCI_SFRST_CMD | SDHCI_SFRST_DATA));
}
/*********************************************************************************************************
** 函数名称: API_SdhciHostCreate
** 功能描述: 创建SD标准主控制器
** 输    入: psdAdapter     所对应的SD总线适配器名称
**           psdhcihostattr 主控器属性
** 输    出: NONE
** 返    回: 成功,返回主控指针.失败,返回NULL.
*********************************************************************************************************/
LW_API PVOID  API_SdhciHostCreate (CPCHAR               pcAdapterName,
                                   PLW_SDHCI_HOST_ATTR  psdhcihostattr)
{
    PLW_SD_ADAPTER  psdadapter  = LW_NULL;
    __PSDHCI_HOST   psdhcihost  = LW_NULL;
    PVOID           pvDmaBuf;
    INT             iError;

    if (!pcAdapterName || !psdhcihostattr) {
        _ErrorHandle(EINVAL);
        return  ((PVOID)LW_NULL);
    }

    if (psdhcihostattr->SDHCIHOST_uiMaxClock > __SDHCI_MAX_BASE_CLK) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "max clock must be less than 102.4Mhz.\r\n");
        _ErrorHandle(EINVAL);
        return  ((PVOID)LW_NULL);
    }

    psdhcihost = (__PSDHCI_HOST)__SHEAP_ALLOC(sizeof(__SDHCI_HOST));   /*  分配HOST                     */
    if (!psdhcihost) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  ((PVOID)LW_NULL);
    }
    lib_bzero(psdhcihost, sizeof(__SDHCI_HOST));

    __sdhciHostCapDecode(psdhcihostattr, &psdhcihost->SDHCIHS_sdhcicap);/*  主控功能解析                */

    API_SdLibInit();                                                    /*  初始化sd组建库              */

    psdhcihost->SDHCIHS_sdfunc.SDFUNC_pfuncMasterXfer = __sdhciTransfer;
    psdhcihost->SDHCIHS_sdfunc.SDFUNC_pfuncMasterCtl  = __sdhciIoCtl;   /*  初始化驱动函数              */

    iError = API_SdAdapterCreate(pcAdapterName, &psdhcihost->SDHCIHS_sdfunc);
    if (iError != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "create sd adapter error.\r\n");
        __SHEAP_FREE(psdhcihost);
        return  ((PVOID)LW_NULL);
    }

    psdadapter = API_SdAdapterGet(pcAdapterName);                       /*  获得适配器                  */

    psdhcihost->SDHCIHS_psdadapter             = psdadapter;
    psdhcihost->SDHCIHS_atomicDevCnt.counter   = 0;                     /*  初始设备数为0               */
    psdhcihost->SDHCIHS_pfuncMasterXfer        = __sdhciTransferNorm;
    psdhcihost->SDHCIHS_ucTransferMod          = SDHCIHOST_TMOD_SET_NORMAL;
                                                                        /*  默认使用一般传输            */
#if LW_CFG_VMM_EN
    pvDmaBuf = API_VmmDmaAlloc(__SDHCI_DMA_BOUND_LEN);
    if (!pvDmaBuf) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        __SHEAP_FREE(psdhcihost);
        return  ((PVOID)LW_NULL);
    }
    psdhcihost->SDHCIHS_pucDmaBuf = (UINT8 *)pvDmaBuf;
#endif

    lib_memcpy(&psdhcihost->SDHCIHS_hostattr, psdhcihostattr, sizeof(LW_SDHCI_HOST_ATTR));
                                                                        /*  保存属性域                  */
    __sdhciHostRest(psdhcihost);
    __sdhciPowerSetVol(psdhcihost, SDHCI_POWCTL_330, LW_TRUE);          /*  3.3v                        */
    SDHCI_WRITEB(&psdhcihost->SDHCIHS_hostattr, SDHCI_TIMEOUT_CONTROL, 0xe);
                                                                        /*  使用最大超时                */
    __sdhciIntDisAndEn(psdhcihost,
                       SDHCI_INT_ALL_MASK,
                       SDHCI_INT_BUS_POWER | SDHCI_INT_DATA_END_BIT |
                       SDHCI_INT_DATA_CRC  | SDHCI_INT_DATA_TIMEOUT |
                       SDHCI_INT_INDEX     | SDHCI_INT_END_BIT      |
                       SDHCI_INT_CRC       | SDHCI_INT_TIMEOUT      |
                       SDHCI_INT_DATA_END  | SDHCI_INT_RESPONSE);       /*  初始化时钟主控器中断        */

    return  ((PVOID)psdhcihost);
}
/*********************************************************************************************************
** 函数名称: API_SdhciHostDelete
** 功能描述: 删除SD标准主控制器
** 输    入: pvHost    主控器指针
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
LW_API INT  API_SdhciHostDelete (PVOID  pvHost)
{
    __PSDHCI_HOST    psdhcihost = LW_NULL;

    if (!pvHost) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdhcihost   = (__PSDHCI_HOST)pvHost;

    if (API_AtomicGet(&psdhcihost->SDHCIHS_atomicDevCnt)) {
        _ErrorHandle(EBUSY);
        return  (PX_ERROR);
    }

    API_SdAdapterDelete(__SDHCI_HOST_NAME(psdhcihost));

#if LW_CFG_VMM_EN
    API_VmmDmaFree(psdhcihost->SDHCIHS_pucDmaBuf);
#endif

    __SHEAP_FREE(psdhcihost);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdhciHostTransferModGet
** 功能描述: 获得主控支持的传输模式
** 输    入: pvHost    主控器指针
** 输    出: NONE
** 返    回: 成功,返回传输模式支持情况; 失败, 返回 0;
*********************************************************************************************************/
LW_API INT  API_SdhciHostTransferModGet (PVOID    pvHost)
{
    __PSDHCI_HOST   psdhcihost  = LW_NULL;
    INT             iTmodFlg    = SDHCIHOST_TMOD_CAN_NORMAL;            /*  总是支持一般传输            */

    if (!pvHost) {
        _ErrorHandle(EINVAL);
        return  (0);
    }

    psdhcihost   = (__PSDHCI_HOST)pvHost;

    if (psdhcihost->SDHCIHS_sdhcicap.SDHCICAP_bCanSdma) {
        iTmodFlg |= SDHCIHOST_TMOD_CAN_SDMA;
    }

    if (psdhcihost->SDHCIHS_sdhcicap.SDHCICAP_bCanAdma) {
        iTmodFlg |= SDHCIHOST_TMOD_CAN_ADMA;
    }

    return (iTmodFlg);
}
/*********************************************************************************************************
** 函数名称: API_SdhciHostTransferModSet
** 功能描述: 设置主控的传输模式
** 输    入: pvHost    主控器指针
**           iTmod     要设置的传输模式
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
LW_API INT  API_SdhciHostTransferModSet (PVOID    pvHost, INT   iTmod)
{
    __PSDHCI_HOST    psdhcihost = LW_NULL;

    if (!pvHost) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdhcihost   = (__PSDHCI_HOST)pvHost;

    /*
     * 必须保证主控上没有设备,才能更改主控传输模式
     */
    if (API_AtomicGet(&psdhcihost->SDHCIHS_atomicDevCnt)) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "device exist.\r\n");
        _ErrorHandle(EBUSY);
        return  (PX_ERROR);
    }

    switch (iTmod) {
    
    case SDHCIHOST_TMOD_SET_NORMAL:
        psdhcihost->SDHCIHS_ucTransferMod    = SDHCIHOST_TMOD_SET_NORMAL;
        psdhcihost->SDHCIHS_pfuncMasterXfer  = __sdhciTransferNorm;
        break;

    case SDHCIHOST_TMOD_SET_SDMA:
        if (psdhcihost->SDHCIHS_sdhcicap.SDHCICAP_bCanSdma) {
            psdhcihost->SDHCIHS_ucTransferMod    = SDHCIHOST_TMOD_SET_SDMA;
            psdhcihost->SDHCIHS_pfuncMasterXfer  = __sdhciTransferSdma;
        } else {
            return  (PX_ERROR);
        }
        break;

    case SDHCIHOST_TMOD_SET_ADMA:
        if (psdhcihost->SDHCIHS_sdhcicap.SDHCICAP_bCanAdma) {
            psdhcihost->SDHCIHS_ucTransferMod     = SDHCIHOST_TMOD_SET_ADMA;
            psdhcihost->SDHCIHS_pfuncMasterXfer   = __sdhciTransferAdma;
        } else {
            return  (PX_ERROR);
        }
        break;

    default:
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdhciDeviceAdd
** 功能描述: 向SD标准主控制器上添加一个SD设备
** 输    入: pvHost      主控器指针
**           pcDevice    要添加的SD设备名称
** 输    出: NONE
** 返    回: 成功,返回设备指针.失败,返回NULL.
*********************************************************************************************************/
LW_API PVOID  API_SdhciDeviceAdd (PVOID     pvHost,
                                  CPCHAR    pcDeviceName)
{
    __PSDHCI_HOST   psdhcihost    = LW_NULL;
    __PSDHCI_DEVICE psdhcidevice  = LW_NULL;
    PLW_SD_DEVICE   psddevice     = LW_NULL;

    if (!pvHost) {
        _ErrorHandle(EINVAL);
        return  ((PVOID)0);
    }

    psdhcihost = (__PSDHCI_HOST)pvHost;

    if (API_AtomicGet(&psdhcihost->SDHCIHS_atomicDevCnt) >= __SDHCI_HOST_DEVNUM_MAX) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not add more devices.\r\n");
        return  ((PVOID)0);
    }

    psddevice = API_SdDeviceGet(__SDHCI_HOST_NAME(psdhcihost), pcDeviceName);
    if (!psddevice) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "find sd device failed.\r\n");
       return  ((PVOID)0);
    }

    /*
     * 分配一个SDHCI DEVICE 结构
     */
    psdhcidevice = (__PSDHCI_DEVICE)__SHEAP_ALLOC(sizeof(__SDHCI_DEVICE));
    if (!psdhcidevice) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  ((PVOID)0);
    }

    lib_bzero(psdhcidevice, sizeof(__SDHCI_DEVICE));

    psdhcidevice->SDHCIDEV_psddevice  = psddevice;
    psdhcidevice->SDHCIDEV_psdhcihost = psdhcihost;

    psddevice->SDDEV_pvUsr = psdhcidevice;                              /*  绑定用户数据                */

    API_AtomicInc(&psdhcihost->SDHCIHS_atomicDevCnt);                   /*  设备++                      */

    return  ((PVOID)psdhcidevice);
}
/*********************************************************************************************************
** 函数名称: API_SdhciDeviceRemove
** 功能描述: 从SD标准主控制器上移除一个SD设备
** 输    入: pvDevice  要移除的SD设备指针
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
LW_API INT  API_SdhciDeviceRemove (PVOID pvDevice)
{
    __PSDHCI_HOST   psdhcihost    = LW_NULL;
    __PSDHCI_DEVICE psdhcidevice  = LW_NULL;

    if (!pvDevice) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdhcidevice  = (__PSDHCI_DEVICE)pvDevice;
    psdhcihost    = psdhcidevice->SDHCIDEV_psdhcihost;

    psdhcidevice->SDHCIDEV_psddevice->SDDEV_pvUsr = LW_NULL;            /*  用户数据无效                */
    __SHEAP_FREE(psdhcidevice);
    API_AtomicDec(&psdhcihost->SDHCIHS_atomicDevCnt);                   /*  设备--                      */

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdhciDeviceUsageInc
** 功能描述: 设备的使用计数增加一
** 输    入: pvDevice  SD设备指针
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
LW_API INT  API_SdhciDeviceUsageInc (PVOID  pvDevice)
{
    __PSDHCI_DEVICE psdhcidevice  = LW_NULL;
    INT             iError;

    if (!pvDevice) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdhcidevice  = (__PSDHCI_DEVICE)pvDevice;

    iError = API_SdDeviceUsageInc(psdhcidevice->SDHCIDEV_psddevice);

    return  (iError);
}
/*********************************************************************************************************
** 函数名称: API_SdhciDeviceUsageInc
** 功能描述: 设备的使用计数减一
** 输    入: pvDevice  SD设备指针
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
LW_API INT  API_SdhciDeviceUsageDec (PVOID  pvDevice)
{
    __PSDHCI_DEVICE psdhcidevice  = LW_NULL;
    INT             iError;

    if (!pvDevice) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdhcidevice  = (__PSDHCI_DEVICE)pvDevice;

    iError = API_SdDeviceUsageDec(psdhcidevice->SDHCIDEV_psddevice);

    return  (iError);
}
/*********************************************************************************************************
** 函数名称: API_SdhciDeviceUsageInc
** 功能描述: 获得设备的使用计数
** 输    入: pvDevice  SD设备指针
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
LW_API INT  API_SdhciDeviceUsageGet (PVOID  pvDevice)
{
    __PSDHCI_DEVICE psdhcidevice  = LW_NULL;
    INT             iError;

    if (!pvDevice) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdhcidevice  = (__PSDHCI_DEVICE)pvDevice;

    iError = API_SdDeviceUsageGet(psdhcidevice->SDHCIDEV_psddevice);

    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __sdhciTransferNorm
** 功能描述: 一般传输
** 输    入: psdhcihost  SD主控制器
**           psddev      SD设备
**           psdmsg      SD传输控制消息组
**           iNum        消息个数
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciTransferNorm (__PSDHCI_HOST       psdhcihost,
                                PLW_SD_DEVICE       psddev,
                                PLW_SD_MESSAGE      psdmsg,
                                INT                 iNum)
{
    PLW_SD_COMMAND psdcmd  = LW_NULL;
    PLW_SD_DATA    psddat  = LW_NULL;
    INT            iError  = ERROR_NONE;
    INT            i       = 0;

    /*
     * 传输消息
     */
    while ((i < iNum) && (psdmsg != LW_NULL)) {
        psdcmd = psdmsg->SDMSG_psdcmdCmd;
        psddat = psdmsg->SDMSG_psdData;

        __sdhciDataPrepareNorm(psdhcihost, psddat);                     /*  数据准备                    */
        iError = __sdhciSendCmd(psdhcihost, psdcmd, psddat);            /*  发送命令                    */
        if (iError != ERROR_NONE) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "send cmd error.\r\n");
            return  (PX_ERROR);
        }

        /*
         * 数据处理
         */
        if (psddat) {

            /*
             * TODO: 流模式..
             */
            if (SD_DAT_IS_STREAM(psddat)) {
                _DebugHandle(__ERRORMESSAGE_LEVEL, "don't support stream mode.\r\n");
                return  (PX_ERROR);

            }

            if (SD_DAT_IS_READ(psddat)) {
                iError = __sdhciDataReadNorm(psdhcihost,
                                             psddat->SDDAT_uiBlkSize,
                                             psddat->SDDAT_uiBlkNum,
                                             psdmsg->SDMSG_pucRdBuffer);
                if (iError != ERROR_NONE) {
                    _DebugHandle(__ERRORMESSAGE_LEVEL, "read error.\r\n");
                    return  (PX_ERROR);

                }
            } else {
                iError = __sdhciDataWriteNorm(psdhcihost,
                                              psddat->SDDAT_uiBlkSize,
                                              psddat->SDDAT_uiBlkNum,
                                              psdmsg->SDMSG_pucWrtBuffer);
                if (iError != ERROR_NONE) {
                    _DebugHandle(__ERRORMESSAGE_LEVEL, "write error.\r\n");
                    return  (PX_ERROR);
                }
            }
        }

        i++;
        psdmsg++;

        /*
         * 注意, 没有处理停止命令,因为只有多块操作时才使用,
         * 而标准主控支持ACMD12(自动停止命令功能),因此忽略.
         */
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdhciTransferSdma
** 功能描述: SDMA传输
** 输    入: psdhcihost  SD主控制器
**           psddev      SD设备
**           psdmsg      SD传输控制消息组
**           iNum        消息个数
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciTransferSdma (__PSDHCI_HOST       psdhcihost,
                                PLW_SD_DEVICE       psddev,
                                PLW_SD_MESSAGE      psdmsg,
                                INT                 iNum)
{
    PLW_SD_COMMAND psdcmd  = LW_NULL;
    PLW_SD_DATA    psddat  = LW_NULL;
    INT            iError  = ERROR_NONE;
    INT            i       = 0;
    /*
     * 传输消息
     */
    while ((i < iNum) && (psdmsg != LW_NULL)) {
        psdcmd = psdmsg->SDMSG_psdcmdCmd;
        psddat = psdmsg->SDMSG_psdData;
        __sdhciDataPrepareSdma(psdhcihost, psdmsg);                     /*  数据准备                    */
        iError = __sdhciSendCmd(psdhcihost, psdcmd, psddat);            /*  发送命令                    */
        if (iError != ERROR_NONE) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "send cmd error.\r\n");
            return  (PX_ERROR);
        }

        /*
         * 数据处理
         */
        if (psddat) {
            iError = __sdhciDataFinishSdma(psdhcihost, psdmsg);
            if (iError != ERROR_NONE) {
                return (PX_ERROR);
            }
        }

        i++;
        psdmsg++;

        /*
         * 注意, 没有处理停止命令,因为只有多块操作时才使用,
         * 而标准主控支持ACMD12(自动停止命令功能),因此忽略.
         */
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdhciTransferAdma
** 功能描述: ADMA传输
** 输    入: psdadapter  SD主控制器
**           psddev      SD设备
**           psdmsg      SD传输控制消息组
**           iNum        消息个数
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciTransferAdma (__PSDHCI_HOST       psdhcihost,
                                PLW_SD_DEVICE       psddev,
                                PLW_SD_MESSAGE      psdmsg,
                                INT                 iNum)
{
    /*
     *  TODO: adma mode support.
     */
    __sdhciDataPrepareAdma(LW_NULL, LW_NULL);                           /*  不产生未使用 warning        */
    
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __sdhciTransfer
** 功能描述: 总线传输函数
** 输    入: psdadapter  SD总线适配器
**           iCmd        控制命令
**           lArg        参数
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciTransfer (PLW_SD_ADAPTER      psdadapter,
                            PLW_SD_DEVICE       psddev,
                            PLW_SD_MESSAGE      psdmsg,
                            INT                 iNum)
{
    __PSDHCI_HOST  psdhcihost = LW_NULL;
    INT            iError     = ERROR_NONE;

    if (!psdadapter) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdhcihost = (__PSDHCI_HOST)psdadapter->SDADAPTER_psdfunc;
    if (!psdhcihost) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "no sdhci host.\r\n");
        return  (PX_ERROR);
    }

    /*
     * 执行主控上对应的总线传输函数
     */
    iError = psdhcihost->SDHCIHS_pfuncMasterXfer(psdhcihost, psddev, psdmsg, iNum);

    return (iError);

}
/*********************************************************************************************************
** 函数名称: __sdhciIoCtl
** 功能描述: SD 主控器IO控制函数
** 输    入: psdadapter  SD总线适配器
**           iCmd        控制命令
**           lArg        参数
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciIoCtl (PLW_SD_ADAPTER  psdadapter,
                         INT             iCmd,
                         LONG            lArg)
{
    __PSDHCI_HOST  psdhcihost = LW_NULL;
    INT            iError     = ERROR_NONE;

    if (!psdadapter) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    psdhcihost = (__PSDHCI_HOST)psdadapter->SDADAPTER_psdfunc;
    if (!psdhcihost) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "no sdhci host.\r\n");
        return  (PX_ERROR);
    }

    switch (iCmd) {
    
    case SDBUS_CTRL_POWEROFF:
        iError = __sdhciPowerOff(psdhcihost);
        break;

    case SDBUS_CTRL_POWERUP:
    case SDBUS_CTRL_POWERON:
        iError = __sdhciPowerOn(psdhcihost);
        break;

    case SDBUS_CTRL_SETBUSWIDTH:
        iError = __sdhciBusWidthSet(psdhcihost, (UINT32)lArg);
        break;

    case SDBUS_CTRL_SETCLK:
        if (lArg == SDARG_SETCLK_NORMAL) {
            iError =  __sdhciClockSet(psdhcihost, __SDHCI_NOR_CLK);
        } else if (lArg == SDARG_SETCLK_MAX) {
            iError =  __sdhciClockSet(psdhcihost, __SDHCI_MAX_CLK);
        } else {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "setting clock is not supported.\r\n ");
            iError = PX_ERROR;
        }
        break;

    case SDBUS_CTRL_DELAYCLK:
        break;

    case SDBUS_CTRL_GETOCR:
        *(UINT32 *)lArg = psdhcihost->SDHCIHS_sdhcicap.SDHCICAP_uiVoltage;
        iError = ERROR_NONE;
        break;

    default:
        _DebugHandle(__ERRORMESSAGE_LEVEL, "invalidate command.\r\n");
        iError = PX_ERROR;
        break;
    }

    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __sdhciHostCapDecode
** 功能描述: 解码主控的功能寄存器
** 输    入: psdhcihostattr  主控属性
** 输    出: psdhcicap       功能结构指针
** 返    回: ERROR CODE
*********************************************************************************************************/
static VOID __sdhciHostCapDecode (PLW_SDHCI_HOST_ATTR psdhcihostattr, __PSDHCI_CAPAB psdhcicap)

{
    UINT32  uiCapReg = 0;
    UINT32  uiTmp    = 0;

    if (!psdhcicap) {
        _ErrorHandle(EINVAL);
        return;
    }

    uiCapReg = SDHCI_READL(psdhcihostattr, SDHCI_CAPABILITIES);

    psdhcicap->SDHCICAP_uiBaseClkFreq  = (uiCapReg & SDHCI_CAP_BASECLK_MASK) >> SDHCI_CAP_BASECLK_SHIFT;
    psdhcicap->SDHCICAP_uiMaxBlkSize   = (uiCapReg & SDHCI_CAP_MAXBLK_MASK) >> SDHCI_CAP_MAXBLK_SHIFT;
    psdhcicap->SDHCICAP_bCanSdma       = (uiCapReg & SDHCI_CAP_CAN_DO_SDMA) ? LW_TRUE : LW_FALSE;
    psdhcicap->SDHCICAP_bCanAdma       = (uiCapReg & SDHCI_CAP_CAN_DO_ADMA) ? LW_TRUE : LW_FALSE;
    psdhcicap->SDHCICAP_bCanHighSpeed  = (uiCapReg & SDHCI_CAP_CAN_DO_HISPD) ? LW_TRUE : LW_FALSE;
    psdhcicap->SDHCICAP_bCanSusRes     = (uiCapReg & SDHCI_CAP_CAN_DO_SUSRES) ? LW_TRUE : LW_FALSE;

    psdhcicap->SDHCICAP_uiBaseClkFreq *= 1000000;                       /*  转为实际频率                */
    psdhcicap->SDHCICAP_uiMaxBlkSize   = 512 << psdhcicap->SDHCICAP_uiMaxBlkSize;
                                                                        /*  转换为实际最大块大小        */
    /*
     * 如果在此寄存器里的时钟为0,则说明主控器没有自己内部时钟,而是来源于其他时钟源.
     * 因此采用另外的时钟源作为基时钟.
     */
    if (psdhcicap->SDHCICAP_uiBaseClkFreq == 0) {
        uiTmp = psdhcihostattr->SDHCIHOST_uiMaxClock;
        if (uiTmp == 0) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "no clock source .\r\n");
            return;
        }
        psdhcicap->SDHCICAP_uiBaseClkFreq = uiTmp;
    }

    uiTmp = 0;
    if ((uiCapReg & SDHCI_CAP_CAN_VDD_330) != 0) {
        uiTmp |= SD_VDD_32_33 | SD_VDD_33_34;
    }
    if ((uiCapReg & SDHCI_CAP_CAN_VDD_300) != 0) {
        uiTmp |= SD_VDD_29_30 | SD_VDD_30_31;
    }
    if ((uiCapReg & SDHCI_CAP_CAN_VDD_180) != 0) {
        uiTmp |= SD_VDD_165_195;
    }

    if (uiTmp == 0) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "no voltage information.\r\n");
        return;
    }
    psdhcicap->SDHCICAP_uiVoltage = uiTmp;

#ifdef  __SYLIXOS_DEBUG
#ifndef printk
#define printk
#endif                                                                  /*  printk                      */
    printk("\nhost capablity >>\n");
    printk("uiCapReg       : %08x\n", uiCapReg);
    printk("base clock     : %u\n", psdhcicap->SDHCICAP_uiBaseClkFreq);
    printk("max block size : %u\n", psdhcicap->SDHCICAP_uiMaxBlkSize);
    printk("can sdma       : %s\n", psdhcicap->SDHCICAP_bCanSdma ? "yes" : "no");
    printk("can adma       : %s\n", psdhcicap->SDHCICAP_bCanAdma ? "yes" : "no");
    printk("can high speed : %s\n", psdhcicap->SDHCICAP_bCanHighSpeed ? "yes" : "no");
    printk("voltage support: %08x\n", psdhcicap->SDHCICAP_uiVoltage);
#endif                                                                  /*  __SYLIXOS_DEBUG             */
}
/*********************************************************************************************************
** 函数名称: __sdhciClockSet
** 功能描述: 时钟频率设置
** 输    入: psdhcihost      SDHCI HOST结构指针
**           uiSetClk        要设置的时钟频率
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciClockSet (__PSDHCI_HOST  psdhcihost, UINT32 uiSetClk)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    UINT32              uiBaseClk      = psdhcihost->SDHCIHS_sdhcicap.SDHCICAP_uiBaseClkFreq;

    UINT16              usDivClk       = 0;
    UINT                uiTimeOut      = 30;                            /*  等待为30ms                  */

    if (uiSetClk > uiBaseClk) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "the clock to set is larger than base clock.\r\n");
        return  (PX_ERROR);
    }

    SDHCI_WRITEW(psdhcihostattr, SDHCI_CLOCK_CONTROL, 0);               /*  禁止时钟模块所有内部功能    */

    /*
     * 计算分频因子
     */
    if (uiSetClk < uiBaseClk) {
        usDivClk = (UINT16)((uiBaseClk + uiSetClk - 1) / uiSetClk);
        if (usDivClk <= 2) {
            usDivClk = 1 << 0;
        } else if (usDivClk <= 4) {
            usDivClk = 1 << 1;
        } else if (usDivClk <= 8) {
            usDivClk = 1 << 2;
        } else if (usDivClk <= 16) {
            usDivClk = 1 << 3;
        } else if (usDivClk <= 32) {
            usDivClk = 1 << 4;
        } else if (usDivClk <= 64) {
            usDivClk = 1 << 5;
        } else if (usDivClk <= 128) {
            usDivClk = 1 << 6;
        } else {
            usDivClk = 1 << 7;
        }
    }

#ifdef __SYLIXOS_DEBUG
#ifndef printk
#define printk
#endif                                                                  /*  printk                      */
        printk("current clock: %uHz\n", uiBaseClk / (usDivClk << 1));
#endif                                                                  /*  __SYLIXOS_DEBUG             */

    usDivClk <<= SDHCI_CLKCTL_DIVIDER_SHIFT;
    usDivClk  |= SDHCI_CLKCTL_INTER_EN;                                 /*  内部时钟使能                */

    SDHCI_WRITEW(psdhcihostattr, SDHCI_CLOCK_CONTROL, usDivClk);

    /*
     * 等待时钟稳定
     */
    while (1) {
        usDivClk = SDHCI_READW(psdhcihostattr, SDHCI_CLOCK_CONTROL);
        if (usDivClk & SDHCI_CLKCTL_INTER_STABLE) {
            break;
        }

        uiTimeOut--;
        if (uiTimeOut == 0) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "wait internal clock to be stable timeout.\r\n");
            return  (PX_ERROR);
        }
        SDHCI_DELAYMS(1);
    }

    usDivClk |= SDHCI_CLKCTL_CLOCK_EN;                                  /*  SDCLK 设备时钟使能          */
    SDHCI_WRITEW(psdhcihostattr, SDHCI_CLOCK_CONTROL, usDivClk);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdhciBusWidthSet
** 功能描述: 主控总线位宽设置
** 输    入: psdhcihost      SDHCI HOST结构指针
**           uiBusWidth      要设置的总线位宽
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciBusWidthSet (__PSDHCI_HOST  psdhcihost, UINT32 uiBusWidth)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    UINT8               ucCtl;
    UINT16              usIntStaEn;

    usIntStaEn  = SDHCI_READW(psdhcihostattr, SDHCI_INTSTA_ENABLE);
    usIntStaEn &= ~SDHCI_INTSTAEN_CARD_INT;
    SDHCI_WRITEW(psdhcihostattr, SDHCI_INTSTA_ENABLE, usIntStaEn);      /*  禁止卡中断                  */

    ucCtl  = SDHCI_READB(psdhcihostattr, SDHCI_HOST_CONTROL);

    switch (uiBusWidth) {
    
    case SDARG_SETBUSWIDTH_1:
        ucCtl &= ~SDHCI_HCTRL_4BITBUS;
        break;

    case SDARG_SETBUSWIDTH_4:
        ucCtl |= SDHCI_HCTRL_4BITBUS;
        break;

    default:
        _DebugHandle(__ERRORMESSAGE_LEVEL, "parameter of bus width error.\r\n");
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    SDHCI_WRITEB(psdhcihostattr, SDHCI_HOST_CONTROL, ucCtl);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdhciPowerOn
** 功能描述: 打开电源
** 输    入: psdhcihost     SDHCI HOST结构指针
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciPowerOn (__PSDHCI_HOST  psdhcihost)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    UINT8               ucPow;

    ucPow  = SDHCI_READB(psdhcihostattr, SDHCI_POWER_CONTROL);
    ucPow |= SDHCI_POWCTL_ON;
    SDHCI_WRITEB(psdhcihostattr, SDHCI_POWER_CONTROL, ucPow);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdhciPowerff
** 功能描述: 关闭电源
** 输    入: psdhcihost       SDHCI HOST结构指针
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciPowerOff (__PSDHCI_HOST  psdhcihost)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    UINT8               ucPow;

    ucPow  = SDHCI_READB(psdhcihostattr, SDHCI_POWER_CONTROL);
    ucPow &= ~SDHCI_POWCTL_ON;
    SDHCI_WRITEB(psdhcihostattr, SDHCI_POWER_CONTROL, ucPow);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdhciPowerSetVol
** 功能描述: 电源设置电压
** 输    入: psdhcihost      SDHCI HOST结构指针
**           ucVol           电压
**           bIsOn           设置之后电源是否开启
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciPowerSetVol (__PSDHCI_HOST  psdhcihost,
                               UINT8          ucVol,
                               BOOL           bIsOn)
{
    if (bIsOn) {
        ucVol |= SDHCI_POWCTL_ON;
    } else {
        ucVol &= ~SDHCI_POWCTL_ON;
    }

    __sdhciPowerOff(psdhcihost);

    SDHCI_WRITEB(&psdhcihost->SDHCIHS_hostattr, SDHCI_POWER_CONTROL, ucVol);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdhciSendCmd
** 功能描述: 命令发送
** 输    入: psdhcihost            SDHCI HOST结构指针
**           psdcmd                命令结构指针
**           psddat                数据传输控制结构
** 输    出: psdcmd->SDCMD_uiResp  如果有应答,这个域存放应答结果
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT  __sdhciSendCmd (__PSDHCI_HOST   psdhcihost,
                            PLW_SD_COMMAND  psdcmd,
                            PLW_SD_DATA     psddat)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    UINT32              uiRespFlag;
    UINT32              uiMask;
    UINT                uiTimeOut;
    INT                 iCmdFlg;

    struct timespec     tvOld;
    struct timespec     tvNow;

    uiMask = SDHCI_PSTA_CMD_INHIBIT;

    if (psddat || SD_CMD_TEST_RSP(psdcmd, SD_RSP_BUSY)) {
        uiMask |= SDHCI_PSTA_DATA_INHIBIT;
    }

    /*
     * 等待命令(数据)线空闲
     */
    uiTimeOut = 0;
    lib_clock_gettime(CLOCK_MONOTONIC, &tvOld);
    while (SDHCI_READL(psdhcihostattr, SDHCI_PRESENT_STATE) & uiMask) {
        uiTimeOut++;
        if (uiTimeOut > __SDHCI_CMD_RETRY) {
            goto __cmd_free_timeout;
        }

        lib_clock_gettime(CLOCK_MONOTONIC, &tvNow);
        if ((tvNow.tv_sec - tvOld.tv_sec) >= __SDHCI_CMD_TOUT_SEC) {
            goto __cmd_free_timeout;
        }
    }

    /*
     * 写参数寄存器
     */
    SDHCI_WRITEL(psdhcihostattr, SDHCI_ARGUMENT, psdcmd->SDCMD_uiArg);

    /*
     * 设置传输模式
     */
    __sdhciTransferModSet(psdhcihost, psddat);

    /*
     * 命令发送
     */
    iCmdFlg = __sdhciCmdRespType(psdcmd);                               /*  应答类型                    */
    if (SD_CMD_TEST_RSP(psdcmd, SD_RSP_CRC)) {
        iCmdFlg |= SDHCI_CMD_CRC_CHK;
    }
    if (SD_CMD_TEST_RSP(psdcmd, SD_RSP_OPCODE)) {
        iCmdFlg |= SDHCI_CMD_INDEX_CHK;
    }
    if (psddat) {
        iCmdFlg |= SDHCI_CMD_DATA;                                      /*  命令类型                    */
    }

    SDHCI_WRITEW(psdhcihostattr, SDHCI_COMMAND, SDHCI_MAKE_CMD(psdcmd->SDCMD_uiOpcode, iCmdFlg));
                                                                        /*  写入命令寄存器              */
    /*
     * 等待命令完成
     */
    uiTimeOut = 0;
    lib_clock_gettime(CLOCK_MONOTONIC, &tvOld);
    while (1) {
        uiMask = SDHCI_READL(psdhcihostattr, SDHCI_INT_STATUS);         /*  读中断状态                  */
        if (uiMask & SDHCI_INT_CMD_MASK) {
            SDHCI_WRITEL(psdhcihostattr,
                                SDHCI_INT_STATUS,
                                uiMask);                                /*  写1 清除相应中断标志位      */
            break;
        }

        uiTimeOut++;
        if (uiTimeOut > __SDHCI_CMD_RETRY) {
            goto __cmd_end_timeout;
        }

        lib_clock_gettime(CLOCK_MONOTONIC, &tvNow);
        if ((tvNow.tv_sec - tvOld.tv_sec) >= __SDHCI_CMD_TOUT_SEC) {
            goto __cmd_end_timeout;
        }
    }

    if (uiMask & SDHCI_INT_TIMEOUT) {
        goto __cmd_end_timeout;
    }

    if (uiMask & SDHCI_INT_CRC) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "crc error.\r\n");
        return  (PX_ERROR);
    }

    if (uiMask & SDHCI_INT_END_BIT) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "end bit error.\r\n");
        return  (PX_ERROR);
    }

    if (uiMask & SDHCI_INT_INDEX) {                                     /*  命令检查错误                */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "command check error.\r\n");
        return  (PX_ERROR);
    }

    if (!(uiMask & SDHCI_INT_RESPONSE)) {                               /*  没有得到应答                */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "response error.\r\n");
        return  (PX_ERROR);
    }

    /*
     * 命令接收正确,应答处理
     */
    uiRespFlag = SD_RESP_TYPE(psdcmd);
    if (uiRespFlag & SD_RSP_PRESENT) {
        UINT32  *puiResp = psdcmd->SDCMD_uiResp;

        /*
         * 长应答,因为应答寄存器中去除了CRC这一个字节,因此作移位处理.
         * 并且其应答字节序与上层规定的相反,作相应的转换.
         */
        if (uiRespFlag & SD_RSP_136) {

            UINT32   uiRsp0;
            UINT32   uiRsp1;
            UINT32   uiRsp2;
            UINT32   uiRsp3;

            uiRsp3     = SDHCI_READL(psdhcihostattr, SDHCI_RESPONSE3);
            uiRsp2     = SDHCI_READL(psdhcihostattr, SDHCI_RESPONSE2);
            uiRsp1     = SDHCI_READL(psdhcihostattr, SDHCI_RESPONSE1);
            uiRsp0     = SDHCI_READL(psdhcihostattr, SDHCI_RESPONSE0);

            puiResp[0] = (uiRsp3 << 8) | ( uiRsp2 >> 24);
            puiResp[1] = (uiRsp2 << 8) | ( uiRsp1 >> 24);
            puiResp[2] = (uiRsp1 << 8) | ( uiRsp0 >> 24);
            puiResp[3] = (uiRsp0 << 8);
        } else {
            puiResp[0] = SDHCI_READL(psdhcihostattr, SDHCI_RESPONSE0);
        }
    }

    return  (ERROR_NONE);

__cmd_free_timeout:
    _DebugHandle(__ERRORMESSAGE_LEVEL, "wait command\\data line timeout.\r\n");
    return  (PX_ERROR);

__cmd_end_timeout:
    _DebugHandle(__ERRORMESSAGE_LEVEL, "wait command complete timeout.\r\n");

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __sdhciTransferModSet
** 功能描述: 设置传输模式
** 输    入: psdhcihost      SDHCI HOST结构指针
**           psddat          数据传输控制机构
** 输    出: NONE
** 返    回: NONE
*********************************************************************************************************/
static VOID __sdhciTransferModSet (__PSDHCI_HOST  psdhcihost, PLW_SD_DATA psddat)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    UINT16              usTmod;

    if (!psddat) {
        return;
    }

    usTmod = SDHCI_TRNS_BLK_CNT_EN;                                     /*  使能块计数                  */

    if (psddat->SDDAT_uiBlkNum > 1) {
        usTmod |= SDHCI_TRNS_MULTI | SDHCI_TRNS_ACMD12;                 /*  始终使用ACMD12功能          */
    }

    if (SD_DAT_IS_READ(psddat)) {
        usTmod |= SDHCI_TRNS_READ;
    }

    if (psdhcihost->SDHCIHS_ucTransferMod != SDHCIHOST_TMOD_SET_NORMAL) {
        usTmod |= SDHCI_TRNS_DMA;
    }

    SDHCI_WRITEW(psdhcihostattr, SDHCI_TRANSFER_MODE, usTmod);
}
/*********************************************************************************************************
** 函数名称: __sdhciDataPrepareNorm
** 功能描述: 一般数据传输准备
** 输    入: psdhcihost      SDHCI HOST结构指针
**           psddat          数据传输控制机构
** 输    出: NONE
** 返    回: NONE
*********************************************************************************************************/
static VOID __sdhciDataPrepareNorm (__PSDHCI_HOST  psdhcihost, PLW_SD_DATA psddat)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;

    if (!psddat) {
        return;
    }

    __sdhciTransferIntSet(psdhcihost);                                  /*  传输中断使能                */

    SDHCI_WRITEW(psdhcihostattr,
                 SDHCI_BLOCK_SIZE,
                 SDHCI_MAKE_BLKSZ(__SDHCI_DMA_BOUND_NBITS,
                                  psddat->SDDAT_uiBlkSize));            /*  块大小寄存器                */
    SDHCI_WRITEW(psdhcihostattr,
                 SDHCI_BLOCK_COUNT,
                 psddat->SDDAT_uiBlkNum);                               /*  块计数寄存器                */
}
/*********************************************************************************************************
** 函数名称: __sdhciDataPrepareSdma
** 功能描述: DMA数据传输准备
** 输    入: psdhcihost      SDHCI HOST结构指针
**           psdmsg          传输控制消息结构指针
** 输    出: NONE
** 返    回: NONE
*********************************************************************************************************/
static VOID __sdhciDataPrepareSdma (__PSDHCI_HOST  psdhcihost, PLW_SD_MESSAGE psdmsg)
{
    PLW_SD_DATA  psddata = psdmsg->SDMSG_psdData;
    UINT8       *pucSdma;

    if (!psddata) {
        return;
    }

#if LW_CFG_VMM_EN
    pucSdma = psdhcihost->SDHCIHS_pucDmaBuf;
    if (SD_DAT_IS_WRITE(psddata)) {                                     /*  如果是DMA写,先把用户数据拷贝*/
        lib_memcpy(pucSdma,
                   psdmsg->SDMSG_pucWrtBuffer,
                   psddata->SDDAT_uiBlkNum * psddata->SDDAT_uiBlkSize);
    }
#else
    if (SD_DAT_IS_READ(psddata)) {
        pucSdma = psdmsg->SDMSG_pucRdBuffer;
    } else {
        pucSdma = psdmsg->SDMSG_pucWrtBuffer;
    }
#endif
    
    /*
     * TODO: STREAM MODE
     */
    __sdhciSdmaAddrUpdate(psdhcihost, (LONG)pucSdma);                   /*  写入DMA系统地址寄存器       */
                                                                        /*  TODO: 地址对齐  ?           */
    __sdhciDataPrepareNorm(psdhcihost, psddata);
}
/*********************************************************************************************************
** 函数名称: __sdhciDataPrepareNorm
** 功能描述: 高效DMA数据传输准备
** 输    入: psdhcihost      SDHCI HOST结构指针
**           psddat          数据传输控制机构
** 输    出: NONE
** 返    回: NONE
*********************************************************************************************************/
static VOID __sdhciDataPrepareAdma (__PSDHCI_HOST  psdhcihost, PLW_SD_DATA psddat)
{
}
/*********************************************************************************************************
** 函数名称: __sdhciDataReadNorm
** 功能描述: 一般数据传输(读数据)
** 输    入: psdhcihost      SDHCI HOST结构指针
**           uiBlkSize       块大小
**           uiBlkNum        块数量
** 输    出: pucRdBuff       结果缓冲
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciDataReadNorm (__PSDHCI_HOST psdhcihost,
                                UINT32        uiBlkSize,
                                UINT32        uiBlkNum,
                                PUCHAR        pucRdBuff)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    UINT32              uiRdTmp;
    UINT32              uiBlkSizeTmp = uiBlkSize;
    UINT32              uiIntSta;
    INT                 iRetry = 0;

    struct timespec   tvOld;
    struct timespec   tvNow;

    while (uiBlkNum > 0) {
        iRetry = 0;
        lib_clock_gettime(CLOCK_MONOTONIC, &tvOld);
        while (!(SDHCI_READL(psdhcihostattr, SDHCI_PRESENT_STATE) & SDHCI_PSTA_DATA_AVAILABLE)) {
            iRetry++;
            if (iRetry > __SDHCI_READ_RETRY) {
                goto __read_ready_timeout;
            }

            lib_clock_gettime(CLOCK_MONOTONIC, &tvNow);
            if ((tvNow.tv_sec - tvOld.tv_sec) > __SDHCI_READ_TOUT_SEC) {
                goto __read_ready_timeout;
            }
        }

        /*
         * 数据读中断是以块为单位的,因此,一次读取一个块.
         * 注意,这里的块大小应该是以4字节对称.注意在上层处理.
         */
        uiBlkSize = uiBlkSizeTmp;
        while (uiBlkSize > 0) {
            uiRdTmp      = SDHCI_READL(psdhcihostattr, SDHCI_BUFFER);
            *pucRdBuff++ =  uiRdTmp & 0xff;
            *pucRdBuff++ = (uiRdTmp >> 8) & 0xff;
            *pucRdBuff++ = (uiRdTmp >> 16) & 0xff;
            *pucRdBuff++ = (uiRdTmp >> 24) & 0xff;
            uiBlkSize   -= 4;
        }
        uiBlkNum--;
    }

    /*
     * 对于多块或单块传输,结束后需要查看数据完成中断标志.
     * 对于无限制的传输,当数据处理完成后,需要发送中止命令.
     * 对于当前版本,没有使用无限制传输.
     */
    iRetry = 0;
    lib_clock_gettime(CLOCK_MONOTONIC, &tvOld);
    while (!((uiIntSta = SDHCI_READL(psdhcihostattr, SDHCI_INT_STATUS)) &
              SDHCI_INTSTA_DATA_END)) {
        iRetry++;
        if (iRetry >= __SDHCI_READ_RETRY) {
            goto __read_end_timeout;
        }

        lib_clock_gettime(CLOCK_MONOTONIC, &tvNow);
        if ((tvNow.tv_sec - tvOld.tv_sec) > __SDHCI_READ_TOUT_SEC) {
            goto __read_end_timeout;
        }
    }
    SDHCI_WRITEL(psdhcihostattr, SDHCI_INT_STATUS, uiIntSta);           /*  清除状态                    */

    return  (ERROR_NONE);

__read_ready_timeout:
    _DebugHandle(__ERRORMESSAGE_LEVEL, "wait block ready timeout.\r\n");
    return  (PX_ERROR);

__read_end_timeout:
    _DebugHandle(__ERRORMESSAGE_LEVEL, "wait complete timeout.\r\n");
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __sdhciDataWriteNorm
** 功能描述: 一般数据传输(写数据)
** 输    入: psdhcihost      SDHCI HOST结构指针
**           uiBlkSize       块大小
**           uiBlkNum        块数量
**           pucWrtBuf       写缓冲
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciDataWriteNorm (__PSDHCI_HOST psdhcihost,
                                 UINT32        uiBlkSize,
                                 UINT32        uiBlkNum,
                                 PUCHAR        pucWrtBuff)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    UINT32              uiBlkSizeTmp   = uiBlkSize;
    UINT32              uiWrtTmp;
    INT                 iRetry;
    UINT32              uiIntSta;

    struct timespec     tvOld;
    struct timespec     tvNow;

    while (uiBlkNum > 0) {
        /*
         * 等待写缓冲有效
         */
        iRetry = 0;
        lib_clock_gettime(CLOCK_MONOTONIC, &tvOld);
        while (!((uiIntSta = SDHCI_READL(psdhcihostattr, SDHCI_PRESENT_STATE)) &
                SDHCI_PSTA_SPACE_AVAILABLE)) {
            iRetry++;
            if (iRetry >= __SDHCI_WRITE_RETRY) {
                goto __write_ready_timeout;
            }

            lib_clock_gettime(CLOCK_MONOTONIC, &tvNow);
            if ((tvNow.tv_sec - tvOld.tv_sec) > __SDHCI_WRITE_TOUT_SEC) {
                goto __write_ready_timeout;
            }
        }
        SDHCI_WRITEL(psdhcihostattr, SDHCI_INT_STATUS, uiIntSta);       /*  清除状态                    */

        /*
         * 数据写中断是以块为单位的,因此,一次写入一个块.
         * 注意,这里的块大小应该是以4字节对称.注意在上层处理.
         */
        uiBlkSize = uiBlkSizeTmp;
        while (uiBlkSize > 0) {
            uiWrtTmp   =  *pucWrtBuff++;
            uiWrtTmp  |= (*pucWrtBuff++) << 8;
            uiWrtTmp  |= (*pucWrtBuff++) << 16;
            uiWrtTmp  |= (*pucWrtBuff++) << 24;
            uiBlkSize -= 4;
            SDHCI_WRITEL(psdhcihostattr, SDHCI_BUFFER, uiWrtTmp);
        }
        uiBlkNum--;
    }

    /*
     * 对于多块或单块传输,结束后需要查看数据完成中断标志.
     * 对于无限制的传输,当数据处理完成后,需要发送中止命令.
     * 对于当前版本,没有使用无限制传输.
     */
    iRetry = 0;
    lib_clock_gettime(CLOCK_MONOTONIC, &tvOld);
    while (!((uiIntSta = SDHCI_READL(psdhcihostattr, SDHCI_INT_STATUS)) &
              SDHCI_INTSTA_DATA_END)) {
        iRetry++;
        if (iRetry >= __SDHCI_WRITE_RETRY) {
            goto __write_end_timeout;
        }

        lib_clock_gettime(CLOCK_MONOTONIC, &tvNow);
        if ((tvNow.tv_sec - tvOld.tv_sec) > __SDHCI_WRITE_TOUT_SEC) {
            goto __write_end_timeout;
        }
    }

    SDHCI_WRITEL(psdhcihostattr, SDHCI_INT_STATUS, uiIntSta);           /*  清除状态                    */

    return  (ERROR_NONE);

__write_ready_timeout:
    _DebugHandle(__ERRORMESSAGE_LEVEL, "wait block ready timeout.\r\n");
    return  (PX_ERROR);

__write_end_timeout:
    _DebugHandle(__ERRORMESSAGE_LEVEL, "wait complete timeout.\r\n");
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __sdhciDataFinishSdma
** 功能描述: SDMA数据传输完成处理
** 输    入: psdhcihost       SDHCI HOST结构指针
**           psdmsg
** 输    出: NONE
** 返    回: ERROR CODE
*********************************************************************************************************/
static INT __sdhciDataFinishSdma (__PSDHCI_HOST   psdhcihost,  PLW_SD_MESSAGE psdmsg)
{
    PLW_SDHCI_HOST_ATTR  psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    PLW_SD_DATA          psddat         = psdmsg->SDMSG_psdData;
    UINT8               *pucSdma;

#if LW_CFG_VMM_EN
    UINT8               *pucDst;
#endif

    INT                  iRetry;
    UINT32               uiIntSta;

    struct timespec      tvOld;
    struct timespec      tvNow;

#if LW_CFG_VMM_EN
    if (SD_DAT_IS_READ(psddat)) {
        pucDst = psdmsg->SDMSG_pucRdBuffer;
    }
    pucSdma = psdhcihost->SDHCIHS_pucDmaBuf;
#else
    if (SD_DAT_IS_READ(psdmsg->SDMSG_psdData)) {
        pucSdma = psdmsg->SDMSG_pucRdBuffer;
    } else {
        pucSdma = psdmsg->SDMSG_pucWrtBuffer;
    }
#endif

    /*
     * 使用SDAM时, 标准主控最大可以一次性搬移连续内存上的512k数据,
     * 超过这个边界, 主控会产生一个DMA中断,通知需要更新DMA缓冲地址.
     * 目前这个版本已经设置为512k边界地址.
     * 所有数据(在块大小和块数量寄存器中标示的)传输完成,产生一个数据完成中断.
     * 数据完成中断的优先级高于DMA中断.
     */
    iRetry = 0;
    lib_clock_gettime(CLOCK_MONOTONIC, &tvOld);
    while (1) {
        uiIntSta = SDHCI_READL(psdhcihostattr, SDHCI_INT_STATUS);
        if (uiIntSta & SDHCI_INTSTA_DATA_END) {                         /*  数据传输结束                */
            SDHCI_WRITEL(psdhcihostattr, SDHCI_INT_STATUS, uiIntSta);   /*  清除状态                    */
            return  (ERROR_NONE);
        }

        if (uiIntSta & SDHCI_INTSTA_DMA) {                              /*  DMA边界中断                 */
            pucSdma += __SDHCI_DMA_BOUND_LEN;                           /*  更新DMA地址                 */
            __sdhciSdmaAddrUpdate(psdhcihost, (LONG)pucSdma);
        }

        iRetry++;
        if (iRetry > __SDHCI_SDMARW_RETRY) {
            goto __sdma_timeout;
        }

        lib_clock_gettime(CLOCK_MONOTONIC, &tvNow);
        if ((tvNow.tv_sec - tvOld.tv_sec) > __SDHCI_WRITE_TOUT_SEC) {
            goto __sdma_timeout;
        }
    }

    /*
     * 如果开启了VMM,则需要把DMA物理区数据拷贝到用户区带cache的数据区
     */
#if LW_CFG_VMM_EN
     if (SD_DAT_IS_READ(psddat)) {
         lib_memcpy(pucDst, pucSdma, psddat->SDDAT_uiBlkNum * psddat->SDDAT_uiBlkSize);
     }
#endif

__sdma_timeout:
    _DebugHandle(__ERRORMESSAGE_LEVEL, "timeout.\r\n");
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __sdhciTransferIntSet
** 功能描述: 数据传输中断设置
** 输    入: psdhcihost       SDHCI HOST结构指针
** 输    出: NONE
** 返    回: NONE
*********************************************************************************************************/
static VOID __sdhciTransferIntSet (__PSDHCI_HOST  psdhcihost)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;

    UINT32              uiIntIoMsk;                                     /*  使用一般传输时的中断掩码    */
    UINT32              uiIntDmaMsk;                                    /*  使用DMA传输时的中断掩码     */
    UINT32              uiEnMask;                                       /*  最终使能掩码                */

    uiIntIoMsk  = SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL;
    uiIntDmaMsk = SDHCI_INT_DMA_END     | SDHCI_INT_ADMA_ERROR;
    uiEnMask    = SDHCI_READL(psdhcihostattr, SDHCI_INTSTA_ENABLE);     /*  读取32位(包括错误中断)      */

    if (psdhcihost->SDHCIHS_ucTransferMod != SDHCIHOST_TMOD_SET_NORMAL) {
        uiEnMask &= ~uiIntIoMsk;
        uiEnMask |=  uiIntDmaMsk;
    } else {
        uiEnMask &= ~uiIntDmaMsk;
        uiEnMask |=  uiIntIoMsk;
    }

    uiEnMask |= SDHCI_INT_RESPONSE | SDHCI_INT_DATA_END;

    /*
     * 因为这两个寄存器的位标位置都完全相同,
     * 所以可以同时用一个掩码.
     */
    SDHCI_WRITEL(psdhcihostattr, SDHCI_INTSTA_ENABLE, uiEnMask);
    SDHCI_WRITEL(psdhcihostattr, SDHCI_SIGNAL_ENABLE, uiEnMask);
}
/*********************************************************************************************************
** 函数名称: __sdhciIntDisAndEn
** 功能描述: 中断设置(使能\禁能).就是设置中断状态(错误\一般状态)和中断信号(错误\一般信号)寄存器.
** 输    入: psdhcihost      SDHCI HOST结构指针
**           uiDisMask       禁能掩码
**           uiEnMask        使能掩码
** 输    出: NONE
** 返    回: NONE
*********************************************************************************************************/
static VOID __sdhciIntDisAndEn (__PSDHCI_HOST  psdhcihost,
                                UINT32         uiDisMask,
                                UINT32         uiEnMask)
{
    PLW_SDHCI_HOST_ATTR psdhcihostattr = &psdhcihost->SDHCIHS_hostattr;
    UINT32              uiMask;

    uiMask  =  SDHCI_READW(psdhcihostattr, SDHCI_INTSTA_ENABLE);
    uiMask &= ~uiDisMask;
    uiMask |=  uiEnMask;

    /*
     * 因为这两个寄存器的位标位置都完全相同,
     * 所以可以同时用一个掩码.
     */
    SDHCI_WRITEW(psdhcihostattr, SDHCI_INTSTA_ENABLE, (UINT16)uiMask);
    SDHCI_WRITEW(psdhcihostattr, SDHCI_SIGNAL_ENABLE, (UINT16)uiMask);
}
/*********************************************************************************************************
** 函数名称: __sdhciPreStaShow
** 功能描述: 显示当前所有状态
** 输    入: lBasePoint
** 输    出: NONE
** 返    回: NONE
*********************************************************************************************************/
#ifdef  __SYLIXOS_DEBUG

static VOID __sdhciPreStaShow(PLW_SDHCI_HOST_ATTR psdhcihostattr)
{
#ifndef printk
#define printk
#endif                                                                  /*  printk                      */

    UINT32  uiSta;

    uiSta = SDHCI_READL(psdhcihostattr, SDHCI_PRESENT_STATE);
    printk("\nhost present status >>\n");
    printk("cmd line(cmd) : %s\n", uiSta & SDHCI_PSTA_CMD_INHIBIT ? "busy" : "free");
    printk("cmd line(dat) : %s\n", uiSta & SDHCI_PSTA_DATA_INHIBIT ? "busy" : "free");
    printk("dat line      : %s\n", uiSta & SDHCI_PSTA_DATA_ACTIVE ? "busy" : "free");
    printk("write active  : %s\n", uiSta & SDHCI_PSTA_DOING_WRITE ? "active" : "inactive");
    printk("read active   : %s\n", uiSta & SDHCI_PSTA_DOING_READ ? "active" : "inactive");
    printk("write buffer  : %s\n", uiSta & SDHCI_PSTA_SPACE_AVAILABLE ? "ready" : "not ready");
    printk("read  buffer  : %s\n", uiSta & SDHCI_PSTA_DATA_AVAILABLE ? "ready" : "not ready");
    printk("card insert   : %s\n", uiSta & SDHCI_PSTA_CARD_PRESENT ? "insert" : "not insert");
}
/*********************************************************************************************************
** 函数名称: __sdhciIntStaShow
** 功能描述: 显示中断状态
** 输    入: lBasePoint
** 输    出: NONE
** 返    回: NONE
*********************************************************************************************************/
static VOID __sdhciIntStaShow(PLW_SDHCI_HOST_ATTR psdhcihostattr)
{
#ifndef printk
#define printk
#endif                                                                  /*  printk                      */

    UINT32  uiSta;

    uiSta = SDHCI_READL(psdhcihostattr, SDHCI_INT_STATUS);
    printk("\nhost int status >>\n");
    printk("cmd finish  : %s\n", uiSta & SDHCI_INT_RESPONSE ? "yes" : "no");
    printk("dat finish  : %s\n", uiSta & SDHCI_INT_DATA_END ? "yes" : "no");
    printk("dma finish  : %s\n", uiSta & SDHCI_INT_DMA_END ? "yes" : "no");
    printk("space avail : %s\n", uiSta & SDHCI_INT_SPACE_AVAIL ? "yes" : "no");
    printk("data avail  : %s\n", uiSta & SDHCI_INT_DATA_AVAIL ? "yes" : "no");
    printk("card insert : %s\n", uiSta & SDHCI_INT_CARD_INSERT ? "insert" : "not insert");
    printk("card remove : %s\n", uiSta & SDHCI_INT_CARD_REMOVE ? "remove" : "not remove");
}

#endif                                                                  /*  __SYLIXOS_DEBUG             */
#endif                                                                  /*  (LW_CFG_DEVICE_EN > 0)      */
                                                                        /*  (LW_CFG_SDCARD_EN > 0)      */
/*********************************************************************************************************
  END
*********************************************************************************************************/
