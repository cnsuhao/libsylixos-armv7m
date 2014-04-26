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
** 文   件   名: lwip_vpnnetif.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2010 年 05 月 25 日
**
** 描        述: SSL VPN 虚拟网络接口.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "../SylixOS/net/include/net_net.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if (LW_CFG_NET_EN > 0) && (LW_CFG_NET_VPN_EN > 0)
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/inet.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "netif/etharp.h"
#include "polarssl/ssl.h"
#include "polarssl/havege.h"
#include "lwip_vpnlib.h"
/*********************************************************************************************************
  VPN 虚拟网络接口数据包格式 (以太网数据分组格式)
  +----------+----------+----------+--------------------------+
  | 目标 MAC |  源 MAC  |   类型   |         数据负载         |
  +----------+----------+----------+--------------------------+
     6 bytes    6 bytes    2 bytes          46 - 1500
*********************************************************************************************************/
/*********************************************************************************************************
  VPN 虚拟网络接口配置
*********************************************************************************************************/
#define VPN_NETIF_MTU           1500                                    /*  VPN 网口的最大数据包长度    */
/*********************************************************************************************************
  VPN 虚拟网络接口网络命令
*********************************************************************************************************/
#define VPN_COMMAND_GETMAC      0x01                                    /*  查询 MAC 地址               */
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
INT     __vpnClientClose(__PVPN_CONTEXT  pvpnctx);
/*********************************************************************************************************
  VPN 全局变量
*********************************************************************************************************/
static const u8_t       _G_ucVpnEthBoradcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
/*********************************************************************************************************
** 函数名称: __vpnNetifOutput
** 功能描述: VPN 虚拟网口发送函数
** 输　入  : netif         VPN 网络接口
**           p             数据包
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static err_t  __vpnNetifOutput (struct netif *netif, struct pbuf *p)
{
    __PVPN_CONTEXT  pvpnctx = (__PVPN_CONTEXT)netif->state;
    u8_t            ucBuffer[VPN_NETIF_MTU + 14];
    u8_t           *pucTemp = ucBuffer;
    INT             iTotalLen;                                          /*  整个数据包大小, 至少 60 字节*/
    INT             iDataLen;                                           /*  数据包数据部分长度          */
    INT             i;
    struct  pbuf   *q;

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE);                                      /*  drop the padding word       */
#endif                                                                  /*  ETH_PAD_SIZE                */

    for(q = p; q != LW_NULL; q = q->next) {
        lib_memcpy(pucTemp, q->payload, q->len);                        /*  copy data                   */
        pucTemp += q->len;
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE);                                       /*  reclaim the padding word    */
#endif                                                                  /*  ETH_PAD_SIZE                */

    iDataLen  = p->tot_len;
#if ETH_PAD_SIZE
    iDataLen -= ETH_PAD_SIZE;
#endif                                                                  /*  ETH_PAD_SIZE                */
    /*
     *  需要补充至少 60 个字节(不包含 CRC 部分)
     */
    if (iDataLen < 60) {
        iTotalLen = 60;
        for (i = 0; i < iTotalLen - iDataLen; i++) {
            *pucTemp++ = 0x00;
        }
    } else {
        iTotalLen = iDataLen;
    }

    if (ssl_write(&pvpnctx->VPNCTX_sslctx, ucBuffer, iTotalLen) == iTotalLen) {
        if (lib_memcmp(ucBuffer, _G_ucVpnEthBoradcast, 6) == 0) {
            snmp_inc_ifoutnucastpkts(netif);
        } else {
            snmp_inc_ifoutucastpkts(netif);
        }
        snmp_add_ifoutoctets(netif, iDataLen);
        LINK_STATS_INC(link.xmit);
        return  (ERR_OK);

    } else {
        snmp_inc_ifoutdiscards(netif);
        LINK_STATS_INC(link.err);
        return  (ERR_IF);
    }
}
/*********************************************************************************************************
** 函数名称: __vpnNetifProc
** 功能描述: VPN 虚拟网口接收线程
** 输　入  : pvArg         参数
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
PVOID  __vpnNetifProc (PVOID  pvArg)
{
    __PVPN_CONTEXT   pvpnctx = (__PVPN_CONTEXT)pvArg;
    struct netif    *netif   = &pvpnctx->VPNCTX_netif;

    struct pbuf     *p;
    struct pbuf     *q;

    u8_t             ucBuffer[VPN_NETIF_MTU + 14];
    PUCHAR           pucTemp;
    INT              iRead;


    if (pvpnctx == LW_NULL) {
        return  (LW_NULL);
    }

    for (;;) {
        iRead = ssl_read(&pvpnctx->VPNCTX_sslctx, ucBuffer, sizeof(ucBuffer));
        if (iRead <= 0) {
            if (errno != ETIMEDOUT) {
                break;                                                  /*  链接断开                    */
            }
        } else {
            if (iRead < 60) {                                           /*  命令数据包                  */
                /*
                 *  命令数据包
                 */
                switch (ucBuffer[0]) {

                case VPN_COMMAND_GETMAC:                                /*  获取 MAC                    */
                    ucBuffer[0] = 0x01;
                    ucBuffer[1] = 0x06;
                    ucBuffer[2] = netif->hwaddr[0];
                    ucBuffer[3] = netif->hwaddr[1];
                    ucBuffer[4] = netif->hwaddr[2];
                    ucBuffer[5] = netif->hwaddr[3];
                    ucBuffer[6] = netif->hwaddr[4];
                    ucBuffer[7] = netif->hwaddr[5];
                    ssl_write(&pvpnctx->VPNCTX_sslctx, ucBuffer, 8);
                    break;

                default:
                    break;
                }
            } else {
                /*
                 *  普通数据包
                 */
#if ETH_PAD_SIZE
                iRead += ETH_PAD_SIZE;                                  /*  allow room for Eth padding  */
#endif                                                                  /*  ETH_PAD_SIZE                */
                p = pbuf_alloc(PBUF_RAW, iRead, PBUF_POOL);
                if (p == LW_NULL) {
                    snmp_inc_ifindiscards(netif);
                    LINK_STATS_INC(link.memerr);
                    LINK_STATS_INC(link.drop);

                } else {
#if ETH_PAD_SIZE
                    pbuf_header(p, -ETH_PAD_SIZE);                      /*  drop the padding word       */
#endif
                    pucTemp = &ucBuffer[0];
                    for(q = p; q != LW_NULL; q = q->next) {
                        lib_memcpy(q->payload, pucTemp, q->len);        /*  read data into pbuf         */
                        pucTemp += q->len;
                    }
#if ETH_PAD_SIZE
                    pbuf_header(p, ETH_PAD_SIZE);                       /*  reclaim the padding word    */
#endif
                    if (lib_memcmp(ucBuffer, _G_ucVpnEthBoradcast, 6) == 0) {
                        snmp_inc_ifinnucastpkts(netif);
                    } else {
                        snmp_inc_ifinucastpkts(netif);
                    }
                    snmp_add_ifinoctets(netif, iRead - ETH_PAD_SIZE);
                    LINK_STATS_INC(link.recv);

                    if (netif->input(p, netif) != ERR_OK) {
                        pbuf_free(p);                                   /*  失败则释放                  */
                    }
                }
            }
        }
    }

    /*
     *  运行到这里需要关闭 VPN SSL 链接, 并卸载网络接口, 释放资源.
     */
    __vpnClientClose(pvpnctx);                                          /*  关闭 ssl client             */

    netifapi_netif_common(&pvpnctx->VPNCTX_netif,
                          netif_remove, LW_NULL);                       /*  卸载虚拟网口                */

    __SHEAP_FREE(pvpnctx);                                              /*  释放 VPN 上下文             */

    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __vpnNetifInit
** 功能描述: 初始化 VPN 虚拟网口
** 输　入  : pvpnctx       VPN 上下文
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT __vpnNetifInit (__PVPN_CONTEXT  pvpnctx, UINT8  *pucMac)
{
    struct netif *netif = &pvpnctx->VPNCTX_netif;

    if (pvpnctx == LW_NULL) {
        return  (PX_ERROR);
    }

    netif->state = (void *)pvpnctx;

    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    netif->hwaddr[0]  = pucMac[0];
    netif->hwaddr[1]  = pucMac[1];
    netif->hwaddr[2]  = pucMac[2];
    netif->hwaddr[3]  = pucMac[3];
    netif->hwaddr[4]  = pucMac[4];
    netif->hwaddr[5]  = pucMac[5];

    netif->mtu   = VPN_NETIF_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
                                                                        /*  注意, 真正的 flag 初始化必须*/
                                                                        /*  在 netif_add() 函数回调中   */
                                                                        /*  这里为了程序更清晰才加入的  */
    netif->name[0] = __VPN_IFNAME0;
    netif->name[1] = __VPN_IFNAME1;

    netif->hostname = "sylixos_vpn";

    netif->output     = etharp_output;
    netif->linkoutput = __vpnNetifOutput;

    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_NET_VPN_EN > 0       */
/*********************************************************************************************************
  END
*********************************************************************************************************/
