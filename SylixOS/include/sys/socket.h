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
** 文   件   名: socket.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 05 月 10 日
**
** 描        述: socket 通信.
*********************************************************************************************************/

#ifndef __SYS_SOCKET_H
#define __SYS_SOCKET_H

#define AF_PUP              4         /* pup protocols: e.g. BSP */
#define AF_CHAOS            5         /* mit CHAOS protocols */
#define AF_NS               6         /* XEROX NS protocols */
#define AF_ISO              7         /* ISO protocols */
#define AF_OSI              AF_ISO
#define AF_ECMA             8         /* european computer manufacturers */
#define AF_DATAKIT          9         /* datakit protocols */
#define AF_CCITT            10        /* CCITT protocols, X.25 etc */
#define AF_SNA              11        /* IBM SNA */
#define AF_DECnet           12        /* DECnet */
#define AF_DLI              13        /* DEC Direct data link interface */
#define AF_LAT              14        /* LAT */
#define AF_HYLINK           15        /* NSC Hyperchannel */
#define AF_APPLETALK        16        /* Apple Talk */
#define AF_ROUTE            17        /* Internal Routing Protocol */
#define AF_LINK             18        /* Link layer interface */
#define pseudo_AF_XTP       19        /* eXpress Transfer Protocol (no AF) */
#define AF_COIP             20        /* connection-oriented IP, aka ST II */
#define AF_CNT              21        /* Computer Network Technology */
#define pseudo_AF_RTIP      22        /* Help Identify RTIP packets */
#define AF_IPX              23        /* Novell Internet Protocol */
#define AF_SIP              24        /* Simple Internet Protocol */
#define pseudo_AF_PIP       25        /* Help Identify PIP packets */

#define AF_MAX              26

#include "../inet/lwip/sockets.h"

#endif                                                                  /*  __SYS_SOCKET_H              */
/*********************************************************************************************************
  END
*********************************************************************************************************/
