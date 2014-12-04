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
** 文   件   名: lwip_nat.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2010 年 03 月 19 日
**
** 描        述: lwip NAT 支持包.

** BUG:
2011.03.06  修正 gcc 4.5.1 相关 warning.
2011.07.30  nats 命令不能在网络终端(例如: telnet)中被调用, 因为他要使用 NAT 锁, printf 如果最终打印到网络
            上, 将会和 netproto 任务抢占 NAT 锁.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "../SylixOS/net/include/net_net.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if (LW_CFG_NET_EN > 0) && (LW_CFG_NET_NAT_EN > 0)
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/inet.h"
#include "lwip_natlib.h"
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
VOID        __natPoolCreate(VOID);
VOID        __natTimer(VOID);
VOID        __natShow(ip_addr_t  *pipaddr);

#if LW_CFG_SHELL_EN > 0
static INT  __tshellNat(INT  iArgC, PCHAR  ppcArgV[]);
static INT  __tshellNatShow(INT  iArgC, PCHAR  ppcArgV[]);
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
/*********************************************************************************************************
  NAT 本地内网与 AP 外网网络接口
*********************************************************************************************************/
extern struct netif        *_G_pnetifNatLocal;
extern struct netif        *_G_pnetifNatAp;
/*********************************************************************************************************
  NAT 操作锁
*********************************************************************************************************/
extern LW_OBJECT_HANDLE     _G_ulNatOpLock;
/*********************************************************************************************************
  NAT 刷新定时器
*********************************************************************************************************/
static LW_OBJECT_HANDLE     _G_ulNatTimer;
/*********************************************************************************************************
** 函数名称: API_INetNatInit
** 功能描述: internet NAT 初始化
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
VOID  API_INetNatInit (VOID)
{
    static   BOOL   bIsInit = LW_FALSE;
    
    if (bIsInit) {
        return;
    }
    
    __natPoolCreate();                                                  /*  创建 NAT 控制块缓冲池       */
    
    _G_ulNatOpLock = API_SemaphoreMCreate("nat_lock", LW_PRIO_DEF_CEILING, 
                                          LW_OPTION_INHERIT_PRIORITY |
                                          LW_OPTION_OBJECT_GLOBAL,
                                          LW_NULL);                     /*  创建 NAT 操作锁             */
                                          
    _G_ulNatTimer = API_TimerCreate("nat_timer", LW_OPTION_ITIMER | LW_OPTION_OBJECT_GLOBAL, LW_NULL);
    
    API_TimerStart(_G_ulNatTimer, 
                   (LW_CFG_TICKS_PER_SEC * 60),
                   LW_OPTION_AUTO_RESTART,
                   (PTIMER_CALLBACK_ROUTINE)__natTimer,
                   LW_NULL);                                            /*  每分钟执行一次              */
    
#if LW_CFG_SHELL_EN > 0
    API_TShellKeywordAdd("nat", __tshellNat);
    API_TShellFormatAdd("nat", " [-stop] / {[LAN netif] [WAN netif]}");
    API_TShellHelpAdd("nat",   "start or stop NAT network.\n"
                               "eg. nat wl2 en1   (start NAT network use wl2 as LAN, en1 as WAN)\n"
                               "    nat -stop     (stop NAT network)\n");
                               
    API_TShellKeywordAdd("nats", __tshellNatShow);
    API_TShellFormatAdd("nats", " [ip addr]");
    API_TShellHelpAdd("nats",   "show NAT network.\n"
                                "warning: do not use this commond in networking terminal!\n");
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
    
    bIsInit = LW_TRUE;
}
/*********************************************************************************************************
** 函数名称: API_INetNatStart
** 功能描述: 启动 NAT (外网网络接口必须为 lwip 默认的路由接口)
** 输　入  : pcLocalNetif          本地内网网络接口名
**           pcApNetif             外网网络接口名
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_INetNatStart (CPCHAR  pcLocalNetif, CPCHAR  pcApNetif)
{
    struct netif   *pnetifLocal;
    struct netif   *pnetifAp;

    if (!pcLocalNetif || !pcApNetif) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    if (_G_pnetifNatLocal || _G_pnetifNatAp) {                          /*  NAT 正在工作                */
        _ErrorHandle(EISCONN);
        return  (PX_ERROR);
    }
    
    __NAT_OP_LOCK();                                                    /*  锁定 NAT 链表               */
    pnetifLocal = netif_find((PCHAR)pcLocalNetif);
    pnetifAp    = netif_find((PCHAR)pcApNetif);
    
    if (!pnetifLocal || !pnetifAp) {                                    /*  有网络接口不存在            */
        __NAT_OP_UNLOCK();                                              /*  解锁 NAT 链表               */
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    __KERNEL_ENTER();
    _G_pnetifNatLocal = pnetifLocal;                                    /*  保存网络接口                */
    _G_pnetifNatAp    = pnetifAp;
    __KERNEL_EXIT();
    
    __NAT_OP_UNLOCK();                                                  /*  解锁 NAT 链表               */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_INetNatStop
** 功能描述: 停止 NAT (注意: 删除 NAT 网络接口时, 必须要先停止 NAT 网络接口)
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_INetNatStop (VOID)
{
    if (!_G_pnetifNatLocal || !_G_pnetifNatAp) {                        /*  NAT 没有工作                */
        _ErrorHandle(ENOTCONN);
        return  (PX_ERROR);
    }
    
    __NAT_OP_LOCK();                                                    /*  锁定 NAT 链表               */
    
    __KERNEL_ENTER();
    _G_pnetifNatLocal = LW_NULL;
    _G_pnetifNatAp    = LW_NULL;
    __KERNEL_EXIT();
    
    __NAT_OP_UNLOCK();                                                  /*  解锁 NAT 链表               */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellNat
** 功能描述: 系统命令 "nat"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_SHELL_EN > 0

static INT  __tshellNat (INT  iArgC, PCHAR  ppcArgV[])
{
    if (iArgC == 3) {
        if (API_INetNatStart(ppcArgV[1], ppcArgV[2]) != ERROR_NONE) {
            fprintf(stderr, "can not start NAT network, errno : %s\n", lib_strerror(errno));
        } else {
            printf("NAT network started, [LAN : %s] [WAN : %s]\n", ppcArgV[1], ppcArgV[2]);
        }
    } else if (iArgC == 2) {
        if (lib_strcmp(ppcArgV[1], "-stop") == 0) {
            API_INetNatStop();
            printf("NAT network stoped.\n");
        }
    } else {
        fprintf(stderr, "option error!\n");
        return  (-ERROR_TSHELL_EPARAM);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellNatShow
** 功能描述: 系统命令 "nats"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellNatShow (INT  iArgC, PCHAR  ppcArgV[])
{
    ip_addr_t       ipaddr;
    
    if (iArgC == 1) {
        ipaddr.addr = IPADDR_ANY;
        __natShow(&ipaddr);
    } else if (iArgC == 2) {
        ipaddr.addr = inet_addr(ppcArgV[2]);
        __natShow(&ipaddr);
    } else {
        fprintf(stderr, "option error!\n");
        return  (-ERROR_TSHELL_EPARAM);
    }
    
    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */

#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_NET_NAT_EN > 0       */
/*********************************************************************************************************
  END
*********************************************************************************************************/
