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
** 文   件   名: lwip_natlib.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2010 年 03 月 19 日
**
** 描        述: lwip NAT 支持包内部库.

** BUG:
2010.04.08  今天是购买科鲁兹后第一次升级代码, 修正注释, 修正代码格式.
2010.04.12  修正 nat_ip_input_hook() 类型, 系统使用的 ip_input_hook().
2010.11.06  广州亚运会马上召开, 今天却宣布停止相关的惠民政策, 转而发放惠民款到广州户籍个人, 对外来务工人员
            是赤裸裸的歧视! 外来务工人员为广州亚运的建设添砖加瓦, 却得到如此待遇.
            ip nat 单网口网络代理.
2011.03.06  修正 gcc 4.5.1 相关 warning.
2011.06.23  优化一部分代码. 
            对 NAT 转发的条件进行了严格的限制.
2011.07.01  lwip 支持本人提出的路由改进方案, 至此 NAT 支持单网口转发.
            https://savannah.nongnu.org/bugs/?33634
2011.07.30  当 ap 输入不在代理端口范围以内, 直接退出.
2013.04.01  修正 GCC 4.7.3 引发的新 warning.
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
#include "lwip/opt.h"
#include "lwip/inet.h"
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/udp.h"
#include "lwip/icmp.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip_natlib.h"
/*********************************************************************************************************
  NAT 控制块缓冲池
*********************************************************************************************************/
static __NAT_CB             _G_natcbBuffer[LW_CFG_NET_NAT_MAX_SESSION];
static LW_OBJECT_HANDLE     _G_ulNatcbPool = 0;
/*********************************************************************************************************
  NAT 本地内网与 AP 外网网络接口
*********************************************************************************************************/
       struct netif        *_G_pnetifNatLocal = LW_NULL;
       struct netif        *_G_pnetifNatAp    = LW_NULL;
/*********************************************************************************************************
  NAT 操作锁
*********************************************************************************************************/
       LW_OBJECT_HANDLE     _G_ulNatOpLock = 0;
/*********************************************************************************************************
  NAT 控制块链表
*********************************************************************************************************/
static LW_LIST_LINE_HEADER  _G_plineNatcbTcp  = LW_NULL;
static LW_LIST_LINE_HEADER  _G_plineNatcbUdp  = LW_NULL;
static LW_LIST_LINE_HEADER  _G_plineNatcbIcmp = LW_NULL;
/*********************************************************************************************************
** 函数名称: __natChksumAdjust
** 功能描述: 调整 NAT 数据包校验和.
** 输　入  : ucChksum        points to the chksum in the packet
**           ucOptr          points to the old data in the packet
**           sOlen           old len
**           ucNptr          points to the new data in the packet
**           sNlen           new len
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __natChksumAdjust (u8_t  *ucChksum, u8_t  *ucOptr, s16_t  sOlen, u8_t  *ucNptr, s16_t  sNlen)
{
    s32_t     i32X, i32Old, i32New;
  
    i32X = (ucChksum[0] << 8) + ucChksum[1];
    i32X = ~i32X & 0xFFFF;
    
    while (sOlen) {
        i32Old  = (ucOptr[0] << 8) + ucOptr[1];
        ucOptr += 2;
        i32X   -= i32Old & 0xffff;
        if (i32X <= 0) {
            i32X--; 
            i32X &= 0xffff;
        }
        sOlen -= 2;
    }
    
    while (sNlen) {
        i32New  = (ucNptr[0] << 8) + ucNptr[1]; 
        ucNptr +=2;
        i32X   += i32New & 0xffff;
        if (i32X & 0x10000) {
            i32X++; 
            i32X &= 0xffff; 
        }
        sNlen -= 2;
    }
    
    i32X = ~i32X & 0xFFFF;
    ucChksum[0] = (u8_t)(i32X >> 8);
    ucChksum[1] = (u8_t)(i32X & 0xff);
}
/*********************************************************************************************************
** 函数名称: __natPoolCreate
** 功能描述: 创建 NAT 控制块缓冲池.
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __natPoolCreate (VOID)
{
    _G_ulNatcbPool = API_PartitionCreate("nat_pool", 
                                         (PVOID)_G_natcbBuffer,
                                         LW_CFG_NET_NAT_MAX_SESSION,
                                         sizeof(__NAT_CB),
                                         LW_OPTION_OBJECT_GLOBAL,
                                         LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __natPoolAlloc
** 功能描述: 分配一个 NAT 控制块.
** 输　入  : NONE
** 输　出  : NAT 控制块
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static __PNAT_CB   __natPoolAlloc (VOID)
{
    __PNAT_CB   pnatcb = (__PNAT_CB)API_PartitionGet(_G_ulNatcbPool);
    
    if (pnatcb) {
        lib_bzero(pnatcb, sizeof(__NAT_CB));
    }

    return  (pnatcb);
}
/*********************************************************************************************************
** 函数名称: __natPoolFree
** 功能描述: 释放一个 NAT 控制块.
** 输　入  : pnatcb            NAT 控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID   __natPoolFree (__PNAT_CB  pnatcb)
{
    if (pnatcb) {
        API_PartitionPut(_G_ulNatcbPool, (PVOID)pnatcb);
    }
}
/*********************************************************************************************************
** 函数名称: __natNewPort
** 功能描述: 产生一个 NAT 临时唯一端口.
** 输　入  : NONE
** 输　出  : 端口
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static u16_t   __natNewPort (VOID)
{
    static u32_t    u32Port = LW_CFG_NET_NAT_MIN_PORT;
    __PNAT_CB       pnatcb;
    PLW_LIST_LINE   plineTemp;
    
__check_again:
    if (++u32Port > LW_CFG_NET_NAT_MAX_PORT) {
        u32Port = LW_CFG_NET_NAT_MIN_PORT;
    }
    
    for (plineTemp  = _G_plineNatcbTcp;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {                 /*  查找是否有重复的端口号      */
        
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        if (pnatcb->NAT_usAssPort == (u16_t)u32Port) {
            goto    __check_again;
        }
    }
    
    for (plineTemp  = _G_plineNatcbUdp;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {                 /*  查找是否有重复的端口号      */
        
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        if (pnatcb->NAT_usAssPort == (u16_t)u32Port) {
            goto    __check_again;
        }
    }
    
    for (plineTemp  = _G_plineNatcbIcmp;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {                 /*  查找是否有重复的端口号      */
        
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        if (pnatcb->NAT_usAssPort == (u16_t)u32Port) {
            goto    __check_again;
        }
    }
    
    return  ((u16_t)u32Port);
}
/*********************************************************************************************************
** 函数名称: __natNew
** 功能描述: 创建一个 NAT 对象.
** 输　入  : pipaddr       本地机器 IP
**           usPort        本地机器 端口
**           ucProto       使用的协议
** 输　出  : NAT 控制块
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static __PNAT_CB   __natNew (ip_addr_p_t  *pipaddr, u16_t  usPort, u8_t  ucProto)
{
    PLW_LIST_LINE   plineTemp;
    __PNAT_CB       pnatcb = __natPoolAlloc();
    __PNAT_CB       pnatcbOldest;
    u16_t           usNewPort;
    
    if (pnatcb == LW_NULL) {                                            /*  需要删除最古老的            */
        /*
         *  没有开辟出来, 所以协议代理的三个链表中, 必然至少有一个存在节点.
         */
        if (_G_plineNatcbTcp) {
            pnatcbOldest = _LIST_ENTRY(_G_plineNatcbTcp, __NAT_CB, NAT_lineManage);
        } else if (_G_plineNatcbUdp) {
            pnatcbOldest = _LIST_ENTRY(_G_plineNatcbUdp, __NAT_CB, NAT_lineManage);
        } else {
            pnatcbOldest = _LIST_ENTRY(_G_plineNatcbIcmp, __NAT_CB, NAT_lineManage);
        }
            
        for (plineTemp  = _G_plineNatcbTcp;
             plineTemp != LW_NULL;
             plineTemp  = _list_line_get_next(plineTemp)) {
            
            pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
            if (pnatcb->NAT_ulIdleTimer > pnatcbOldest->NAT_ulIdleTimer) {
                pnatcbOldest = pnatcb;
            }
        }
        for (plineTemp  = _G_plineNatcbUdp;
             plineTemp != LW_NULL;
             plineTemp  = _list_line_get_next(plineTemp)) {
            
            pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
            if (pnatcb->NAT_ulIdleTimer > pnatcbOldest->NAT_ulIdleTimer) {
                pnatcbOldest = pnatcb;
            }
        }
        for (plineTemp  = _G_plineNatcbIcmp;
             plineTemp != LW_NULL;
             plineTemp  = _list_line_get_next(plineTemp)) {
            
            pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
            if (pnatcb->NAT_ulIdleTimer > pnatcbOldest->NAT_ulIdleTimer) {
                pnatcbOldest = pnatcb;
            }
        }
        
        pnatcb = pnatcbOldest;                                          /*  使用最老的                  */
        switch (pnatcb->NAT_ucProto) {
        
        case IP_PROTO_TCP:                                              /*  TCP 链表                    */
            _List_Line_Del(&pnatcb->NAT_lineManage, &_G_plineNatcbTcp);
            break;
            
        case IP_PROTO_UDP:                                              /*  UDP 链表                    */
            _List_Line_Del(&pnatcb->NAT_lineManage, &_G_plineNatcbUdp);
            break;
            
        case IP_PROTO_ICMP:                                             /*  ICMP 链表                   */
            _List_Line_Del(&pnatcb->NAT_lineManage, &_G_plineNatcbIcmp);
            break;
            
        default:                                                        /*  不应该运行到这里            */
            _List_Line_Del(&pnatcb->NAT_lineManage, &_G_plineNatcbIcmp);
            break;
        }
    }
        
    pnatcb->NAT_ucProto            = ucProto;
    pnatcb->NAT_ipaddrLocalIp.addr = pipaddr->addr;
    pnatcb->NAT_usLocalPort        = usPort;
    
    usNewPort = __natNewPort();
    pnatcb->NAT_usAssPort = htons(usNewPort);                           /*  分配一个新的唯一端口        */
    
    pnatcb->NAT_ulIdleTimer = 0;
    pnatcb->NAT_ulTermTimer = 0;
    pnatcb->NAT_iStatus     = __NAT_STATUS_OPEN;
    
    switch (ucProto) {
        
    case IP_PROTO_TCP:                                                  /*  TCP 链表                    */
        _List_Line_Add_Ahead(&pnatcb->NAT_lineManage, &_G_plineNatcbTcp);
        break;
        
    case IP_PROTO_UDP:                                                  /*  UDP 链表                    */
        _List_Line_Add_Ahead(&pnatcb->NAT_lineManage, &_G_plineNatcbUdp);
        break;
        
    case IP_PROTO_ICMP:                                                 /*  ICMP 链表                   */
        _List_Line_Add_Ahead(&pnatcb->NAT_lineManage, &_G_plineNatcbIcmp);
        break;
        
    default:                                                            /*  不应该运行到这里            */
        _List_Line_Add_Ahead(&pnatcb->NAT_lineManage, &_G_plineNatcbIcmp);
        break;
    }
    
    return  (pnatcb);
}
/*********************************************************************************************************
** 函数名称: __natClose
** 功能描述: 删除一个 NAT 对象.
** 输　入  : pnatcb            NAT 控制块
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __natClose (__PNAT_CB  pnatcb)
{
    if (pnatcb == LW_NULL) {
        return;
    }
    
    switch (pnatcb->NAT_ucProto) {
    
    case IP_PROTO_TCP:                                                  /*  TCP 链表                    */
        _List_Line_Del(&pnatcb->NAT_lineManage, &_G_plineNatcbTcp);
        break;
        
    case IP_PROTO_UDP:                                                  /*  UDP 链表                    */
        _List_Line_Del(&pnatcb->NAT_lineManage, &_G_plineNatcbUdp);
        break;
        
    case IP_PROTO_ICMP:                                                 /*  ICMP 链表                   */
        _List_Line_Del(&pnatcb->NAT_lineManage, &_G_plineNatcbIcmp);
        break;
        
    default:                                                            /*  不应该运行到这里            */
        _List_Line_Del(&pnatcb->NAT_lineManage, &_G_plineNatcbIcmp);
        break;
    }

    __natPoolFree(pnatcb);
}
/*********************************************************************************************************
** 函数名称: __natTimer
** 功能描述: NAT 定时器.
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __natTimer (VOID)
{
    __PNAT_CB       pnatcb;
    PLW_LIST_LINE   plineTemp;
    
    __NAT_OP_LOCK();                                                    /*  锁定 NAT 链表               */
    plineTemp = _G_plineNatcbTcp;
    while (plineTemp) {
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        plineTemp = _list_line_get_next(plineTemp);                     /*  指向下一个节点              */
        
        pnatcb->NAT_ulIdleTimer++;
        if (pnatcb->NAT_ulIdleTimer >= LW_CFG_NET_NAT_IDLE_TIMEOUT) {   /*  空闲时间过长关闭            */
            __natClose(pnatcb);
        
        } else {
            if (pnatcb->NAT_iStatus == __NAT_STATUS_CLOSING) {
                pnatcb->NAT_ulTermTimer++;
                if (pnatcb->NAT_ulTermTimer >= 3) {                     /*  断链超时                    */
                    __natClose(pnatcb);
                }
            }
        }
    }
    
    plineTemp = _G_plineNatcbUdp;
    while (plineTemp) {
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        plineTemp = _list_line_get_next(plineTemp);                     /*  指向下一个节点              */
        
        pnatcb->NAT_ulIdleTimer++;
        if (pnatcb->NAT_ulIdleTimer >= LW_CFG_NET_NAT_IDLE_TIMEOUT) {   /*  空闲时间过长关闭            */
            __natClose(pnatcb);
        }
    }
    
    plineTemp = _G_plineNatcbIcmp;
    while (plineTemp) {
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        plineTemp = _list_line_get_next(plineTemp);                     /*  指向下一个节点              */
        
        pnatcb->NAT_ulIdleTimer++;
        if (pnatcb->NAT_ulIdleTimer >= LW_CFG_NET_NAT_IDLE_TIMEOUT) {   /*  空闲时间过长关闭            */
            __natClose(pnatcb);
        }
    }
    __NAT_OP_UNLOCK();                                                  /*  解锁 NAT 链表               */
}
/*********************************************************************************************************
** 函数名称: __natLocalInput
** 功能描述: NAT 网络接口输入 (本地端).
** 输　入  : p             数据包
**           netif         网络接口
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static err_t  __natLocalInput (struct pbuf *p, struct netif *netif)
{
    struct ip_hdr           *iphdr   = (struct ip_hdr *)p->payload;
    struct tcp_hdr          *tcphdr  = LW_NULL;
    struct udp_hdr          *udphdr  = LW_NULL;
    struct icmp_echo_hdr    *icmphdr = LW_NULL;
    
    u32_t                    u32OldAddr;
    u16_t                    usDestPort, usSrcPort;
    u8_t                     ucProto;
    
    static u16_t             iphdrlen;
    
    __PNAT_CB                pnatcb  = LW_NULL;
    PLW_LIST_LINE            plineTemp;
    LW_LIST_LINE_HEADER      plineHeader;
    
    
    if (ip_addr_cmp(&iphdr->dest, &netif->ip_addr)) {
        return  (ERR_OK);                                               /*  指向本机的数据包            */
    
    } else if (ip_addr_cmp(&iphdr->dest, IP_ADDR_BROADCAST)) {
        return  (ERR_OK);                                               /*  本地受限广播                */
    }
    
    iphdrlen  = (u16_t)IPH_HL(iphdr);                                   /*  获得 IP 报头长度            */
    iphdrlen *= 4;
    
    /*
     *  这里必须保证 p->payload 足够大装入了 IP + UDP/TCP/ICMP 的报头
     */
    ucProto = IPH_PROTO(iphdr);
    switch (ucProto) {
    
    case IP_PROTO_TCP:                                                  /*  TCP 数据报                  */
        tcphdr = (struct tcp_hdr *)(((u8_t *)p->payload) + iphdrlen);
        usDestPort  = tcphdr->dest;
        usSrcPort   = tcphdr->src;
        plineHeader = _G_plineNatcbTcp;
        break;
        
    case IP_PROTO_UDP:                                                  /*  UDP 数据报                  */
        udphdr = (struct udp_hdr *)(((u8_t *)p->payload) + iphdrlen);
        usDestPort  = udphdr->dest;
        usSrcPort   = udphdr->src;
        plineHeader = _G_plineNatcbUdp;
        break;
        
    case IP_PROTO_ICMP:
        icmphdr = (struct icmp_echo_hdr *)(((u8_t *)p->payload) + iphdrlen);
        usSrcPort   = usDestPort = icmphdr->id;
        plineHeader = _G_plineNatcbIcmp;
        break;
    
    default:
        plineHeader = _G_plineNatcbIcmp;
        break;
    }
    
    __NAT_OP_LOCK();                                                    /*  锁定 NAT 链表               */
    
    for (plineTemp  = plineHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
        
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        
        /*
         *  控制块内的源端 IP 与 源端 PORT 使用协议完全一致.
         */
        if ((ip_addr_cmp(&iphdr->src, &pnatcb->NAT_ipaddrLocalIp)) &&
            (usSrcPort == pnatcb->NAT_usLocalPort) &&
            (ucProto   == pnatcb->NAT_ucProto)) {
            break;                                                      /*  找到了 NAT 控制块           */
        }
    }
    if (plineTemp == LW_NULL) {
        pnatcb = LW_NULL;                                               /*  没有找到                    */
    }
    
    if (pnatcb == LW_NULL) {
        INT  iSrcSameNet  = ip_addr_netcmp(&iphdr->src, &netif->ip_addr, &netif->netmask);
        INT  iDestSameNet = ip_addr_netcmp(&iphdr->dest, &netif->ip_addr, &netif->netmask);
        
        if (iSrcSameNet && !iDestSameNet) {
            /*  
             *  如果源地址与 local 接口属于同一网段, 目的地址与 local 不属于同一网段 (则需要 NAT 转发)
             */
            pnatcb = __natNew(&iphdr->src, usSrcPort, ucProto);         /*  新建控制块                  */
        }
    }
    
    if (pnatcb) {
        pnatcb->NAT_ulIdleTimer = 0;                                    /*  刷新空闲时间                */
        
        /*
         *  将数据包转换为本机 AP 外网网络接口发送的数据包
         */
        u32OldAddr = iphdr->src.addr;
        ((ip_addr_t *)&(iphdr->src))->addr = _G_pnetifNatAp->ip_addr.addr;
        __natChksumAdjust((u8_t *)&IPH_CHKSUM(iphdr),(u8_t *)&u32OldAddr, 4, (u8_t *)&iphdr->src.addr, 4);
        
        /*
         *  本机发送到外网的数据包使用 NAT_usAssPort (唯一的分配端口)
         */
        switch (IPH_PROTO(iphdr)) {
        
        case IP_PROTO_TCP:
            __natChksumAdjust((u8_t *)&tcphdr->chksum,(u8_t *)&u32OldAddr, 4, (u8_t *)&iphdr->src.addr, 4);
            tcphdr->src = pnatcb->NAT_usAssPort;
            __natChksumAdjust((u8_t *)&tcphdr->chksum,(u8_t *)&usSrcPort, 2, (u8_t *)&tcphdr->src, 2);
            break;
            
        case IP_PROTO_UDP:
            if (udphdr->chksum != 0) {
        	    __natChksumAdjust((u8_t *)&udphdr->chksum,(u8_t *)&u32OldAddr, 4, (u8_t *)&iphdr->src.addr, 4);
        	    udphdr->src = pnatcb->NAT_usAssPort;
        	    __natChksumAdjust((u8_t *)&udphdr->chksum,(u8_t *)&usSrcPort, 2, (u8_t *)&udphdr->src, 2);
        	}
            break;
            
        case IP_PROTO_ICMP:
            icmphdr->id = pnatcb->NAT_usAssPort;
            __natChksumAdjust((u8_t *)&icmphdr->chksum,(u8_t *)&usSrcPort, 2, (u8_t *)&icmphdr->id, 2);
            break;
            
        default:
            __NAT_OP_UNLOCK();                                      /*  解锁 NAT 链表               */
            return  (ERR_RTE);
            break;
        }
        
        /*
         *  NAT 控制块状态更新
         */
        if (IPH_PROTO(iphdr) == IP_PROTO_TCP) {
            if (TCPH_FLAGS(tcphdr) & TCP_RST) {
	            pnatcb->NAT_iStatus = __NAT_STATUS_CLOSING;
            } else if (TCPH_FLAGS(tcphdr) & TCP_FIN) {
	            if (pnatcb->NAT_iStatus == __NAT_STATUS_OPEN) {
	                pnatcb->NAT_iStatus = __NAT_STATUS_FIN;
	            } else {
	                pnatcb->NAT_iStatus = __NAT_STATUS_CLOSING;
	            }
            }
        }
    }
    __NAT_OP_UNLOCK();                                                  /*  解锁 NAT 链表               */
    
    return  (ERR_OK);
}
/*********************************************************************************************************
** 函数名称: __natApInput
** 功能描述: NAT 网络接口输入 (广域网端).
** 输　入  : p             数据包
**           netif         网络接口
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static err_t  __natApInput (struct pbuf *p, struct netif *netif)
{
    struct ip_hdr           *iphdr   = (struct ip_hdr *)p->payload;
    struct tcp_hdr          *tcphdr  = LW_NULL;
    struct udp_hdr          *udphdr  = LW_NULL;
    struct icmp_echo_hdr    *icmphdr = LW_NULL;
    
    u32_t                    u32OldAddr;
    u16_t                    usDestPort;
    u8_t                     ucProto;
    
    static u16_t             iphdrlen;
    
    __PNAT_CB                pnatcb  = LW_NULL;
    PLW_LIST_LINE            plineTemp;
    LW_LIST_LINE_HEADER      plineHeader;
    
    
    iphdrlen  = (u16_t)IPH_HL(iphdr);                                   /*  获得 IP 报头长度            */
    iphdrlen *= 4;
    
    /*
     *  这里必须保证 p->payload 足够大装入了 IP + UDP/TCP/ICMP 的报头
     */
    ucProto = IPH_PROTO(iphdr);
    switch (ucProto) {
    
    case IP_PROTO_TCP:                                                  /*  TCP 数据报                  */
        tcphdr = (struct tcp_hdr *)(((u8_t *)p->payload) + iphdrlen);
        usDestPort  = tcphdr->dest;
        plineHeader = _G_plineNatcbTcp;
        break;
        
    case IP_PROTO_UDP:                                                  /*  UDP 数据报                  */
        udphdr = (struct udp_hdr *)(((u8_t *)p->payload) + iphdrlen);
        usDestPort  = udphdr->dest;
        plineHeader = _G_plineNatcbUdp;
        break;
        
    case IP_PROTO_ICMP:
        icmphdr = (struct icmp_echo_hdr *)(((u8_t *)p->payload) + iphdrlen);
        usDestPort = icmphdr->id;
        plineHeader = _G_plineNatcbIcmp;
        break;
    
    default:
        plineHeader = _G_plineNatcbIcmp;
        break;
    }
    
    if ((ntohs(usDestPort) < LW_CFG_NET_NAT_MIN_PORT) || 
        (ntohs(usDestPort) > LW_CFG_NET_NAT_MAX_PORT)) {                /*  目标端口不在代理端口之间    */
        return  (ERR_OK);
    }
    
    __NAT_OP_LOCK();                                                    /*  锁定 NAT 链表               */
    
    for (plineTemp  = plineHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
        
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        if ((usDestPort == pnatcb->NAT_usAssPort) &&
            (ucProto    == pnatcb->NAT_ucProto)) {
            break;                                                      /*  找到了 NAT 控制块           */
        }
    }
    if (plineTemp == LW_NULL) {
        pnatcb = LW_NULL;                                               /*  没有找到                    */
    }
    
    if (pnatcb) {                                                       /*  如果找到控制块              */
        pnatcb->NAT_ulIdleTimer = 0;                                    /*  刷新空闲时间                */
        
        /*
         *  将数据包目标转为本地内网目标地址
         */
        u32OldAddr = iphdr->dest.addr;
        ((ip_addr_t *)&(iphdr->dest))->addr = pnatcb->NAT_ipaddrLocalIp.addr;
        __natChksumAdjust((u8_t *)&IPH_CHKSUM(iphdr),(u8_t *)&u32OldAddr, 4, (u8_t *)&iphdr->dest.addr, 4);
        
        
        /*
         *  本机发送到内网的数据包目标端口为 NAT_usLocalPort
         */
        if (IPH_PROTO(iphdr) == IP_PROTO_TCP) {
            __natChksumAdjust((u8_t *)&tcphdr->chksum,(u8_t *)&u32OldAddr, 4, (u8_t *)&iphdr->dest.addr, 4);
            tcphdr->dest = pnatcb->NAT_usLocalPort;
            __natChksumAdjust((u8_t *)&tcphdr->chksum,(u8_t *)&usDestPort, 2, (u8_t *)&tcphdr->dest, 2);
        
        } else if (IPH_PROTO(iphdr) == IP_PROTO_UDP) {
            if (udphdr->chksum != 0) {
                __natChksumAdjust((u8_t *)&udphdr->chksum,(u8_t *)&u32OldAddr, 4, (u8_t *)&iphdr->dest.addr, 4);
	            udphdr->dest = pnatcb->NAT_usLocalPort;
	            __natChksumAdjust((u8_t *)&udphdr->chksum,(u8_t *)&usDestPort, 2, (u8_t *)&udphdr->dest, 2);
            }
            
        } else if ((IPH_PROTO(iphdr) == IP_PROTO_ICMP) && 
                   ((ICMPH_CODE(icmphdr) == ICMP_ECHO || ICMPH_CODE(icmphdr) == ICMP_ER))) {
            icmphdr->id = pnatcb->NAT_usLocalPort;
            __natChksumAdjust((u8_t *)&icmphdr->chksum,(u8_t *)&usDestPort, 2, (u8_t *)&icmphdr->id, 2);
        }
        
        /*
         *  NAT 控制块状态更新
         */
        if (IPH_PROTO(iphdr) == IP_PROTO_TCP) {
            if (TCPH_FLAGS(tcphdr) & TCP_RST) {
	            pnatcb->NAT_iStatus = __NAT_STATUS_CLOSING;
            } else if (TCPH_FLAGS(tcphdr) & TCP_FIN) {
	            if (pnatcb->NAT_iStatus == __NAT_STATUS_OPEN) {
	                pnatcb->NAT_iStatus = __NAT_STATUS_FIN;
	            } else {
	                pnatcb->NAT_iStatus = __NAT_STATUS_CLOSING;
	            }
            }
        }
    } else {
        /*
         *  TODO: 外网主动链接内网计算机, 未来支持!
         */
    }
    
    __NAT_OP_UNLOCK();                                                  /*  解锁 NAT 链表               */

    return  (ERR_OK);
}
/*********************************************************************************************************
** 函数名称: nat_ip_input_hook
** 功能描述: NAT ip 输入回调
** 输　入  : p         数据包
**           inp       网络接口
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  nat_ip_input_hook (struct pbuf *p, struct netif *inp)
{
    struct ip_hdr   *iphdr = (struct ip_hdr *)p->payload;
    
    if (IPH_V(iphdr) != 4) {
        return;                                                         /*  IPv4 有效                   */
    }

    if (_G_pnetifNatLocal == _G_pnetifNatAp) {                          /*  单网口代理                  */
        if (inp == _G_pnetifNatLocal) {
            if (ip_addr_cmp(&iphdr->dest, &inp->ip_addr)) {
                __natApInput(p, inp);                                   /*  指向本机的数据包            */
            
            } else if (ip_addr_isbroadcast(&iphdr->dest, inp) == 0) {
                __natLocalInput(p, inp);                                /*  非指向本机的非广播包        */
            }
        }
    
    } else {                                                            /*  多网口                      */
        if (inp == _G_pnetifNatLocal) {
            __natLocalInput(p, inp);
        
        } else if (inp == _G_pnetifNatAp) {
            __natApInput(p, inp);
        }
    }
}
/*********************************************************************************************************
** 函数名称: __natShow
** 功能描述: 打印所有 NAT 代理信息
** 输　入  : pipaddr        指定的 IP 地址, (INADDR_ANY 表示所有 IP)
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __natShow (ip_addr_t  *pipaddr)
{
    static const CHAR   natInfoHeader[] = "\n"
    "    LOCAL IP    LOCAL PORT ASS PORT PROTO IDLE(min)  STATUS\n"
    "--------------- ---------- -------- ----- --------- --------\n";
    
    PCHAR           pcProto;
    PCHAR           pcStatus;
    __PNAT_CB       pnatcb  = LW_NULL;
    PLW_LIST_LINE   plineTemp;
    
    INT             iType = LW_DRV_TYPE_SOCKET;
    
    
    printf("NAT networking show >>\n");
    printf("%s", natInfoHeader);
    
    /*
     *  因为打印在 NAT_OP 锁中完成, 而协议栈内部也需要进入此锁, 所以, 不能在使用 socket 输出
     */
    iosFdGetType(STD_OUT, &iType);
    if (iType == LW_DRV_TYPE_SOCKET) {
        printf("NAT networking show do not support out put by socket fd!\n");
        return;
    }
    
    __NAT_OP_LOCK();                                                    /*  锁定 NAT 链表               */
    for (plineTemp  = _G_plineNatcbTcp;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
        
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        if ((pipaddr->addr == INADDR_ANY) ||
            (pipaddr->addr == pnatcb->NAT_ipaddrLocalIp.addr)) {
            /*
             *  打印节点信息
             */
            struct in_addr  inaddr;
            CHAR            cIpBuffer[INET_ADDRSTRLEN];
            
            inaddr.s_addr = pnatcb->NAT_ipaddrLocalIp.addr;
            
            if (pnatcb->NAT_iStatus == __NAT_STATUS_OPEN) {
                pcStatus = "OPEN";
            } else if (pnatcb->NAT_iStatus == __NAT_STATUS_FIN) {
                pcStatus = "FIN";
            } else if (pnatcb->NAT_iStatus == __NAT_STATUS_CLOSING) {
                pcStatus = "CLOSING";
            } else {
                pcStatus = "?";
            }
            
            inet_ntoa_r(inaddr, cIpBuffer, INET_ADDRSTRLEN);
            printf("%-15s %10d %8d %-5s %9ld %-8s\n", cIpBuffer,
                                                      ntohs(pnatcb->NAT_usLocalPort),
                                                      ntohs(pnatcb->NAT_usAssPort),
                                                      "TCP",
                                                      pnatcb->NAT_ulIdleTimer,
                                                      pcStatus);
        }
    }
    for (plineTemp  = _G_plineNatcbUdp;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
        
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        if ((pipaddr->addr == INADDR_ANY) ||
            (pipaddr->addr == pnatcb->NAT_ipaddrLocalIp.addr)) {
            /*
             *  打印节点信息
             */
            struct in_addr  inaddr;
            CHAR            cIpBuffer[INET_ADDRSTRLEN];
            
            inaddr.s_addr = pnatcb->NAT_ipaddrLocalIp.addr;
            
            if (pnatcb->NAT_iStatus == __NAT_STATUS_OPEN) {
                pcStatus = "OPEN";
            } else if (pnatcb->NAT_iStatus == __NAT_STATUS_FIN) {
                pcStatus = "FIN";
            } else if (pnatcb->NAT_iStatus == __NAT_STATUS_CLOSING) {
                pcStatus = "CLOSING";
            } else {
                pcStatus = "?";
            }
            
            inet_ntoa_r(inaddr, cIpBuffer, INET_ADDRSTRLEN);
            printf("%-15s %10d %8d %-5s %9ld %-8s\n", cIpBuffer,
                                                      ntohs(pnatcb->NAT_usLocalPort),
                                                      ntohs(pnatcb->NAT_usAssPort),
                                                      "UDP",
                                                      pnatcb->NAT_ulIdleTimer,
                                                      pcStatus);
        }
    }
    for (plineTemp  = _G_plineNatcbIcmp;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
        
        pnatcb = _LIST_ENTRY(plineTemp, __NAT_CB, NAT_lineManage);
        if ((pipaddr->addr == INADDR_ANY) ||
            (pipaddr->addr == pnatcb->NAT_ipaddrLocalIp.addr)) {
            /*
             *  打印节点信息
             */
            struct in_addr  inaddr;
            CHAR            cIpBuffer[INET_ADDRSTRLEN];
            
            inaddr.s_addr = pnatcb->NAT_ipaddrLocalIp.addr;
            if (pnatcb->NAT_ucProto == IP_PROTO_ICMP) {
                pcProto = "ICMP";
            } else {
                pcProto = "?";
            }
            
            if (pnatcb->NAT_iStatus == __NAT_STATUS_OPEN) {
                pcStatus = "OPEN";
            } else if (pnatcb->NAT_iStatus == __NAT_STATUS_FIN) {
                pcStatus = "FIN";
            } else if (pnatcb->NAT_iStatus == __NAT_STATUS_CLOSING) {
                pcStatus = "CLOSING";
            } else {
                pcStatus = "?";
            }
            
            inet_ntoa_r(inaddr, cIpBuffer, INET_ADDRSTRLEN);
            printf("%-15s %10d %8d %-5s %9ld %-8s\n", cIpBuffer,
                                                      ntohs(pnatcb->NAT_usLocalPort),
                                                      ntohs(pnatcb->NAT_usAssPort),
                                                      pcProto,
                                                      pnatcb->NAT_ulIdleTimer,
                                                      pcStatus);
        }
    }
    __NAT_OP_UNLOCK();                                                  /*  解锁 NAT 链表               */

}
#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_NET_NAT_EN > 0       */
/*********************************************************************************************************
  END
*********************************************************************************************************/
