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
** 文   件   名: sdcardLib.h
**
** 创   建   人: Zeng.Bo (曾波)
**
** 文件创建日期: 2014 年 11 月 07 日
**
** 描        述: sdcard 模块包含的头文件

** BUG:
*********************************************************************************************************/

#ifndef __SDCARD_LIB_H
#define __SDCARD_LIB_H

#include "../core/sdcore.h"                                         /*  sd 卡核心设备支持 API           */
#include "../core/sddrvm.h"                                         /*  sdm sd 驱动管理支持 API         */
#include "../core/sdiodrvm.h"                                       /*  sdm sdio 驱动管理支持 API       */
#include "../core/sdcoreLib.h"                                      /*  sd 卡工具库支持 API             */
#include "../core/sdiocoreLib.h"                                    /*  sdio 卡工具库支持 API           */
#include "../client/sdmemory.h"                                     /*  sd 存储卡支持 API               */
#include "../client/sdmemoryDrv.h"                                  /*  sd 存储卡SDM 接口驱动           */
#include "../client/sdiobaseDrv.h"                                  /*  sdio SDM 接口基础驱动           */
#include "../host/sdhci.h"                                          /*  sd 标准控制器类驱动             */

#endif                                                              /*  __SDCARD_LIB_H                  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
