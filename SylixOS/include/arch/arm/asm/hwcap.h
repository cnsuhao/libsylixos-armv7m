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
** 文   件   名: hwcap.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 05 月 04 日
**
** 描        述: ARM 硬件特性标记.
*********************************************************************************************************/

#ifndef __ASMARM_HWCAP_H
#define __ASMARM_HWCAP_H

#define HWCAP_SWP               1
#define HWCAP_HALF              2
#define HWCAP_THUMB             4
#define HWCAP_26BIT             8
#define HWCAP_FAST_MULT         16
#define HWCAP_FPA               32
#define HWCAP_VFP               64
#define HWCAP_EDSP              128
#define HWCAP_JAVA              256
#define HWCAP_IWMMXT            512
#define HWCAP_CRUNCH            1024
#define HWCAP_THUMBEE           2048
#define HWCAP_NEON              4096
#define HWCAP_VFPv3             8192
#define HWCAP_VFPv3D16          16384
#define HWCAP_TLS               32768

#endif                                                                  /*  __ASMARM_HWCAP_H            */
/*********************************************************************************************************
  END
*********************************************************************************************************/
