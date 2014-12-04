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
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "../SylixOS/net/include/net_net.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if (LW_CFG_NET_EN > 0) && (LW_CFG_NET_VPN_EN > 0) && (LW_CFG_SHELL_EN > 0)
#include "socket.h"
#include "lwip_vpn.h"
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
  e2:22:32:a5:8f:94
*********************************************************************************************************/
/*********************************************************************************************************
** 函数名称: __tshellVpnOpen
** 功能描述: 系统命令 "vpnopen"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __tshellVpnOpen (INT  iArgC, PCHAR  *ppcArgV)
{
    INT          iFd;
    struct stat  statFile;
    PCHAR        pcBuffer = LW_NULL;
    INT          iError;

    INT          i;
    PCHAR        pcParam[12];                                           /*  参数表                      */
    PCHAR        pcTemp;

    PCHAR        pcCACrtFile;
    PCHAR        pcPrivateCrtFile;
    PCHAR        pcKeyFile;
    PCHAR        pcKeyPassword;

    INT          iPort          = 4443;
    INT          iSSLTimeoutSec = 500;
    INT          iVerifyOpt     = LW_VPN_SSL_VERIFY_OPT;
    UCHAR        ucMac[6];
    INT          iMac[6];


    if (iArgC != 2) {
        fprintf(stderr, "argments error!\n");
        return  (-ERROR_TSHELL_EPARAM);
    }

    iFd = open(ppcArgV[1], O_RDONLY);
    if (iFd < 0) {
        fprintf(stderr, "can not read the configration file %s!\n", lib_strerror(errno));
        return  (-ERROR_TSHELL_EPARAM);
    }

    if (fstat(iFd, &statFile) < 0) {
        close(iFd);
        fprintf(stderr, "configration file error!\n");
        return  (-ERROR_TSHELL_EPARAM);
    }

    if ((statFile.st_size) < 1 ||
        (statFile.st_size) > 1024) {                                    /*  配置文件应该在 1K 以内      */
        close(iFd);
        fprintf(stderr, "configration file size error!\n");
        return  (-ERROR_TSHELL_EPARAM);
    }

    pcBuffer = (PCHAR)__SHEAP_ALLOC((size_t)(statFile.st_size) + 1);
    if (pcBuffer == LW_NULL) {
        close(iFd);
        fprintf(stderr, "system low memory!\n");
        return  (-ERROR_SYSTEM_LOW_MEMORY);
    }

    if (read(iFd, pcBuffer, (size_t)statFile.st_size) != (ssize_t)statFile.st_size) {
        __SHEAP_FREE(pcBuffer);
        close(iFd);
        fprintf(stderr, "system low memory!\n");
        return  (-ERROR_SYSTEM_LOW_MEMORY);
    }

    close(iFd);

    /*
     *  按约定顺序获得所有配置参数
     */
    pcTemp = pcBuffer;
    while ((*pcTemp == ' ') || (*pcTemp == '\t')) {                     /*  忽略空白字符                */
        pcTemp++;
        if ((pcTemp - pcBuffer) > (INT)statFile.st_size) {              /*  超过文件大小                */
            fprintf(stderr, "configration file size error!\n");
            goto    __error_handle;
        }
    }

    for (i = 0; i < 11; i++) {                                          /*  获得前 11 个参数            */
        pcParam[i] = pcTemp;
        do {
            pcTemp++;                                                   /*  找到下一参数                */
            if ((pcTemp - pcBuffer) > (INT)statFile.st_size) {          /*  超过文件大小                */
                fprintf(stderr, "configration file size error!\n");
                goto    __error_handle;
            }
        } while ((*pcTemp != '\r') && (*pcTemp != '\n'));
        *pcTemp = PX_EOS;                                               /*  当前参数结束                */
        pcTemp++;                                                       /*  换行                        */

        while ((*pcTemp == ' ')  || 
               (*pcTemp == '\t') || 
               (*pcTemp == '\n') ||
               (*pcTemp == '\r')) {
            pcTemp++;                                                   /*  忽略空白字符                */
            if ((pcTemp - pcBuffer) > (INT)statFile.st_size) {          /*  超过文件大小                */
                fprintf(stderr, "configration file size error!\n");
                goto    __error_handle;
            }
        }
    }

    pcParam[11] = pcTemp;
    pcBuffer[statFile.st_size] = PX_EOS;                                /*  获得最后一个参数            */

    /*
     *  选择参数
     */
    if (lib_strcmp(pcParam[0], "null")) {
        pcCACrtFile = pcParam[0];
    } else {
        pcCACrtFile = LW_NULL;
    }
    if (lib_strcmp(pcParam[1], "null")) {
        pcPrivateCrtFile = pcParam[1];
    } else {
        pcPrivateCrtFile = LW_NULL;
    }
    if (lib_strcmp(pcParam[2], "null")) {
        pcKeyFile = pcParam[2];
    } else {
        pcKeyFile = LW_NULL;
    }
    if (lib_strcmp(pcParam[3], "null")) {
        pcKeyPassword = pcParam[3];
    } else {
        pcKeyPassword = LW_NULL;
    }

    sscanf(pcParam[ 8], "%d", &iPort);
    sscanf(pcParam[ 9], "%d", &iSSLTimeoutSec);
    sscanf(pcParam[10], "%d", &iVerifyOpt);
    sscanf(pcParam[11], "%x:%x:%x:%x:%x:%x", &iMac[0],
                                             &iMac[1],
                                             &iMac[2],
                                             &iMac[3],
                                             &iMac[4],
                                             &iMac[5]);
    for (i = 0; i < 6; i++) {
        ucMac[i] = (UCHAR)iMac[i];
    }

    iError = API_INetVpnClientCreate(pcCACrtFile,
                                     pcPrivateCrtFile,
                                     pcKeyFile,
                                     pcKeyPassword,
                                     pcParam[4],
                                     pcParam[5],
                                     pcParam[6],
                                     pcParam[7],
                                     htons((UINT16)iPort),
                                     iSSLTimeoutSec,
                                     iVerifyOpt,
                                     ucMac);                            /*  创建 vpn 客户机链接         */
    if (iError < ERROR_NONE) {
        fprintf(stderr, "can not create the vpn connection!\n");
        goto    __error_handle;
    }
    
    __SHEAP_FREE(pcBuffer);

    return  (ERROR_NONE);

__error_handle:
    if (pcBuffer) {
        __SHEAP_FREE(pcBuffer);
    }

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __tshellVpnClose
** 功能描述: 系统命令 "vpnclose"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __tshellVpnClose (INT  iArgC, PCHAR  *ppcArgV)
{
    if (iArgC != 2) {
        fprintf(stderr, "argments error!\n");
        return  (-ERROR_TSHELL_EPARAM);
    }

    return  (API_INetVpnClientDelete(ppcArgV[1]));                      /*  移除 VPN 网络接口           */
}
#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_NET_VPN_EN > 0       */
                                                                        /*  LW_CFG_SHELL_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/

