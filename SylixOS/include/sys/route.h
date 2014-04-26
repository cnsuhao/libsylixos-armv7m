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
** 文   件   名: route.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 01 月 15 日
**
** 描        述: SylixOS 简易路由接口.
*********************************************************************************************************/

#ifndef __SYS_ROUTE_H
#define __SYS_ROUTE_H

#include "netinet/in.h"
#include "netinet/in6.h"
#include "sys/types.h"
#include "net/if.h"

/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_NET_EN > 0

/*********************************************************************************************************
  route_add/change type
*********************************************************************************************************/

#define ROUTE_TYPE_HOST             0                                   /*  destination is a host       */
#define ROUTE_TYPE_NET              1                                   /*  destination is a net        */
#define ROUTE_TYPE_GATEWAY          2                                   /*  destination is a gateway    */
#define ROUTE_TYPE_DEFAULT          3                                   /*  default gateway             */

/*********************************************************************************************************
  route_get flag (U H G flag)
*********************************************************************************************************/

#define ROUTE_RTF_UP                0x01                                /* route useable                */
#define ROUTE_RTF_GATEWAY           0x02                                /* destination is a gateway     */
#define ROUTE_RTF_HOST              0x04                                /* host entry (net otherwise)   */

/*********************************************************************************************************
  route_get msgbuf
*********************************************************************************************************/

struct route_msg {
    u_char          rm_flag;                                            /*  类型                        */
    int             rm_metric;                                          /*  暂时未使用                  */
    struct in_addr  rm_dst;                                             /*  目的地址信息                */
    struct in_addr  rm_gw;                                              /*  网关信息                    */
    struct in_addr  rm_mask;                                            /*  子网掩码信息                */
    struct in_addr  rm_if;                                              /*  网络接口地址                */
    char            rm_ifname[IF_NAMESIZE];                             /*  网络接口名                  */
};

/*********************************************************************************************************
  api (only for ipv4)
*********************************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif                                                                  /*  __cplusplus                 */

LW_API int  route_add(struct in_addr *pinaddr, int  type, const char *ifname);
LW_API int  route_delete(struct in_addr *pinaddr);
LW_API int  route_change(struct in_addr *pinaddr, int  type, const char *ifname);
LW_API int  route_getnum(void);                                         /*  获得路由表总数量            */
LW_API int  route_get(u_char flag, struct route_msg  *msgbuf, size_t  num);

#ifdef __cplusplus
}
#endif                                                                  /*  __cplusplus                 */

#endif                                                                  /*  LW_CFG_NET_EN > 0           */
#endif                                                                  /*  __SYS_ROUTE_H               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
