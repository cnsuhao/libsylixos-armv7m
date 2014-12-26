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
** 文   件   名: sddrvm.h
**
** 创   建   人: Zeng.Bo (曾波)
**
** 文件创建日期: 2014 年 10 月 24 日
**
** 描        述: sd drv manager layer

** BUG:
2014.11.07  增加孤儿设备管理,这允许先插入设备,再注册驱动后,设备能够正确创建.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_SDCARD_EN > 0)
#include "sddrvm.h"
#include "sdutil.h"
#include "../include/sddebug.h"
#if LW_CFG_SDCARD_SDIO_EN > 0
#include "sdiostd.h"
#include "sdiodrvm.h"
#include "sdiocoreLib.h"
#endif                                                                  /*  LW_CFG_SDCARD_SDIO_EN > 0   */
/*********************************************************************************************************
  宏定义
*********************************************************************************************************/
#define __SDM_DEVNAME_MAX               32
#define __SDM_DEBUG_EN                  0
/*********************************************************************************************************
  仅 SDM_SDIO 模块可见的事件
*********************************************************************************************************/
#define SDM_EVENT_NEW_DRV               3
/*********************************************************************************************************
  SDM 内部使用的数据结构
*********************************************************************************************************/
struct __sdm_sd_dev;
struct __sdm_host;
struct __sdm_host_drv_funcs;
struct __sdm_host_chan;

typedef struct __sdm_sd_dev            __SDM_SD_DEV;
typedef struct __sdm_host              __SDM_HOST;
typedef struct __sdm_host_drv_funcs    __SDM_HOST_DRV_FUNCS;
typedef struct __sdm_host_chan         __SDM_HOST_CHAN;

struct __sdm_sd_dev {
    SD_DRV           *SDMDEV_psddrv;
    VOID             *SDMDEV_pvDevPriv;
    CHAR              SDMDEV_pcDevName[__SDM_DEVNAME_MAX];
    INT               SDMDEV_iUnit;
    PLW_SDCORE_DEVICE SDMDEV_psdcoredev;
};

struct __sdm_host_chan {
    __SDM_HOST_DRV_FUNCS    *SDMHOSTCHAAN_pdrvfuncs;
};

struct __sdm_host_drv_funcs {
    INT     (*SDMHOSTDRV_pfuncCallbackInstall)
            (
             __SDM_HOST_CHAN  *psdmhostchan,
             INT               iCallbackType,
             SD_CALLBACK       callback,
             PVOID             pvCallbackArg
            );
    VOID    (*SDMHOSTDRV_pfuncSpicsEn)(__SDM_HOST_CHAN *psdmhostchan);
    VOID    (*SDMHOSTDRV_pfuncSpicsDis)(__SDM_HOST_CHAN *psdmhostchan);
};

struct __sdm_host {
    LW_LIST_LINE     SDMHOST_lineManage;
    __SDM_HOST_CHAN  SDMHOST_sdmhostchan;
    SD_HOST         *SDMHOST_psdhost;
    __SDM_SD_DEV    *SDMHOST_psdmdevAttached;
    BOOL             SDMHOST_bDevIsOrphan;
};
/*********************************************************************************************************
  前置声明
*********************************************************************************************************/
static __SDM_HOST *__sdmHostFind(CPCHAR cpcName);
static VOID        __sdmHostInsert(__SDM_HOST *psdmhost);
static VOID        __sdmHostDelete(__SDM_HOST *psdmhost);
static VOID        __sdmDrvInsert(SD_DRV *psddrv);
static VOID        __sdmDrvDelete(SD_DRV *psddrv);

static VOID        __sdmDevCreate(__SDM_HOST *psdmhost);
static VOID        __sdmDevDelete(__SDM_HOST *psdmhost);
static VOID        __sdmSdioIntHandle(__SDM_HOST *psdmhost);

static INT         __sdmCallbackInstall(__SDM_HOST_CHAN  *psdmhostchan,
                                        INT               iCallbackType,
                                        SD_CALLBACK       callback,
                                        PVOID             pvCallbackArg);
static INT         __sdmCallbackUnInstall(SD_HOST *psdhost);

static VOID        __sdmSpiCsEn(__SDM_HOST_CHAN *psdmhostchan);
static VOID        __sdmSpiCsDis(__SDM_HOST_CHAN *psdmhostchan);
/*********************************************************************************************************
  SDM 内部调试相关
  使用独立的线程处理事件, 方便跟踪调试
*********************************************************************************************************/
#if __SDM_DEBUG_EN > 0
#define __SDM_DEBUG_THREAD_PRIO     200
#define __SDM_DEBUG_THREAD_STKSZ    (8 * 1024)

typedef struct {
    CPCHAR        EVTMSG_cpcHostName;
    INT           EVTMSG_iEvtType;
} __SDM_EVT_MSG;

static LW_OBJECT_HANDLE             _G_hsdmevtHandle = LW_OBJECT_HANDLE_INVALID;
static LW_OBJECT_HANDLE             _G_hsdmevtMsgQ   = LW_OBJECT_HANDLE_INVALID;

static INT    __sdmDebugLibInit(VOID);
static VOID   __sdmDebugEvtNotify(CPCHAR cpcHostName, INT iEvtType);
static PVOID  __sdmDebugEvtHandle(VOID *pvArg);
#endif                                                                  /*   __SDM_DEBUG_EN > 0         */
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
static LW_SPINLOCK_DEFINE       (_G_slSdmHostLock);

static LW_LIST_LINE_HEADER       _G_plineSddrvHeader   = LW_NULL;
static LW_LIST_LINE_HEADER       _G_plineSdmhostHeader = LW_NULL;

#define __SDM_HOST_LOCK()        LW_SPIN_LOCK_IRQ(&_G_slSdmHostLock, &iregInterLevel)
#define __SDM_HOST_UNLOCK()      LW_SPIN_UNLOCK_IRQ(&_G_slSdmHostLock, iregInterLevel)

static __SDM_HOST_DRV_FUNCS      _G_sdmhostdrvfuncs;
/*********************************************************************************************************
** 函数名称: API_SdmLibInit
** 功能描述: SDM 组件库初始化
** 输    入: NONE
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API INT   API_SdmLibInit (VOID)
{
    _G_sdmhostdrvfuncs.SDMHOSTDRV_pfuncCallbackInstall = __sdmCallbackInstall;
    _G_sdmhostdrvfuncs.SDMHOSTDRV_pfuncSpicsDis        = __sdmSpiCsDis;
    _G_sdmhostdrvfuncs.SDMHOSTDRV_pfuncSpicsEn         = __sdmSpiCsEn;

    API_SdLibInit();

    LW_SPIN_INIT(&_G_slSdmHostLock);

#if __SDM_DEBUG_EN > 0
    __sdmDebugLibInit();
#endif                                                                  /*   __SDM_DEBUG_EN > 0         */

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdmHostRegister
** 功能描述: 向 SDM 注册一个 HOST 信息
** 输    入: pHost        Host 信息, 注意 SDM 内部会直接引用该对象, 因此该对象需要持续有效
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API INT   API_SdmHostRegister (SD_HOST *psdhost)
{
    __SDM_HOST *psdmhost;

    if (!psdhost                              ||
        !psdhost->SDHOST_cpcName              ||
        !psdhost->SDHOST_pfuncCallbackInstall ||
        !psdhost->SDHOST_pfuncCallbackUnInstall) {
        SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "host must provide: name, callback install"
                                               " method and callback uninstall method.\r\n");
        return  (PX_ERROR);
    }

    if (psdhost->SDHOST_iType == SDHOST_TYPE_SPI) {
        if (!psdhost->SDHOST_pfuncSpicsDis ||
            !psdhost->SDHOST_pfuncSpicsEn) {
            SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "spi type host must provide:"
                                                   " spi chip select method.\r\n");
            return  (PX_ERROR);
        }
    }

    /*
     * SDM内部对HOST的名称很敏感, 不允许存在相同的名称
     */
    psdmhost = __sdmHostFind(psdhost->SDHOST_cpcName);
    if (psdmhost) {
        SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "there is already a same "
                                               "name host registered.\r\n");
        return  (PX_ERROR);
    }

    psdmhost = (__SDM_HOST *)__SHEAP_ALLOC(sizeof(__SDM_HOST));
    if (!psdmhost) {
        SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "system memory low.\r\n");
        return  (PX_ERROR);
    }

    psdmhost->SDMHOST_psdhost         = psdhost;
    psdmhost->SDMHOST_psdmdevAttached = LW_NULL;
    psdmhost->SDMHOST_bDevIsOrphan    = LW_FALSE;

    psdmhost->SDMHOST_sdmhostchan.SDMHOSTCHAAN_pdrvfuncs = &_G_sdmhostdrvfuncs;

    __sdmHostInsert(psdmhost);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdmHostUnRegister
** 功能描述: 从 SDM 注销一个 HOST 信息
** 输    入: cpcHostName      主控的名称
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API INT   API_SdmHostUnRegister (CPCHAR cpcHostName)
{
    __SDM_HOST *psdmhost;

    if (!cpcHostName) {
        return  (PX_ERROR);
    }

    psdmhost = __sdmHostFind(cpcHostName);
    if (!psdmhost) {
        return  (PX_ERROR);
    }

    if (psdmhost->SDMHOST_psdmdevAttached) {
        SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "there is always a device attached.\r\n");
        return  (PX_ERROR);
    }

    if (psdmhost->SDMHOST_bDevIsOrphan) {
        SDCARD_DEBUG_MSG(__LOGMESSAGE_LEVEL, "warning: there is always a "
                                             "orphan device attached.\r\n");
    }

    __sdmHostDelete(psdmhost);

    __SHEAP_FREE(psdmhost);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdmHostCapGet
** 功能描述: 根据核心传输对象获得对应控制器的功能信息
** 输    入: psdcoredev       核心设备传输对象
**           piCapbility      保存返回的控制器功能
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API INT  API_SdmHostCapGet (PLW_SDCORE_DEVICE psdcoredev, INT *piCapbility)
{
    CPCHAR      cpcHostName;
    __SDM_HOST *psdmhost;

    cpcHostName = API_SdCoreDevAdapterName(psdcoredev);
    if (!cpcHostName) {
        return  (PX_ERROR);
    }

    psdmhost = __sdmHostFind(cpcHostName);
    if (!psdmhost) {
        return  (PX_ERROR);
    }

    *piCapbility = psdmhost->SDMHOST_psdhost->SDHOST_iCapbility;

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdmHostInterEn
** 功能描述: 使能控制器的 SDIO 中断
** 输    入: psdcoredev       核心设备传输对象
**           bEnable          是否使能
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API VOID  API_SdmHostInterEn (PLW_SDCORE_DEVICE psdcoredev, BOOL bEnable)
{
    CPCHAR      cpcHostName;
    __SDM_HOST *psdmhost;
    SD_HOST    *psdhost;

    cpcHostName = API_SdCoreDevAdapterName(psdcoredev);
    if (!cpcHostName) {
        return;
    }

    psdmhost = __sdmHostFind(cpcHostName);
    if (!psdmhost) {
        return;
    }

    psdhost = psdmhost->SDMHOST_psdhost;
    if (psdhost->SDHOST_pfuncSdioIntEn) {
        psdhost->SDHOST_pfuncSdioIntEn(psdhost, bEnable);
    }
}
/*********************************************************************************************************
** 函数名称: API_SdmSdDrvRegister
** 功能描述: 向SDM注册一个设备应用驱动
** 输    入: psddrv       sd 驱动. 注意 SDM 内部会直接引用该对象, 因此该对象需要持续有效
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API INT   API_SdmSdDrvRegister (SD_DRV *psddrv)
{
    if (!psddrv) {
        return  (PX_ERROR);
    }

    psddrv->SDDRV_pvSpec = __sdUnitPoolCreate();
    if (!psddrv->SDDRV_pvSpec) {
        SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "system memory low.\r\n");
        return  (PX_ERROR);
    }

    API_AtomicSet(0, &psddrv->SDDRV_atomicDevCnt);
    __sdmDrvInsert(psddrv);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdmSdDrvUnRegister
** 功能描述: 从 SDM 注销一个设备应用驱动
** 输    入: psddrv       sd 驱动
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API INT   API_SdmSdDrvUnRegister (SD_DRV *psddrv)
{
    if (!psddrv) {
        return  (PX_ERROR);
    }

    if (API_AtomicGet(&psddrv->SDDRV_atomicDevCnt)) {
        SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "exist device using this drv.\r\n");
        return  (PX_ERROR);
    }

    __sdUnitPoolDelete(psddrv->SDDRV_pvSpec);
    __sdmDrvDelete(psddrv);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SdmEventNotify
** 功能描述: 向 SDM 通知事件
** 输    入: cpcHostName      控制器名称
**           iEvtType         事件类型
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_API VOID  API_SdmEventNotify (CPCHAR cpcHostName, INT iEvtType)
{
    INTREG             iregInterLevel;
    PLW_LIST_LINE      plineTemp;
    __SDM_HOST        *psdmhost = LW_NULL;

#if __SDM_DEBUG_EN > 0
    __sdmDebugEvtNotify(cpcHostName, iEvtType);
    return;
#endif                                                                  /*   __SDM_DEBUG_EN > 0         */

    /*
     * 当注册一个新的驱动后, 遍历所有 Host 上的孤儿设备
     */
    if ((iEvtType == SDM_EVENT_NEW_DRV) && !cpcHostName) {
        __SDM_HOST_LOCK();
        for (plineTemp  = _G_plineSdmhostHeader;
             plineTemp != LW_NULL;
             plineTemp  = _list_line_get_next(plineTemp)) {

            psdmhost = _LIST_ENTRY(plineTemp, __SDM_HOST, SDMHOST_lineManage);
            if (!psdmhost->SDMHOST_psdmdevAttached &&
                 psdmhost->SDMHOST_bDevIsOrphan) {
                hotplugEvent((VOIDFUNCPTR)__sdmDevCreate, (PVOID)psdmhost, 0, 0, 0, 0, 0);
            }
        }
        __SDM_HOST_UNLOCK();

        return;
    }

    /*
     * 处理Host传来的事件
     */
    psdmhost = __sdmHostFind(cpcHostName);
    if (!psdmhost) {
        return;
    }

    switch (iEvtType) {
    
    case SDM_EVENT_DEV_INSERT:
        hotplugEvent((VOIDFUNCPTR)__sdmDevCreate, (PVOID)psdmhost, 0, 0, 0, 0, 0);
        break;

    case SDM_EVENT_DEV_REMOVE:
        hotplugEvent((VOIDFUNCPTR)__sdmDevDelete, (PVOID)psdmhost, 0, 0, 0, 0, 0);
        break;

    case SDM_EVENT_SDIO_INTERRUPT:
        __sdmSdioIntHandle(psdmhost);
        break;

    default:
        break;
    }
}
/*********************************************************************************************************
** 函数名称: __sdmDevCreate
** 功能描述: SDM 设备创建方法
** 输    入: psdmhost     SDM 管理的控制器对象
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID __sdmDevCreate (__SDM_HOST *psdmhost)
{
    SD_HOST           *psdhost = psdmhost->SDMHOST_psdhost;
    SD_DRV            *psddrv;
    __SDM_SD_DEV      *psdmdev;
    PLW_SDCORE_DEVICE  psdcoredev;
    PLW_LIST_LINE      plineTemp;
    INT                iRet;

    if (psdmhost->SDMHOST_psdmdevAttached) {
        return;
    }

    psdmdev = (__SDM_SD_DEV *)__SHEAP_ALLOC(sizeof(__SDM_SD_DEV));
    if (!psdmdev) {
        SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        return;
    }

    for (plineTemp  = _G_plineSddrvHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {

        psddrv = _LIST_ENTRY(plineTemp, SD_DRV, SDDRV_lineManage);

        iRet = __sdUnitGet(psddrv->SDDRV_pvSpec);
        if (iRet < 0) {
            goto    __err0;
        }

        psdmdev->SDMDEV_iUnit = iRet;
        snprintf(psdmdev->SDMDEV_pcDevName, __SDM_DEVNAME_MAX, "%s%d", psddrv->SDDRV_cpcName, iRet);

        psdcoredev = API_SdCoreDevCreate(psdhost->SDHOST_iType,
                                         psdhost->SDHOST_cpcName,
                                         psdmdev->SDMDEV_pcDevName,
                                         (PLW_SDCORE_CHAN)&psdmhost->SDMHOST_sdmhostchan);
        if (!psdcoredev) {
            goto    __err1;
        }

        /*
         * 因为在接下来的具体驱动设备创建过程中可能会产生HOST事件通知
         * 这里提前完成处理中断时需要的数据
         */
        psdmdev->SDMDEV_psddrv            = psddrv;
        psdmdev->SDMDEV_psdcoredev        = psdcoredev;
        psdmhost->SDMHOST_psdmdevAttached = psdmdev;
        psdmhost->SDMHOST_bDevIsOrphan    = LW_FALSE;

        iRet = psddrv->SDDRV_pfuncDevCreate(psddrv, psdcoredev, &psdmdev->SDMDEV_pvDevPriv);
        if (iRet != ERROR_NONE) {
            /*
             * 在删除 core dev 之前一定要先卸载安装的回调函数
             * 避免设备对象释放后, host 端还有可能去调用安装的回调函数
             */
            __sdmCallbackUnInstall(psdmhost->SDMHOST_psdhost);
            API_SdCoreDevDelete(psdcoredev);
            __sdUnitPut(psddrv->SDDRV_pvSpec, psdmdev->SDMDEV_iUnit);

        } else {
            if (psdhost->SDHOST_pfuncDevAttach) {
                psdhost->SDHOST_pfuncDevAttach(psdmhost->SDMHOST_psdhost, psdmdev->SDMDEV_pcDevName);
            }
            API_AtomicInc(&psddrv->SDDRV_atomicDevCnt);
            return;
        }
    }

__err1:
    if (psddrv) {
        __sdUnitPut(psddrv->SDDRV_pvSpec, psdmdev->SDMDEV_iUnit);
    }

__err0:
    __SHEAP_FREE(psdmdev);

    /*
     * 表示设备已经插入, 但是没有具体的驱动成功创建设备对象
     */
    psdmhost->SDMHOST_psdmdevAttached = LW_NULL;
    psdmhost->SDMHOST_bDevIsOrphan    = LW_TRUE;
}
/*********************************************************************************************************
** 函数名称: __sdmDevDelete
** 功能描述: SDM 设备删除方法
** 输    入: psdmhost     SDM 管理的控制器对象
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID __sdmDevDelete (__SDM_HOST *psdmhost)
{
    SD_DRV          *psddrv;
    __SDM_SD_DEV    *psdmdev;

    psdmdev = psdmhost->SDMHOST_psdmdevAttached;
    if (!psdmdev) {
        psdmhost->SDMHOST_bDevIsOrphan = LW_FALSE;
        return;
    }

    psddrv = psdmdev->SDMDEV_psddrv;
    psddrv->SDDRV_pfuncDevDelete(psddrv, psdmdev->SDMDEV_pvDevPriv);

    /*
     * 可能删除的是 SDIO 设备, 并且该设备之前开启了 SDIO 中断.
     * 为了不影响新插入的设备, 这里主动关闭
     */
    if (psdmhost->SDMHOST_psdhost->SDHOST_pfuncSdioIntEn) {
        psdmhost->SDMHOST_psdhost->SDHOST_pfuncSdioIntEn(psdmhost->SDMHOST_psdhost, LW_FALSE);
    }

    __sdmCallbackUnInstall(psdmhost->SDMHOST_psdhost);
    API_SdCoreDevDelete(psdmdev->SDMDEV_psdcoredev);

    __sdUnitPut(psddrv->SDDRV_pvSpec, psdmdev->SDMDEV_iUnit);
    __SHEAP_FREE(psdmdev);

    psdmhost->SDMHOST_psdmdevAttached = LW_NULL;
    psdmhost->SDMHOST_bDevIsOrphan    = LW_FALSE;

    if (psdmhost->SDMHOST_psdhost->SDHOST_pfuncDevDetach) {
        psdmhost->SDMHOST_psdhost->SDHOST_pfuncDevDetach(psdmhost->SDMHOST_psdhost);
    }

    API_AtomicDec(&psddrv->SDDRV_atomicDevCnt);
}
/*********************************************************************************************************
** 函数名称: __sdmSdioIntHandle
** 功能描述: SDM SDIO中断处理
** 输    入: psdmhost     SDM 管理的控制器对象
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID __sdmSdioIntHandle (__SDM_HOST *psdmhost)
{
#if LW_CFG_SDCARD_SDIO_EN > 0
    __SDM_SD_DEV    *psdmdev;
    UINT8            ucIntFlag;
    INT              iRet;

    INT  __sdiobaseDevIrqHandle(SD_DRV *psddrv,  VOID *pvDevPriv);

    psdmdev = psdmhost->SDMHOST_psdmdevAttached;
    if (!psdmdev) {
        return;
    }

    /*
     *  Host 控制器可能传来假的 SDIO 中断
     *  因此查看 SDIO 设备是否真的产生了 SDIO 中断
     */
    iRet = API_SdioCoreDevReadByte(psdmdev->SDMDEV_psdcoredev,
                                   SDIO_CCCR_CCCR,
                                   SDIO_CCCR_INTX,
                                   &ucIntFlag);
    if (iRet != ERROR_NONE) {
        return;
    }

    if (ucIntFlag) {
        __sdiobaseDevIrqHandle(psdmdev->SDMDEV_psddrv, psdmdev->SDMDEV_pvDevPriv);
    }
#endif                                                                  /*  LW_CFG_SDCARD_SDIO_EN > 0   */
}
/*********************************************************************************************************
** 函数名称: __sdmHostFind
** 功能描述: 查找 SDM 管理的控制器
** 输    入: cpcName      控制器名称
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static __SDM_HOST *__sdmHostFind (CPCHAR cpcName)
{
    REGISTER PLW_LIST_LINE      plineTemp;
    REGISTER __SDM_HOST        *psdmhost = LW_NULL;
    INTREG                      iregInterLevel;

    if (cpcName == LW_NULL) {
        return  (LW_NULL);
    }

    __SDM_HOST_LOCK();
    for (plineTemp  = _G_plineSdmhostHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {

        psdmhost = _LIST_ENTRY(plineTemp, __SDM_HOST, SDMHOST_lineManage);
        if (lib_strcmp(cpcName, psdmhost->SDMHOST_psdhost->SDHOST_cpcName) == 0) {
            break;
        }
    }
    __SDM_HOST_UNLOCK();

    if (plineTemp) {
        return  (psdmhost);
    } else {
        return  (LW_NULL);
    }
}
/*********************************************************************************************************
** 函数名称: __sdmHostInsert
** 功能描述: 往 SDM 管理的控制器链表插入一个控制器对象
** 输    入: psdmhost     SDM 管理的控制器对象
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID  __sdmHostInsert (__SDM_HOST *psdmhost)
{
    INTREG   iregInterLevel;

    __SDM_HOST_LOCK();
    _List_Line_Add_Ahead(&psdmhost->SDMHOST_lineManage, &_G_plineSdmhostHeader);
    __SDM_HOST_UNLOCK();
}
/*********************************************************************************************************
** 函数名称: __sdmHostDelete
** 功能描述: 从 SDM 管理的控制器链表删除一个控制器对象
** 输    入: psdmhost     SDM 管理的控制器对象
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID  __sdmHostDelete (__SDM_HOST *psdmhost)
{
    INTREG   iregInterLevel;

    __SDM_HOST_LOCK();
    _List_Line_Del(&psdmhost->SDMHOST_lineManage, &_G_plineSdmhostHeader);
    __SDM_HOST_UNLOCK();
}
/*********************************************************************************************************
** 函数名称: __sdmDrvInsert
** 功能描述: 往 SDM 管理的 SD 驱动链表插入一个驱动对象
** 输    入: psddrv     SDM 管理的驱动对象
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID  __sdmDrvInsert (SD_DRV *psddrv)
{
    _List_Line_Add_Ahead(&psddrv->SDDRV_lineManage, &_G_plineSddrvHeader);
}
/*********************************************************************************************************
** 函数名称: __sdmDrvDelete
** 功能描述: 从 SDM 管理的 SD 驱动链表删除一个控制器对象
** 输    入: psddrv     SDM 管理的驱动对象
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID  __sdmDrvDelete (SD_DRV *psddrv)
{
    _List_Line_Del(&psddrv->SDDRV_lineManage, &_G_plineSddrvHeader);
}
/*********************************************************************************************************
** 函数名称: __sdmCallbackInstall
** 功能描述: SDM 内部设备回调函数安装 方法.
** 输    入: psdmhostchan     SDM 内部控制器通道描述对象
**           iCallbackType    回调函数类型
**           callback         安装的回调函数
**           pvCallbackArg    回调函数的参数
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __sdmCallbackInstall (__SDM_HOST_CHAN  *psdmhostchan,
                                 INT               iCallbackType,
                                 SD_CALLBACK       callback,
                                 PVOID             pvCallbackArg)
{
    __SDM_HOST *psdmhost;
    SD_HOST    *psdhost;

    INT         iRet = PX_ERROR;

    psdmhost = __CONTAINER_OF(psdmhostchan, __SDM_HOST, SDMHOST_sdmhostchan);
    psdhost  = psdmhost->SDMHOST_psdhost;

    if (psdhost->SDHOST_pfuncCallbackInstall) {
        iRet = psdhost->SDHOST_pfuncCallbackInstall(psdhost, iCallbackType, callback, pvCallbackArg);
    }

    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: __sdmCallbackUnInstall
** 功能描述: SDM 内部从指定的主控上卸载安装的回调函数 方法
** 输    入: psdhost      控制器注册的信息结构对象
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT  __sdmCallbackUnInstall (SD_HOST *psdhost)
{
    if (psdhost->SDHOST_pfuncCallbackUnInstall) {
        psdhost->SDHOST_pfuncCallbackUnInstall(psdhost, SDHOST_CALLBACK_CHECK_DEV);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __sdmSpiCsEn
** 功能描述: SDM 内部使用的 SPI 控制器片选函数
** 输    入: psdmhostchan     控制器通道
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID __sdmSpiCsEn (__SDM_HOST_CHAN *psdmhostchan)
{
    __SDM_HOST *psdmhost;
    SD_HOST    *psdhost;

    psdmhost = __CONTAINER_OF(psdmhostchan, __SDM_HOST, SDMHOST_sdmhostchan);
    psdhost  = psdmhost->SDMHOST_psdhost;

    if (psdhost->SDHOST_pfuncSpicsEn) {
        psdhost->SDHOST_pfuncSpicsEn(psdhost);
    }
}
/*********************************************************************************************************
** 函数名称: __sdmSpiCsDis
** 功能描述: SDM 内部使用的 SPI 控制器片选函数
** 输    入: psdmhostchan     控制器通道
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID __sdmSpiCsDis (__SDM_HOST_CHAN *psdmhostchan)
{
    __SDM_HOST *psdmhost;
    SD_HOST    *psdhost;

    psdmhost = __CONTAINER_OF(psdmhostchan, __SDM_HOST, SDMHOST_sdmhostchan);
    psdhost  = psdmhost->SDMHOST_psdhost;

    if (psdhost->SDHOST_pfuncSpicsDis) {
        psdhost->SDHOST_pfuncSpicsDis(psdhost);
    }
}
/*********************************************************************************************************
** 函数名称: __sdmDebugLibInit
** 功能描述: SDM 内部调试模块初始化
** 输    入: NONE
** 输    出: ERROR CODE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
#if __SDM_DEBUG_EN > 0
static INT  __sdmDebugLibInit (VOID)
{
    LW_CLASS_THREADATTR  threadAttr;

    _G_hsdmevtMsgQ = API_MsgQueueCreate("sdm_dbgmsgq",
                                        12,
                                        sizeof(__SDM_EVT_MSG),
                                        LW_OPTION_WAIT_FIFO,
                                        LW_NULL);
    if (_G_hsdmevtMsgQ == LW_OBJECT_HANDLE_INVALID) {
        SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "create message queue failed.\r\n");
        return  (PX_ERROR);
    }

    threadAttr = API_ThreadAttrGetDefault();
    threadAttr.THREADATTR_pvArg            = LW_NULL;
    threadAttr.THREADATTR_ucPriority       = __SDM_DEBUG_THREAD_PRIO;
    threadAttr.THREADATTR_stStackByteSize  = __SDM_DEBUG_THREAD_STKSZ;
    _G_hsdmevtHandle = API_ThreadCreate("t_sdmdbgevth",
                                        __sdmDebugEvtHandle,
                                        &threadAttr,
                                        LW_NULL);
    if (_G_hsdmevtHandle == LW_OBJECT_HANDLE_INVALID) {
        SDCARD_DEBUG_MSG(__ERRORMESSAGE_LEVEL, "create message queue failed.\r\n");
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
#endif                                                                  /*   __SDM_DEBUG_EN > 0         */
/*********************************************************************************************************
** 函数名称: __sdmDebugEvtHandle
** 功能描述: SDM 内部事件处理线程函数
** 输    入: pvArg     线程参数
** 输    出: 线程返回值
** 全局变量:
** 调用模块:
*********************************************************************************************************/
#if __SDM_DEBUG_EN > 0
static PVOID  __sdmDebugEvtHandle (VOID *pvArg)
{
    INTREG             iregInterLevel;
    PLW_LIST_LINE      plineTemp;
    __SDM_HOST        *psdmhost;

    CPCHAR             cpcHostName;
    INT                iEvtType;

    __SDM_EVT_MSG      sdmevtmsg;
    size_t             stLen;
    INT                iRet;

    while (1) {
        iRet = API_MsgQueueReceive(_G_hsdmevtMsgQ,
                                   (PVOID)&sdmevtmsg,
                                   sizeof(__SDM_EVT_MSG),
                                   &stLen,
                                   LW_OPTION_WAIT_INFINITE);
        if (iRet != ERROR_NONE) {
            printk(KERN_EMERG"__sdmDebugEvtHandle() error.\r\n");
            continue;
        }

        iEvtType    = sdmevtmsg.EVTMSG_iEvtType;
        cpcHostName = sdmevtmsg.EVTMSG_cpcHostName;

        if ((iEvtType == SDM_EVENT_NEW_DRV) && !cpcHostName) {
            __SDM_HOST_LOCK();
            for (plineTemp  = _G_plineSdmhostHeader;
                 plineTemp != LW_NULL;
                 plineTemp  = _list_line_get_next(plineTemp)) {

                psdmhost = _LIST_ENTRY(plineTemp, __SDM_HOST, SDMHOST_lineManage);
                if (!psdmhost->SDMHOST_psdmdevAttached &&
                     psdmhost->SDMHOST_bDevIsOrphan) {
                    __sdmDevCreate(psdmhost);
                }
            }
            __SDM_HOST_UNLOCK();
            continue;
        }

        psdmhost = __sdmHostFind(cpcHostName);
        if (!psdmhost) {
            continue;
        }

        switch (iEvtType) {
        
        case SDM_EVENT_DEV_INSERT:
            __sdmDevCreate(psdmhost);
            break;

        case SDM_EVENT_DEV_REMOVE:
            __sdmDevDelete(psdmhost);
            break;

        case SDM_EVENT_SDIO_INTERRUPT:
            __sdmSdioIntHandle(psdmhost);
            break;

        default:
            break;
        }
    }

    return  (LW_NULL);
}
#endif                                                                  /*   __SDM_DEBUG_EN > 0         */
/*********************************************************************************************************
** 函数名称: __sdmDebugEvtHandle
** 功能描述: SDM 内部事件通知
** 输    入: pvArg     线程参数
** 输    出: NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
#if __SDM_DEBUG_EN > 0
static VOID  __sdmDebugEvtNotify (CPCHAR cpcHostName, INT iEvtType)
{
    __SDM_EVT_MSG   sdmevtmsg;

    sdmevtmsg.EVTMSG_iEvtType    = iEvtType;
    sdmevtmsg.EVTMSG_cpcHostName = cpcHostName;
    API_MsgQueueSend(_G_hsdmevtMsgQ,
                     &sdmevtmsg,
                     sizeof(__SDM_EVT_MSG));
}
#endif                                                                  /*   __SDM_DEBUG_EN > 0         */
#endif                                                                  /*  (LW_CFG_DEVICE_EN > 0)      */
                                                                        /*  (LW_CFG_SDCARD_EN > 0)      */
/*********************************************************************************************************
  END
*********************************************************************************************************/
