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
** 文   件   名: lwip_vpnnetif.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2010 年 05 月 25 日
**
** 描        述: SSL VPN 应用接口.
*********************************************************************************************************/

#ifndef __LWIP_VPN_H
#define __LWIP_VPN_H

/*********************************************************************************************************
  VPN 证书验证方式
*********************************************************************************************************/

#define LW_VPN_SSL_VERIFY_NONE       0                                  /*  SSL_VERIFY_NONE             */
#define LW_VPN_SSL_VERIFY_OPT        1                                  /*  SSL_VERIFY_OPTIONAL         */
#define LW_VPN_SSL_VERIFY_REQUIRED   2                                  /*  SSL_VERIFY_REQUIRED         */

/*********************************************************************************************************
  VPN API
*********************************************************************************************************/

LW_API VOID     API_INetVpnInit(VOID);
LW_API INT      API_INetVpnClientCreate(CPCHAR          cpcCACrtFile,
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
                                        UINT8          *pucMac);
LW_API INT      API_INetVpnClientDelete(CPCHAR          pcNetifName);

#define inetVpnInit                 API_INetVpnInit
#define inetVpnClientCreate         API_INetVpnClientCreate
#define inetVpnClientDelete         API_INetVpnClientDelete

/*********************************************************************************************************
  shell 命令 "vpnopen" 参数配置文件格式示例如下:
  (参数顺序与 API_INetVpnClientCreate 相同, 注意: 文件无空行, 行首无空格)
  (无认证时, 证书文件和密钥文件都为 null, 当密钥文件不存在密码时, 密码字段为 null)

  /mnt/ata0/vpn/config/ca_crt.pem
  /mnt/ata0/vpn/config/client_crt.pem
  /mnt/ata0/vpn/config/client_key.pem
  123456
  61.186.128.33
  192.168.0.123
  255.255.255.0
  192.168.0.1
  4443
  500
  1
  e2-22-32-a5-8f-94
*********************************************************************************************************/
#endif                                                                  /*  __LWIP_VPN_H                */
/*********************************************************************************************************
  END
*********************************************************************************************************/
