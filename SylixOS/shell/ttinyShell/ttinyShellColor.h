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
** 文   件   名: ttinyShellColor.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2014 年 07 月 08 日
**
** 描        述: tty 颜色系统.
*********************************************************************************************************/

#ifndef __TTINYSHELLCOLOR_H
#define __TTINYSHELLCOLOR_H

/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_SHELL_EN > 0

LW_API  VOID  API_TShellColorRefresh(VOID);
LW_API  VOID  API_TShellColorStart(CPCHAR  pcName, CPCHAR  pcLink, mode_t  mode, INT  iFd);
LW_API  VOID  API_TShellColorEnd(INT  iFd);

#define tshellColorRefresh  API_TShellColorRefresh
#define tshellColorStart    API_TShellColorStart
#define tshellColorEnd      API_TShellColorEnd

#ifdef __SYLIXOS_KERNEL
VOID          __tshellColorInit(VOID);
#endif                                                                  /*  __SYLIXOS_KERNEL            */

#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
#endif                                                                  /*  __TTINYSHELLCOLOR_H         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
