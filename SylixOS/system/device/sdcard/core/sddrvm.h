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
*********************************************************************************************************/

#ifndef __SDDRVM_H
#define __SDDRVM_H

#include "sdcore.h"

/*********************************************************************************************************
  SDM 事件类型 for API_SdmEventNotify()
*********************************************************************************************************/

#define SDM_EVENT_DEV_INSERT            0
#define SDM_EVENT_DEV_REMOVE            1
#define SDM_EVENT_SDIO_INTERRUPT        2

/*********************************************************************************************************
  前置声明
*********************************************************************************************************/
struct sd_drv;
struct sd_host;

typedef struct sd_drv     SD_DRV;
typedef struct sd_host    SD_HOST;

/*********************************************************************************************************
  sd 驱动(用于sd memory 和 sdio base)
*********************************************************************************************************/

struct sd_drv {
    LW_LIST_LINE  SDDRV_lineManage;                               /*  驱动挂载链                        */

    CPCHAR        SDDRV_cpcName;

    INT         (*SDDRV_pfuncDevCreate)(SD_DRV *psddrv, PLW_SDCORE_DEVICE psdcoredev, VOID **ppvDevPriv);
    INT         (*SDDRV_pfuncDevDelete)(SD_DRV *psddrv, VOID *pvDevPriv);

    atomic_t      SDDRV_atomicDevCnt;

    VOID         *SDDRV_pvSpec;
};

/*********************************************************************************************************
  sd host 信息结构
*********************************************************************************************************/

#ifdef  __cplusplus
typedef INT     (*SD_CALLBACK)(...);
#else
typedef INT     (*SD_CALLBACK)();
#endif

struct sd_host {
    CPCHAR        SDHOST_cpcName;

    INT           SDHOST_iType;
#define SDHOST_TYPE_SD                  0
#define SDHOST_TYPE_SPI                 1

    INT           SDHOST_iCapbility;                                /*  主动支持的特性                  */
#define SDHOST_CAP_HIGHSPEED    (1 << 0)                            /*  支持高速传输                    */
#define SDHOST_CAP_DATA_4BIT    (1 << 1)                            /*  支持4位数据传输                 */
#define SDHOST_CAP_DATA_8BIT    (1 << 2)                            /*  支持8位数据传输                 */

    VOID          (*SDHOST_pfuncSpicsEn)(SD_HOST *psdhost);
    VOID          (*SDHOST_pfuncSpicsDis)(SD_HOST *psdhost);
    INT           (*SDHOST_pfuncCallbackInstall)
                  (
                  SD_HOST          *psdhost,
                  INT               iCallbackType,                  /*  安装的回调函数的类型            */
                  SD_CALLBACK       callback,                       /*  回调函数指针                    */
                  PVOID             pvCallbackArg                   /*  回调函数的参数                  */
                  );

    INT           (*SDHOST_pfuncCallbackUnInstall)
                  (
                  SD_HOST          *psdhost,
                  INT               iCallbackType                   /*  安装的回调函数的类型            */
                  );
#define SDHOST_CALLBACK_CHECK_DEV       0                           /*  卡状态检测                      */
#define SDHOST_DEVSTA_UNEXIST           0                           /*  卡状态:不存在                   */
#define SDHOST_DEVSTA_EXIST             1                           /*  卡状态:存在                     */

    VOID          (*SDHOST_pfuncSdioIntEn)(SD_HOST *psdhost, BOOL bEnable);
    BOOL          (*SDHOST_pfuncIsCardWp)(SD_HOST *psdhost);

    VOID          (*SDHOST_pfuncDevAttach)(SD_HOST *psdhost, CPCHAR cpcDevName);
    VOID          (*SDHOST_pfuncDevDetach)(SD_HOST *psdhost);
};

/*********************************************************************************************************
  API
*********************************************************************************************************/

LW_API INT   API_SdmLibInit(VOID);

LW_API INT   API_SdmHostRegister(SD_HOST *psdhost);
LW_API INT   API_SdmHostUnRegister(CPCHAR cpcHostName);

LW_API INT   API_SdmHostCapGet(PLW_SDCORE_DEVICE psdcoredev, INT *piCapbility);
LW_API VOID  API_SdmHostInterEn(PLW_SDCORE_DEVICE psdcoredev, BOOL bEnable);
LW_API BOOL  API_SdmHostIsCardWp(PLW_SDCORE_DEVICE psdcoredev);

LW_API INT   API_SdmSdDrvRegister(SD_DRV *psddrv);
LW_API INT   API_SdmSdDrvUnRegister(SD_DRV *psddrv);

LW_API INT   API_SdmEventNotify(CPCHAR cpcHostName, INT iEvtType);

#endif                                                              /*  __SDDRVM_H                      */
/*********************************************************************************************************
  END
*********************************************************************************************************/
