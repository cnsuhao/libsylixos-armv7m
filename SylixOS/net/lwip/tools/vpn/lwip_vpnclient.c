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
** 描        述: SSL VPN 虚拟网络接口.
**
** BUG:
2011.11.22  升级 polarSSL -> V1.0.0 修改相关接口.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
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
#include "socket.h"
#include "polarssl/ssl.h"
#include "polarssl/havege.h"
#include "lwip_vpnlib.h"
/*********************************************************************************************************
** 函数名称: __vpnClientConnect
** 功能描述: VPN 客户端链接服务器
** 输　入  : pvpnctx                VPN 上下文 (除了 VPNCTX_iVerifyOpt 有初值, 其他字段必须经过清空)
**           cpcCACrtFile           CA 证书文件     .pem or .crt
**           cpcPrivateCrtFile      私有证书文件    .pem or .crt
**           cpcKeyFile             私有密钥文件    .pem or .key
**           cpcKeyPassword         私有密钥文件解密密码, 如果密钥文件不存在密码, 则为 NULL
**           inaddr                 SSL 服务器地址
**           usPort                 SSL 服务器端口  (网络字节序)
**           iSSLTimeoutSec         超时时间(单位秒, 推荐: 600)
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __vpnClientOpen (__PVPN_CONTEXT  pvpnctx,
                      CPCHAR          cpcCACrtFile,
                      CPCHAR          cpcPrivateCrtFile,
                      CPCHAR          cpcKeyFile,
                      CPCHAR          cpcKeyPassword,
                      struct in_addr  inaddr,
                      u16_t           usPort,
                      INT             iSSLTimeoutSec)
{
    INT                     i;
    INT                     iError = PX_ERROR;
    struct sockaddr_in      sockaddrinRemote;
    
    (VOID)iSSLTimeoutSec;                                               /*  新的 PolarSSL 暂未使用      */

    if (pvpnctx == LW_NULL) {
        return  (PX_ERROR);
    }

    pvpnctx->VPNCTX_iMode   = __VPN_SSL_CLIENT;                         /*  设置为 client 模式          */
    pvpnctx->VPNCTX_iSocket = PX_ERROR;                                 /*  没有创建 socket             */

    havege_init(&pvpnctx->VPNCTX_haveagestat);                          /*  初始化随机数                */

    if (pvpnctx->VPNCTX_iVerifyOpt != SSL_VERIFY_NONE) {                /*  需要认证证书                */
        /*
         *  安装 CA 证书和客户端证书
         */
        iError = x509parse_crtfile(&pvpnctx->VPNCTX_x509certCA, cpcCACrtFile);
        if (iError != ERROR_NONE) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "CA root certificate error.\r\n");
            return  (PX_ERROR);
        }

        iError = x509parse_crtfile(&pvpnctx->VPNCTX_x509certPrivate, cpcPrivateCrtFile);
        if (iError != ERROR_NONE) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "client certificate error.\r\n");
            goto    __error_handle;
        }

        /*
         *  安装 RSA 私有密钥
         */
        if (cpcKeyFile) {
            iError = x509parse_keyfile(&pvpnctx->VPNCTX_rasctx, cpcKeyFile, cpcKeyPassword);
        } else {
            iError = x509parse_keyfile(&pvpnctx->VPNCTX_rasctx, cpcPrivateCrtFile, cpcKeyPassword);
        }
        if (iError != ERROR_NONE) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "key file error.\r\n");
            goto    __error_handle;
        }
    }

    /*
     *  链接 SSL 服务器
     */
    pvpnctx->VPNCTX_iSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (pvpnctx->VPNCTX_iSocket < 0) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not create socket.\r\n");
        goto    __error_handle;
    }

    lib_bzero(&sockaddrinRemote, sizeof(sockaddrinRemote));
    sockaddrinRemote.sin_len    = sizeof(struct sockaddr_in);
    sockaddrinRemote.sin_family = AF_INET;
    sockaddrinRemote.sin_addr   = inaddr;
    sockaddrinRemote.sin_port   = usPort;

    if(connect(pvpnctx->VPNCTX_iSocket,
               (struct sockaddr *)&sockaddrinRemote,
               sizeof(struct sockaddr_in)) < 0) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not connect server.\r\n");
        goto    __error_handle;
    }

    havege_init(&pvpnctx->VPNCTX_haveagestat);                          /*  初始化随机数                */

    /*
     *  初始化 SSL/STL
     */
    if (ssl_init(&pvpnctx->VPNCTX_sslctx) != ERROR_NONE) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not init ssl context.\r\n");
        goto    __error_handle;
    }

    ssl_set_endpoint(&pvpnctx->VPNCTX_sslctx, SSL_IS_CLIENT);
    ssl_set_authmode(&pvpnctx->VPNCTX_sslctx, pvpnctx->VPNCTX_iVerifyOpt);

    ssl_set_rng(&pvpnctx->VPNCTX_sslctx, havege_random, &pvpnctx->VPNCTX_haveagestat);
    ssl_set_dbg(&pvpnctx->VPNCTX_sslctx, LW_NULL, stdout);              /*  不需要 DEBUG 信息           */

    ssl_set_bio(&pvpnctx->VPNCTX_sslctx,
                net_recv, &pvpnctx->VPNCTX_iSocket,
                net_send, &pvpnctx->VPNCTX_iSocket);

    ssl_set_ciphersuites(&pvpnctx->VPNCTX_sslctx, ssl_default_ciphersuites);
    ssl_set_session(&pvpnctx->VPNCTX_sslctx, &pvpnctx->VPNCTX_sslsn);

    ssl_set_ca_chain(&pvpnctx->VPNCTX_sslctx, &pvpnctx->VPNCTX_x509certCA, LW_NULL, LW_NULL);
    ssl_set_own_cert(&pvpnctx->VPNCTX_sslctx, &pvpnctx->VPNCTX_x509certPrivate, &pvpnctx->VPNCTX_rasctx);

    ssl_set_hostname(&pvpnctx->VPNCTX_sslctx, LW_NULL);                 /*  不设置服务器名              */

    for (i = 0; i < __VPN_SSL_HANDSHAKE_MAX_TIME; i++) {
        iError = ssl_handshake(&pvpnctx->VPNCTX_sslctx);                /*  握手                        */
        if (iError == ERROR_NONE) {
            break;
        } else if ((iError != POLARSSL_ERR_NET_WANT_READ) &&
                   (iError != POLARSSL_ERR_NET_WANT_WRITE)) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "can not handshake.\r\n");
            goto    __error_handle;
        }
    }
    if (i >= __VPN_SSL_HANDSHAKE_MAX_TIME) {
        goto    __error_handle;
    }

    return  (ERROR_NONE);

__error_handle:
    if (pvpnctx->VPNCTX_iSocket >= 0) {
        net_close(pvpnctx->VPNCTX_iSocket);
    }
    x509_free(&pvpnctx->VPNCTX_x509certPrivate);
    x509_free(&pvpnctx->VPNCTX_x509certCA);
    rsa_free(&pvpnctx->VPNCTX_rasctx);
    ssl_free(&pvpnctx->VPNCTX_sslctx);

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __vpnClientClose
** 功能描述: VPN 客户端关闭链接并释放 ssl 相关数据结构
** 输　入  : pvpnctx                VPN 上下文
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __vpnClientClose (__PVPN_CONTEXT  pvpnctx)
{
    if (pvpnctx == LW_NULL) {
        return  (PX_ERROR);
    }

    ssl_close_notify(&pvpnctx->VPNCTX_sslctx);                          /*  通知 ssl 即将关闭           */

    if (pvpnctx->VPNCTX_iSocket >= 0) {
        net_close(pvpnctx->VPNCTX_iSocket);
    }
    x509_free(&pvpnctx->VPNCTX_x509certPrivate);
    x509_free(&pvpnctx->VPNCTX_x509certCA);
    rsa_free(&pvpnctx->VPNCTX_rasctx);
    ssl_free(&pvpnctx->VPNCTX_sslctx);

    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_NET_VPN_EN > 0       */
/*********************************************************************************************************
  END
*********************************************************************************************************/

