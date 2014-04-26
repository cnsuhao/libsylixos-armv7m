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
** 文   件   名: lwip_pppfd.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2014 年 01 月 08 日
**
** 描        述: lwip ppp 连接管理器.
*********************************************************************************************************/

#ifndef __LWIP_PPPFD_H
#define __LWIP_PPPFD_H

/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#define LW_CFG_NET_EN   1
#define LW_CFG_LWIP_PPP 1

#if (LW_CFG_NET_EN > 0) && (LW_CFG_LWIP_PPP > 0)

#include "sys/ioctl.h"

/*********************************************************************************************************
  ioctl 命令
*********************************************************************************************************/

struct pppfd_dial {
    char   *user;
    size_t  userlen;
    char   *passwd;
    size_t  passwdlen;
    int     redial_delay;
    int     redial_max;
};

#define PPPFD_CMD_ERRCODE       _IOR('p', 1, int)
#define PPPFD_CMD_PHASE         _IOR('p', 2, int)
#define PPPFD_CMD_DIAL          _IOW('p', 3, struct pppfd_dial)
#define PPPFD_CMD_HUP           _IO( 'p', 4)

/*********************************************************************************************************
  api
*********************************************************************************************************/

LW_API INT  API_PppfdDrvInstall(VOID);

#define pppfdDrv    API_PppfdDrvInstall

LW_API int  pppfd_os_create(const char *serial, char *pppif, size_t bufsize);
LW_API int  pppfd_oe_create(const char *ethif, const char *service_name, const char *concentrator_name,
                            char       *pppif, size_t      bufsize);
LW_API int  pppfd_ol2tp_create(const char *ethif,
                               const char *ipaddr, short   port,
                               const char *secret, size_t  secret_len,
                               char       *pppif,  size_t  bufsize);

#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_LWIP_PPP > 0         */
#endif                                                                  /*  __LWIP_PPPFD_H              */
/*********************************************************************************************************
  END
*********************************************************************************************************/
