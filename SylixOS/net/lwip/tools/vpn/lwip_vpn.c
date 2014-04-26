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
** 文   件   名: lwip_vpn.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2010 年 05 月 25 日
**
** 描        述: SSL VPN 应用接口.
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
#include "lwip/inet.h"
#include "lwip/tcpip.h"
#include "socket.h"
#include "polarssl/ssl.h"
#include "polarssl/havege.h"
#include "lwip_vpnlib.h"
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
INT     __vpnClientOpen(__PVPN_CONTEXT  pvpnctx,
                        CPCHAR          cpcCACrtFile,
                        CPCHAR          cpcPrivateCrtFile,
                        CPCHAR          cpcKeyFile,
                        CPCHAR          cpcKeyPassword,
                        struct in_addr  inaddr,
                        u16_t           usPort,
                        INT             iSSLTimeoutSec);
INT     __vpnClientClose(__PVPN_CONTEXT  pvpnctx);
INT     __vpnNetifInit(__PVPN_CONTEXT  pvpnctx, UINT8  *pucMac);
PVOID   __vpnNetifProc(PVOID  pvArg);
/*********************************************************************************************************
  shell 声明
*********************************************************************************************************/
#if LW_CFG_SHELL_EN > 0
INT     __tshellVpnOpen(INT  iArgC, PCHAR  *ppcArgV);
INT     __tshellVpnClose(INT  iArgC, PCHAR  *ppcArgV);
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
/*********************************************************************************************************
** 函数名称: __vpnNetifDummyInit
** 功能描述: VPN 虚拟网口空初始化
** 输　入  : netif         网络接口
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static err_t  __vpnNetifDummyInit (struct netif  *netif)
{
    /*
     *  VPN 网口使用虚拟的以太网数据包格式, 所以必须抽象为以太网接口
     */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    return  (ERR_OK);
}
/*********************************************************************************************************
** 函数名称: API_INetVpnInit
** 功能描述: 初始化 VPN 服务
** 输　入  : pcPath        本地目录
** 输　出  : ERROR
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
VOID  API_INetVpnInit (VOID)
{
#if LW_CFG_SHELL_EN > 0
    API_TShellKeywordAdd("vpnopen", __tshellVpnOpen);
    API_TShellFormatAdd("vpnopen",  " [configration file]");
    API_TShellHelpAdd("vpnopen",    "create a VPN net interface.\n");

    API_TShellKeywordAdd("vpnclose", __tshellVpnClose);
    API_TShellFormatAdd("vpnclose",  " [netifname]");
    API_TShellHelpAdd("vpnclose",    "delete a VPN net interface.\n");
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
}
/*********************************************************************************************************
** 函数名称: API_INetVpnClientCreate
** 功能描述: 创建一个 VPN 客户端连接到指定的服务器, 并生成虚拟网卡
** 输　入  : cpcCACrtFile                  CA 证书文件     .pem or .crt
**           cpcPrivateCrtFile             私有证书文件    .pem or .crt
**           cpcKeyFile                    私有密钥文件    .pem or .key
**           cpcKeyPassword                私有密钥文件解密密码, 如果密钥文件不存在密码, 则为 NULL
**           cpcServerIp                   服务器 ip 地址,       例如: "123.234.123.234"
**           cpcServerClientIp             VPN 虚拟网卡地址      例如: "192.168.0.12"
**           cpcServerClientMask           VPN 虚拟网卡掩码      例如: "255.255.255.0"
**           cpcServerClientGw             VPN 虚拟网卡网关      例如: "192.168.0.1"
**           usPort                        VPN 服务器端口 (网络字节序)
**           iSSLTimeoutSec                SSL 通信超时时间
**           iVerifyOpt                    SSL 认证选项
**           pucMac                        6 个字节的虚拟网卡 MAC 地址.
** 输　出  : ERROR
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
INT  API_INetVpnClientCreate (CPCHAR          cpcCACrtFile,
                              CPCHAR          cpcPrivateCrtFile,
                              CPCHAR          cpcKeyFile,
                              CPCHAR          cpcKeyPassword,
                              CPCHAR          cpcServerIp,
                              CPCHAR          cpcServerClientIp,
                              CPCHAR          cpcServerClientMask,
                              CPCHAR          cpcServerClientGw,
                              UINT16          usPort,
                              INT             iSSLTimeoutSec,
                              INT             iVerifyOpt,
                              UINT8          *pucMac)
{
    __PVPN_CONTEXT          pvpnctx;
    INT                     iError;
    struct in_addr          inaddrServer;

    ip_addr_t               ipaddrIp;
    ip_addr_t               ipaddrMask;
    ip_addr_t               ipaddrGw;

    LW_CLASS_THREADATTR     threadattr;


    if (pucMac == LW_NULL) {                                            /*  没有虚拟网卡 MAC 地址       */
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    if ((cpcServerIp         == LW_NULL) ||
        (cpcServerClientIp   == LW_NULL) ||
        (cpcServerClientMask == LW_NULL) ||
        (cpcServerClientGw   == LW_NULL)) {                             /*  没有服务器及客户机地址      */
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    if (iVerifyOpt != SSL_VERIFY_NONE) {
        if ((cpcCACrtFile == LW_NULL) ||
            (cpcPrivateCrtFile == LW_NULL)) {                           /*  缺少证书文件                */
            _DebugHandle(__ERRORMESSAGE_LEVEL, "param error.\r\n");
            _ErrorHandle(EINVAL);
            return  (PX_ERROR);
        }
    }

    inaddrServer.s_addr = inet_addr(cpcServerIp);                       /*  生成网络地址                */
    ipaddrIp.addr       = ipaddr_addr(cpcServerClientIp);
    ipaddrMask.addr     = ipaddr_addr(cpcServerClientMask);
    ipaddrGw.addr       = ipaddr_addr(cpcServerClientGw);

    pvpnctx = (__PVPN_CONTEXT)__SHEAP_ALLOC(sizeof(__VPN_CONTEXT));     /*  创建 VPN 上下文             */
    if (pvpnctx == LW_NULL) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (PX_ERROR);
    }
    lib_bzero(pvpnctx, sizeof(__VPN_CONTEXT));
    pvpnctx->VPNCTX_iVerifyOpt = iVerifyOpt;                            /*  记录认证方式                */

    iError = __vpnClientOpen(pvpnctx,
                             cpcCACrtFile,
                             cpcPrivateCrtFile,
                             cpcKeyFile,
                             cpcKeyPassword,
                             inaddrServer,
                             usPort,
                             iSSLTimeoutSec);                           /*  创建与服务器的连接          */
    if (iError != ERROR_NONE) {
        __SHEAP_FREE(pvpnctx);
        return  (PX_ERROR);
    }

    iError = __vpnNetifInit(pvpnctx, pucMac);                           /*  创建虚拟网络接口            */
    if (iError != ERROR_NONE) {
        __vpnClientClose(pvpnctx);
        __SHEAP_FREE(pvpnctx);
        return  (PX_ERROR);
    }

    iError = netifapi_netif_add(&pvpnctx->VPNCTX_netif,
                                &ipaddrIp,
                                &ipaddrMask,
                                &ipaddrGw,
                                (PVOID)pvpnctx,
                                __vpnNetifDummyInit,
                                tcpip_input);                           /*  加入 VPN 网络接口           */
    if (iError != ERROR_NONE) {
        __vpnClientClose(pvpnctx);
        __SHEAP_FREE(pvpnctx);
        return  (PX_ERROR);
    }

    netifapi_netif_common(&pvpnctx->VPNCTX_netif,
                          netif_set_link_up, LW_NULL);                  /*  网卡链接正常                */
    netifapi_netif_common(&pvpnctx->VPNCTX_netif,
                          netif_set_up, LW_NULL);                       /*  使能网卡                    */

    API_ThreadAttrBuild(&threadattr,
                        LW_CFG_NET_VPN_STK_SIZE,
                        LW_PRIO_T_BUS,
                        LW_OPTION_THREAD_STK_CHK | LW_OPTION_OBJECT_GLOBAL,
                        (PVOID)pvpnctx);
    if (API_ThreadCreate("t_vpnproc", __vpnNetifProc, &threadattr, LW_NULL) == LW_OBJECT_HANDLE_INVALID) {
        netifapi_netif_common(&pvpnctx->VPNCTX_netif, netif_remove, LW_NULL);
        __vpnClientClose(pvpnctx);
        __SHEAP_FREE(pvpnctx);
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_INetVpnClientDelete
** 功能描述: 删除一个 VPN 虚拟网卡并且释放相关资源
** 输　入  : pcNetifName   网络接口名
** 输　出  : ERROR
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
INT  API_INetVpnClientDelete (CPCHAR   pcNetifName)
{
    __PVPN_CONTEXT          pvpnctx;
    struct netif           *netif;

    if (pcNetifName == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    netif = netif_find((PCHAR)pcNetifName);
    if (netif == LW_NULL) {
        _ErrorHandle(ENOENT);
        return  (PX_ERROR);
    }

    if ((pcNetifName[0] != __VPN_IFNAME0) ||
        (pcNetifName[1] != __VPN_IFNAME1)) {
        _ErrorHandle(ENOENT);
        return  (PX_ERROR);
    }

    pvpnctx = (__PVPN_CONTEXT)netif->state;
    if (pvpnctx->VPNCTX_iMode != __VPN_SSL_CLIENT) {
        _ErrorHandle(ENOENT);
        return  (PX_ERROR);
    }

    return  (shutdown(pvpnctx->VPNCTX_iSocket, 2));                     /*  断开 ssl 网络连接           */
                                                                        /*  VPN 处理线程自动退出        */
}
#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_NET_VPN_EN > 0       */
/*********************************************************************************************************
  END
*********************************************************************************************************/
