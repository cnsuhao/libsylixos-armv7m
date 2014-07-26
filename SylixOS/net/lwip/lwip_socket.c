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
** 文   件   名: lwip_socket.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2012 年 12 月 18 日
**
** 描        述: socket 接口. (使用 sylixos io 系统包裹 lwip 文件描述符)

** BUG:
2013.01.02  在 socket 结构中加入 errno 记录, SO_ERROR 统一在 socket 层处理.
2013.01.10  根据 POSIX 要求设置正确的取消点.
2013.01.20  修正关于 SO_ERROR 的一处错误:
            BSD socket 规定, 如果使用 NBIO 执行 connect 操作, 不管成功与否都会立即退出, 如果正在执行连接
            操作, 则 errno 为 EINPROGRESS. 这时应用程序使用 select 或者 poll 进行阻塞, 如果连接成功, 则
            这时通过 getsockopt 的 SO_ERROR 选项获取的结果应给为 0, 否则则为其他错误类型. 
            所以, 所有通信域通过 select 激活时, 都要提交最新的错误号, 用以更新 socket 当前记录的错误号.
2013.01.30  使用 hash 表, 加快 socket_event 速度.
2013.04.27  加入 hostbyaddr() 伪函数, 整个 netdb 交给外部 C 库实现.
2013.04.28  加入 __IfIoctl() 来模拟 BSD 系统 ifioctl 的部分功能.
2013.04.29  lwip_fcntl() 如果检测到 O_NONBLOCK 位外还有其他位, 则错误. 首先需要过滤.
2013.09.24  ioctl() 加入对 SIOCGSIZIFCONF 的支持.
            ioctl() 支持 IPv6 地址操作.
2013.11.17  加入对 SOCK_CLOEXEC 与 SOCK_NONBLOCK 的支持.
2013.11.21  加入 accept4() 函数.
2014.03.22  加入 AF_PACKET 支持.
2014.04.01  加入 socket 文件对 mmap 的支持.
2014.05.02  __SOCKET_CHECHK() 判断出错时打印 debug 信息.
            socket 加入 monitor 监控器功能.
            修正 __ifIoctl() 对 if_indextoname 加锁的错误.
2014.05.03  加入获取网络类型的接口.
2014.05.07  __ifIoctlIf() 优先使用 netif ioctl.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_NET_EN > 0
#include "sys/socket.h"
#include "sys/un.h"
#include "lwip/mem.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "net/if.h"
#include "net/if_dl.h"
#include "net/if_arp.h"
#include "net/if_type.h"
#include "netdb.h"
#include "lwip_if.h"
#include "./packet/af_packet.h"
#include "./unix/af_unix.h"
#if LW_CFG_NET_WIRELESS_EN > 0
#include "net/if_wireless.h"
#include "./wireless/lwip_wl.h"
#endif                                                                  /*  LW_CFG_NET_WIRELESS_EN > 0  */
/*********************************************************************************************************
  ipv6 extern vars
*********************************************************************************************************/
#if LWIP_IPV6
const struct in6_addr in6addr_any                = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback           = IN6ADDR_LOOPBACK_INIT;
const struct in6_addr in6addr_nodelocal_allnodes = IN6ADDR_NODELOCAL_ALLNODES_INIT;
const struct in6_addr in6addr_linklocal_allnodes = IN6ADDR_LINKLOCAL_ALLNODES_INIT;
#endif
/*********************************************************************************************************
  文件结构 (支持的协议簇 AF_INET AF_INET6 AF_RAW AF_UNIX)
*********************************************************************************************************/
typedef struct {
    LW_LIST_LINE        SOCK_lineManage;                                /*  管理链表                    */
    INT                 SOCK_iFamily;                                   /*  协议簇                      */
    
    union {
        INT             SOCKF_iLwipFd;                                  /*  lwip 文件描述符             */
        AF_UNIX_T      *SOCKF_pafunix;                                  /*  AF_UNIX 控制块              */
        AF_PACKET_T    *SOCKF_pafpacket;                                /*  AF_PACKET 控制块            */
    } SOCK_family;
    
    INT                 SOCK_iHash;                                     /*  hash 表下标                 */
    INT                 SOCK_iSoErr;                                    /*  最后一次错误                */
    LW_SEL_WAKEUPLIST   SOCK_selwulist;
} SOCKET_T;

#define SOCK_iLwipFd    SOCK_family.SOCKF_iLwipFd
#define SOCK_pafunix    SOCK_family.SOCKF_pafunix
#define SOCK_pafpacket  SOCK_family.SOCKF_pafpacket
/*********************************************************************************************************
  驱动声明
*********************************************************************************************************/
static LONG             __socketOpen(LW_DEV_HDR *pdevhdr, PCHAR  pcName, INT  iFlag, mode_t  mode);
static INT              __socketClose(SOCKET_T *psock);
static ssize_t          __socketRead(SOCKET_T *psock, PVOID  pvBuffer, size_t  stLen);
static ssize_t          __socketWrite(SOCKET_T *psock, CPVOID  pvBuffer, size_t  stLen);
static INT              __socketIoctl(SOCKET_T *psock, INT  iCmd, PVOID  pvArg);
static INT              __socketMmap(SOCKET_T *psock, PLW_DEV_MMAP_AREA  pdmap);
static INT              __socketUnmap(SOCKET_T *psock, PLW_DEV_MMAP_AREA  pdmap);
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
static LW_DEV_HDR           _G_devhdrSocket;
static LW_OBJECT_HANDLE     _G_hSockSelMutex;
static LW_OBJECT_HANDLE     _G_hSockMutex;

#define __SOCKET_HASH_SIZE  16
#define __SOCKET_HASH_MASK  (__SOCKET_HASH_SIZE - 1)
static LW_LIST_LINE_HEADER  _G_plineSocket[__SOCKET_HASH_SIZE];

#define __SOCKET_LOCK()     API_SemaphoreMPend(_G_hSockMutex, LW_OPTION_WAIT_INFINITE)
#define __SOCKET_UNLOCK()   API_SemaphoreMPost(_G_hSockMutex)
/*********************************************************************************************************
  lwip 与 unix 内部函数 (相关域实现代码只有在事件有效时才能更新 piSoErr)
*********************************************************************************************************/
extern int              __lwip_have_event(int s, int type, int *piSoErr);
extern int              __unix_have_event(AF_UNIX_T *pafunix, int type, int *piSoErr);
extern int              __packet_have_event(AF_PACKET_T *pafpacket, int type, int  *piSoErr);
/*********************************************************************************************************
  socket fd 有效性检查
*********************************************************************************************************/
#define __SOCKET_CHECHK()   if (psock == (SOCKET_T *)PX_ERROR) {    \
                                _ErrorHandle(EBADF);    \
                                return  (PX_ERROR); \
                            }   \
                            iosFdGetType(s, &iType);    \
                            if (iType != LW_DRV_TYPE_SOCKET) { \
                                _DebugHandle(__ERRORMESSAGE_LEVEL, "not a socket file.\r\n");   \
                                _ErrorHandle(EBADF);    \
                                return  (PX_ERROR); \
                            }
/*********************************************************************************************************
  socket fd hash 操作 (pafunix >> 4) 在 32 或者 64 位 CPU 时忽略低位 0 .
*********************************************************************************************************/
#define __SOCKET_LWIP_HASH(lwipfd)      (lwipfd & __SOCKET_HASH_MASK)
#define __SOCKET_UNIX_HASH(pafunix)     (((size_t)pafunix >> 4) & __SOCKET_HASH_MASK)
#define __SOCKET_PACKET_HASH(pafpacket) __SOCKET_UNIX_HASH(pafpacket)
/*********************************************************************************************************
** 函数名称: __lwip_socket_event
** 功能描述: lwip socket 产生事件
** 输　入  : lwipfd      lwip 文件
**           type        事件类型
**           iSoErr      更新的 SO_ERROR 数值
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  __lwip_socket_event (int  lwipfd, LW_SEL_TYPE type, INT  iSoErr)
{
    INT             iHash = __SOCKET_LWIP_HASH(lwipfd);
    PLW_LIST_LINE   plineTemp;
    SOCKET_T       *psock;

    __SOCKET_LOCK();
    for (plineTemp  = _G_plineSocket[iHash];
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
         
        psock = (SOCKET_T *)plineTemp;
        if (psock->SOCK_iLwipFd == lwipfd) {
            psock->SOCK_iSoErr = iSoErr;                                /*  更新 SO_ERROR               */
            SEL_WAKE_UP_ALL(&psock->SOCK_selwulist, type);
            break;
        }
    }
    __SOCKET_UNLOCK();
}
/*********************************************************************************************************
** 函数名称: __unix_socket_event
** 功能描述: unix socket 产生事件
** 输　入  : pafunix     控制块
**           type        事件类型
**           iSoErr      更新的 SO_ERROR 数值
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  __unix_socket_event (AF_UNIX_T  *pafunix, LW_SEL_TYPE type, INT  iSoErr)
{
    INT             iHash = __SOCKET_UNIX_HASH(pafunix);
    PLW_LIST_LINE   plineTemp;
    SOCKET_T       *psock;

    __SOCKET_LOCK();
    for (plineTemp  = _G_plineSocket[iHash];
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
         
        psock = (SOCKET_T *)plineTemp;
        if (psock->SOCK_pafunix == pafunix) {
            psock->SOCK_iSoErr = iSoErr;                                /*  更新 SO_ERROR               */
            SEL_WAKE_UP_ALL(&psock->SOCK_selwulist, type);
            break;
        }
    }
    __SOCKET_UNLOCK();
}
/*********************************************************************************************************
** 函数名称: __packet_socket_event
** 功能描述: packet socket 产生事件
** 输　入  : pafpacket   控制块
**           type        事件类型
**           iSoErr      更新的 SO_ERROR 数值
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  __packet_socket_event (AF_PACKET_T *pafpacket, LW_SEL_TYPE type, INT  iSoErr)
{
    INT             iHash = __SOCKET_PACKET_HASH(pafpacket);
    PLW_LIST_LINE   plineTemp;
    SOCKET_T       *psock;
    
    __SOCKET_LOCK();
    for (plineTemp  = _G_plineSocket[iHash];
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
         
        psock = (SOCKET_T *)plineTemp;
        if (psock->SOCK_pafpacket == pafpacket) {
            psock->SOCK_iSoErr = iSoErr;                                /*  更新 SO_ERROR               */
            SEL_WAKE_UP_ALL(&psock->SOCK_selwulist, type);
            break;
        }
    }
    __SOCKET_UNLOCK();
}
/*********************************************************************************************************
** 函数名称: __ifConfSize
** 功能描述: 获得网络接口列表保存所需要的内存大小
** 输　入  : piSize    所需内存大小
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __ifConfSize (INT  *piSize)
{
           INT       iNum = 0;
    struct netif    *pnetif;
    
    for (pnetif = netif_list; pnetif != LW_NULL; pnetif = pnetif->next) {
        iNum += sizeof(struct ifreq);
    }
    
    *piSize = iNum;
}
/*********************************************************************************************************
** 函数名称: __ifConf
** 功能描述: 获得网络接口列表操作
** 输　入  : pifconf   列表保存缓冲
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __ifConf (struct ifconf  *pifconf)
{
           INT           iSize;
           INT           iNum = 0;
    struct netif        *pnetif;
    struct ifreq        *pifreq;
    struct sockaddr_in  *psockaddrin;
    
    iSize = pifconf->ifc_len / sizeof(struct ifreq);                    /*  缓冲区个数                  */
    
    pifreq = pifconf->ifc_req;
    for (pnetif = netif_list; pnetif != LW_NULL; pnetif = pnetif->next) {
        if (iNum >= iSize) {
            break;
        }
        pifreq->ifr_name[0] = pnetif->name[0];
        pifreq->ifr_name[1] = pnetif->name[1];
        pifreq->ifr_name[2] = (char)(pnetif->num + '0');
        pifreq->ifr_name[3] = PX_EOS;
        
        psockaddrin = (struct sockaddr_in *)&(pifreq->ifr_addr);
        psockaddrin->sin_len    = sizeof(struct sockaddr_in);
        psockaddrin->sin_family = AF_INET;
        psockaddrin->sin_port   = 0;
        psockaddrin->sin_addr.s_addr = pnetif->ip_addr.addr;
        
        iNum++;
        pifreq++;
    }
    
    pifconf->ifc_len = iNum * sizeof(struct ifreq);                     /*  读取个数                    */
}
/*********************************************************************************************************
** 函数名称: __ifFindByName
** 功能描述: 通过接口名获取接口结构
** 输　入  : pcName    网络接口名
** 输　出  : 网络接口结构
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static struct netif *__ifFindByName (CPCHAR  pcName)
{
    struct netif    *pnetif;
    
    for (pnetif = netif_list; pnetif != LW_NULL; pnetif = pnetif->next) {  
        if ((pcName[0] == pnetif->name[0]) &&
            (pcName[1] == pnetif->name[1]) &&
            (pcName[2] == pnetif->num + '0')) {                         /*  匹配网络接口                */
            break;
        }
    }
    
    if (pnetif == LW_NULL) {
        return  (LW_NULL);
    
    } else {
        return  (pnetif);
    }
}
/*********************************************************************************************************
** 函数名称: __ifFindByName
** 功能描述: 通过 index 获取接口结构
** 输　入  : iIndex   网络结构 index
** 输　出  : 网络接口结构
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static struct netif *__ifFindByIndex (INT  iIndex)
{
    struct netif    *pnetif;
    
    for (pnetif = netif_list; pnetif != LW_NULL; pnetif = pnetif->next) {  
        if ((int)pnetif->num == iIndex) {
            break;
        }
    }
    
    if (pnetif == LW_NULL) {
        return  (LW_NULL);
    
    } else {
        return  (pnetif);
    }
}
/*********************************************************************************************************
** 函数名称: __ifReq6Size
** 功能描述: 获得网络接口 IPv6 地址数目
** 输　入  : ifreq6    ifreq6 请求控制块
** 输　出  : 处理结果
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __ifReq6Size (struct in6_ifreq  *pifreq6)
{
    INT              i;
    INT              iNum   = 0;
    struct netif    *pnetif = __ifFindByIndex(pifreq6->ifr6_ifindex);
    
    if (pnetif == LW_NULL) {
        _ErrorHandle(EADDRNOTAVAIL);                                    /*  未找到指定的网络接口        */
        return  (PX_ERROR);
    }
    
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_isvalid(pnetif->ip6_addr_state[i])) {
            iNum++;
        }
    }
    
    pifreq6->ifr6_len = iNum * sizeof(struct in6_ifr_addr);             /*  回写实际大小                */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __ifIoctlIf
** 功能描述: 网络接口 ioctl 操作 (针对网卡接口)
** 输　入  : iLwipFd   lwip 文件
**           iCmd      命令
**           pvArg     参数
** 输　出  : 处理结果
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __ifIoctlIf (INT iLwipFd, INT  iCmd, PVOID  pvArg)
{
    INT              iRet   = PX_ERROR;
    struct ifreq    *pifreq = LW_NULL;
    struct netif    *pnetif;
    
    pifreq = (struct ifreq *)pvArg;
    
    pnetif = __ifFindByName(pifreq->ifr_name);
    if (pnetif == LW_NULL) {
        _ErrorHandle(EADDRNOTAVAIL);                                    /*  未找到指定的网络接口        */
        return  (iRet);
    }
    
    if (pnetif->ioctl) {                                                /*  优先调用网卡驱动            */
        iRet = pnetif->ioctl(pnetif, iCmd, pvArg);
        if ((iRet == ERROR_NONE) || (errno != ENOSYS)) {
            return  (iRet);
        }
    }

    switch (iCmd) {
    
    case SIOCGIFFLAGS:                                                  /*  获取网卡 flag               */
        pifreq->ifr_flags = 0;
        if (pnetif->flags & NETIF_FLAG_UP) {
            pifreq->ifr_flags |= IFF_UP;
        }
        if (pnetif->flags & NETIF_FLAG_BROADCAST) {
            pifreq->ifr_flags |= IFF_BROADCAST;
        }
        if (pnetif->flags & NETIF_FLAG_POINTTOPOINT) {
            pifreq->ifr_flags |= IFF_POINTOPOINT;
        }
        if (pnetif->flags & NETIF_FLAG_LINK_UP) {
            pifreq->ifr_flags |= IFF_RUNNING;
        }
        if (pnetif->flags & NETIF_FLAG_IGMP) {
            pifreq->ifr_flags |= IFF_MULTICAST;
        }
        if ((pnetif->flags & NETIF_FLAG_ETHARP) == 0) {
            pifreq->ifr_flags |= IFF_NOARP;
        }
        if (pnetif->ip_addr.addr == ntohl(INADDR_LOOPBACK)) {
            pifreq->ifr_flags |= IFF_LOOPBACK;
        }
        iRet = ERROR_NONE;
        break;
        
    case SIOCSIFFLAGS:                                                  /*  设置网卡 flag               */
        if (pifreq->ifr_flags & IFF_UP) {
            netif_set_up(pnetif);                                       /*  只允许 up/down              */
        } else {
            netif_set_down(pnetif);
        }
        iRet = ERROR_NONE;
        break;
        
    case SIOCGIFTYPE:                                                   /*  获得网卡类型                */
        if (pnetif->flags & NETIF_FLAG_POINTTOPOINT) {
            pifreq->ifr_type = IFT_PPP;
        } else if (pnetif->flags & (NETIF_FLAG_ETHERNET | NETIF_FLAG_ETHARP)) {
            pifreq->ifr_type = IFT_ETHER;
        } else if (pnetif->num == 0) {
            pifreq->ifr_type = IFT_LOOP;
        } else {
            pifreq->ifr_type = IFT_OTHER;
        }
        iRet = ERROR_NONE;
        break;
        
    case SIOCGIFINDEX:                                                  /*  获得网卡 index              */
        pifreq->ifr_ifindex = (int)pnetif->num;
        iRet = ERROR_NONE;
        break;
        
    case SIOCGIFMTU:                                                    /*  获得网卡 mtu                */
        pifreq->ifr_mtu = pnetif->mtu;
        iRet = ERROR_NONE;
        break;
        
    case SIOCSIFMTU:                                                    /*  设置网卡 mtu                */
        _ErrorHandle(ENOSYS);
        break;
        
    case SIOCGIFHWADDR:                                                 /*  获得物理地址                */
        if (pnetif->flags & (NETIF_FLAG_ETHERNET | NETIF_FLAG_ETHARP)) {
            INT i;
            for (i = 0; i < IFHWADDRLEN; i++) {
                pifreq->ifr_hwaddr.sa_data[i] = pnetif->hwaddr[i];
            }
            pifreq->ifr_hwaddr.sa_family = ARPHRD_ETHER;
            iRet = ERROR_NONE;
        } else {
            _ErrorHandle(EINVAL);
        }
        break;
        
    case SIOCSIFHWADDR:                                                 /*  设置 mac 地址               */
        if (pnetif->flags & (NETIF_FLAG_ETHERNET | NETIF_FLAG_ETHARP)) {
            INT i;
            INT iIsUp = netif_is_up(pnetif);
            netif_set_down(pnetif);                                     /*  关闭网口                    */
            for (i = 0; i < IFHWADDRLEN; i++) {
                pnetif->hwaddr[i] = pifreq->ifr_hwaddr.sa_data[i];
            }
            if (iIsUp) {
                netif_set_up(pnetif);                                   /*  重启网口                    */
            }
            iRet = ERROR_NONE;
        } else {
            _ErrorHandle(EINVAL);
        }
        break;
    }
    
    return  iRet;
}
/*********************************************************************************************************
** 函数名称: __ifIoctl4
** 功能描述: 网络接口 ioctl 操作 (针对 ipv4)
** 输　入  : iLwipFd   lwip 文件
**           iCmd      命令
**           pvArg     参数
** 输　出  : 处理结果
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __ifIoctl4 (INT iLwipFd, INT  iCmd, PVOID  pvArg)
{
           INT           iRet   = PX_ERROR;
    struct ifreq        *pifreq = LW_NULL;
    struct netif        *pnetif;
    struct sockaddr_in  *psockaddrin;
    
    pifreq = (struct ifreq *)pvArg;
    
    pnetif = __ifFindByName(pifreq->ifr_name);
    if (pnetif == LW_NULL) {
        _ErrorHandle(EADDRNOTAVAIL);                                    /*  未找到指定的网络接口        */
        return  (iRet);
    }
    
    switch (iCmd) {                                                     /*  命令预处理                  */
    
    case SIOCGIFADDR:                                                   /*  获取地址操作                */
    case SIOCGIFNETMASK:
    case SIOCGIFDSTADDR:
    case SIOCGIFBRDADDR:
        psockaddrin = (struct sockaddr_in *)&(pifreq->ifr_addr);
        psockaddrin->sin_len    = sizeof(struct sockaddr_in);
        psockaddrin->sin_family = AF_INET;
        psockaddrin->sin_port   = 0;
        break;
        
    case SIOCSIFADDR:                                                   /*  设置地址操作                */
    case SIOCSIFNETMASK:
    case SIOCSIFDSTADDR:
    case SIOCSIFBRDADDR:
        psockaddrin = (struct sockaddr_in *)&(pifreq->ifr_addr);
        break;
    }
    
    switch (iCmd) {                                                     /*  命令处理器                  */
        
    case SIOCGIFADDR:                                                   /*  获取网卡 IP                 */
        psockaddrin->sin_addr.s_addr = pnetif->ip_addr.addr;
        iRet = ERROR_NONE;
        break;
    
    case SIOCGIFNETMASK:                                                /*  获取网卡 mask               */
        psockaddrin->sin_addr.s_addr = pnetif->netmask.addr;
        iRet = ERROR_NONE;
        break;
        
    case SIOCGIFDSTADDR:                                                /*  获取网卡目标地址            */
        if (pnetif->flags & NETIF_FLAG_POINTTOPOINT) {
            psockaddrin->sin_addr.s_addr = INADDR_ANY;
            iRet = ERROR_NONE;
        } else {
            _ErrorHandle(EINVAL);
        }
        break;
        
    case SIOCGIFBRDADDR:                                                /*  获取网卡广播地址            */
        if (pnetif->flags & NETIF_FLAG_BROADCAST) {
            psockaddrin->sin_addr.s_addr = (pnetif->ip_addr.addr | (~pnetif->netmask.addr));
            iRet = ERROR_NONE;
        } else {
            _ErrorHandle(EINVAL);
        }
        break;
        
    case SIOCSIFADDR:                                                   /*  设置网卡地址                */
        if (psockaddrin->sin_family == AF_INET) {
            ip_addr_t ipaddr;
            ipaddr.addr = psockaddrin->sin_addr.s_addr;
            netif_set_ipaddr(pnetif, &ipaddr);
        } else if (psockaddrin->sin_family == AF_INET6) {
            /*
             *  TODO: ipv6
             */
        }
        iRet = ERROR_NONE;
        break;
        
    case SIOCSIFNETMASK:                                                /*  设置网卡掩码                */
        if (psockaddrin->sin_family == AF_INET) {
            ip_addr_t ipaddr;
            ipaddr.addr = psockaddrin->sin_addr.s_addr;
            netif_set_netmask(pnetif, &ipaddr);
        } else if (psockaddrin->sin_family == AF_INET6) {
            /*
             *  TODO: ipv6
             */
        }
        iRet = ERROR_NONE;
        break;
        
    case SIOCSIFDSTADDR:                                                /*  设置网卡目标地址            */
        if (pnetif->flags & NETIF_FLAG_POINTTOPOINT) {
            iRet = ERROR_NONE;
        } else {
            _ErrorHandle(EINVAL);
        }
        break;
        
    case SIOCSIFBRDADDR:                                                /*  设置网卡广播地址            */
        if (pnetif->flags & NETIF_FLAG_BROADCAST) {
            iRet = ERROR_NONE;
        } else {
            _ErrorHandle(EINVAL);
        }
        break;
    }
    
    return  iRet;
}
/*********************************************************************************************************
** 函数名称: __ifIoctl6
** 功能描述: 网络接口 ioctl 操作 (针对 ipv6)
** 输　入  : iLwipFd   lwip 文件
**           iCmd      命令
**           pvArg     参数
** 输　出  : 处理结果
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __ifIoctl6 (INT iLwipFd, INT  iCmd, PVOID  pvArg)
{
#define __LWIP_GET_IPV6_FROM_NETIF() \
        pifr6addr->ifr6a_addr.un.u32_addr[0] = pnetif->ip6_addr[i].addr[0]; \
        pifr6addr->ifr6a_addr.un.u32_addr[1] = pnetif->ip6_addr[i].addr[1]; \
        pifr6addr->ifr6a_addr.un.u32_addr[2] = pnetif->ip6_addr[i].addr[2]; \
        pifr6addr->ifr6a_addr.un.u32_addr[3] = pnetif->ip6_addr[i].addr[3];
        
#define __LWIP_SET_IPV6_TO_NETIF() \
        pnetif->ip6_addr[i].addr[0] = pifr6addr->ifr6a_addr.un.u32_addr[0]; \
        pnetif->ip6_addr[i].addr[1] = pifr6addr->ifr6a_addr.un.u32_addr[1]; \
        pnetif->ip6_addr[i].addr[2] = pifr6addr->ifr6a_addr.un.u32_addr[2]; \
        pnetif->ip6_addr[i].addr[3] = pifr6addr->ifr6a_addr.un.u32_addr[3]; 
        
#define __LWIP_CMP_IPV6_WITH_NETIF() \
        (pnetif->ip6_addr[i].addr[0] == pifr6addr->ifr6a_addr.un.u32_addr[0]) && \
        (pnetif->ip6_addr[i].addr[1] == pifr6addr->ifr6a_addr.un.u32_addr[1]) && \
        (pnetif->ip6_addr[i].addr[2] == pifr6addr->ifr6a_addr.un.u32_addr[2]) && \
        (pnetif->ip6_addr[i].addr[3] == pifr6addr->ifr6a_addr.un.u32_addr[3]) 

           INT           i;
           INT           iSize;
           INT           iNum = 0;
           INT           iRet    = PX_ERROR;
    struct in6_ifreq    *pifreq6 = LW_NULL;
    struct in6_ifr_addr *pifr6addr;
    struct netif        *pnetif;
    
    pifreq6 = (struct in6_ifreq *)pvArg;
    
    pnetif = __ifFindByIndex(pifreq6->ifr6_ifindex);
    if (pnetif == LW_NULL) {
        _ErrorHandle(EADDRNOTAVAIL);                                    /*  未找到指定的网络接口        */
        return  (iRet);
    }
    
    iSize = pifreq6->ifr6_len / sizeof(struct in6_ifr_addr);            /*  缓冲区个数                  */
    pifr6addr = pifreq6->ifr6_addr_array;
    
    switch (iCmd) {                                                     /*  命令处理器                  */
    
    case SIOCGIFADDR6:                                                  /*  获取网卡 IP                 */
        for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
            if (iNum >= iSize) {
                break;
            }
            if (ip6_addr_isvalid(pnetif->ip6_addr_state[i])) {
                __LWIP_GET_IPV6_FROM_NETIF();
                if (ip6_addr_isloopback(&pnetif->ip6_addr[i])) {
                    pifr6addr->ifr6a_prefixlen = 128;
                } else if (ip6_addr_islinklocal(&pnetif->ip6_addr[i])) {
                    pifr6addr->ifr6a_prefixlen = 6;
                } else {
                    pifr6addr->ifr6a_prefixlen = 64;                    /*  TODO: 目前无法获取          */
                }
                iNum++;
                pifr6addr++;
            }
        }
        pifreq6->ifr6_len = iNum * sizeof(struct in6_ifr_addr);         /*  回写实际大小                */
        iRet = ERROR_NONE;
        break;
        
    case SIOCSIFADDR6:                                                  /*  获取网卡 IP                 */
        if (iSize != 1) {                                               /*  每次只能设置一个 IP 地址    */
            _ErrorHandle(EOPNOTSUPP);
            break;
        }
        if (IN6_IS_ADDR_LOOPBACK(&pifr6addr->ifr6a_addr) ||
            IN6_IS_ADDR_LINKLOCAL(&pifr6addr->ifr6a_addr)) {            /*  不能手动设置这两种类型的地址*/
            _ErrorHandle(EADDRNOTAVAIL);
            break;
        }
        for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {                 /*  首先试图添加                */
            if (ip6_addr_isinvalid(pnetif->ip6_addr_state[i])) {
                __LWIP_SET_IPV6_TO_NETIF();
                pnetif->ip6_addr_state[i] = IP6_ADDR_VALID | IP6_ADDR_TENTATIVE;
                break;
            }
        }
        if (i >= LWIP_IPV6_NUM_ADDRESSES) {                             /*  无法添加                    */
            for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {             /*  优先覆盖暂定地址            */
                if (!ip6_addr_islinklocal(&pnetif->ip6_addr[i]) &&
                    ip6_addr_istentative(pnetif->ip6_addr_state[i])) {
                    __LWIP_SET_IPV6_TO_NETIF();
                    pnetif->ip6_addr_state[i] = IP6_ADDR_VALID | IP6_ADDR_TENTATIVE;
                    break;
                }
            }
        }
        if (i >= LWIP_IPV6_NUM_ADDRESSES) {                             /*  无法添加                    */
            for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {             /*  替换非 linklocal 地址       */
                if (!ip6_addr_islinklocal(&pnetif->ip6_addr[i])) {
                    __LWIP_SET_IPV6_TO_NETIF();
                    pnetif->ip6_addr_state[i] = IP6_ADDR_VALID | IP6_ADDR_TENTATIVE;
                    break;
                }
            }
        }
        iRet = ERROR_NONE;
        break;
        
    case SIOCDIFADDR6:                                                  /*  删除一个 IPv6 地址          */
        if (iSize != 1) {                                               /*  每次只能设置一个 IP 地址    */
            _ErrorHandle(EOPNOTSUPP);
            break;
        }
        for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
            if (ip6_addr_isvalid(pnetif->ip6_addr_state[i])) {
                if (__LWIP_CMP_IPV6_WITH_NETIF()) {                     /*  TODO 没有判断前缀长度       */
                    pnetif->ip6_addr_state[i] = IP6_ADDR_INVALID;
                    break;
                }
            }
        }
        iRet = ERROR_NONE;
        break;
        
    default:
        _ErrorHandle(ENOSYS);                                           /*  TODO: 其他命令暂未实现      */
        break;
    }
    
    return  iRet;
}
/*********************************************************************************************************
** 函数名称: __ifIoctl
** 功能描述: 网络接口 ioctl 操作
** 输　入  : iLwipFd   lwip 文件
**           iCmd      命令
**           pvArg     参数
** 输　出  : 处理结果
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __ifIoctl (INT iLwipFd, INT  iCmd, PVOID  pvArg)
{
    INT     iRet = PX_ERROR;
    
    if (pvArg == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (iRet);
    }
    
    if (iCmd == SIOCGIFCONF) {                                          /*  获得网卡列表                */
        struct ifconf *pifconf = (struct ifconf *)pvArg;
        
        LWIP_NETIF_LOCK();                                              /*  进入临界区                  */
        __ifConf(pifconf);
        LWIP_NETIF_UNLOCK();                                            /*  退出临界区                  */
        return  (ERROR_NONE);
        
    } else if (iCmd == SIOCGSIZIFCONF) {                                /*  SIOCGIFCONF 所需缓冲大小    */
        INT  *piSize = (INT *)pvArg;
        
        LWIP_NETIF_LOCK();                                              /*  进入临界区                  */
        __ifConfSize(piSize);
        LWIP_NETIF_UNLOCK();                                            /*  退出临界区                  */
        return  (ERROR_NONE);
        
    } else if (iCmd == SIOCGSIZIFREQ6) {                                /*  获得指定网口 ipv6 地址数量  */
        struct in6_ifreq *pifreq6 = (struct in6_ifreq *)pvArg;
        
        LWIP_NETIF_LOCK();                                              /*  进入临界区                  */
        iRet = __ifReq6Size(pifreq6);
        LWIP_NETIF_UNLOCK();                                            /*  退出临界区                  */
        return  (iRet);
        
    } else if (iCmd == SIOCGIFNAME) {                                   /*  获得网卡名                  */
        struct ifreq *pifreq = (struct ifreq *)pvArg;
        
        if (if_indextoname(pifreq->ifr_ifindex, pifreq->ifr_name)) {
            iRet = ERROR_NONE;
        }
        return  (iRet);
    }
    
    switch (iCmd) {                                                     /*  命令预处理                  */
    
    case SIOCSIFFLAGS:                                                  /*  基本网络接口操作            */
    case SIOCGIFFLAGS:
    case SIOCGIFTYPE:
    case SIOCGIFINDEX:
    case SIOCGIFMTU:
    case SIOCSIFMTU:
    case SIOCGIFHWADDR:
    case SIOCSIFHWADDR:
        LWIP_NETIF_LOCK();                                              /*  进入临界区                  */
        iRet = __ifIoctlIf(iLwipFd, iCmd, pvArg);
        LWIP_NETIF_UNLOCK();                                            /*  退出临界区                  */
        break;
    
    case SIOCGIFADDR:                                                   /*  ipv4 操作                   */
    case SIOCGIFNETMASK:
    case SIOCGIFDSTADDR:
    case SIOCGIFBRDADDR:
    case SIOCSIFADDR:
    case SIOCSIFNETMASK:
    case SIOCSIFDSTADDR:
    case SIOCSIFBRDADDR:
        LWIP_NETIF_LOCK();                                              /*  进入临界区                  */
        iRet = __ifIoctl4(iLwipFd, iCmd, pvArg);
        LWIP_NETIF_UNLOCK();                                            /*  退出临界区                  */
        break;
        
    case SIOCGIFADDR6:                                                  /*  ipv6 操作                   */
    case SIOCGIFNETMASK6:
    case SIOCGIFDSTADDR6:
    case SIOCSIFADDR6:
    case SIOCSIFNETMASK6:
    case SIOCSIFDSTADDR6:
    case SIOCDIFADDR6:
        LWIP_NETIF_LOCK();                                              /*  进入临界区                  */
        iRet = __ifIoctl6(iLwipFd, iCmd, pvArg);
        LWIP_NETIF_UNLOCK();                                            /*  退出临界区                  */
        break;
    
    default:
#if LW_CFG_NET_WIRELESS_EN > 0
        if ((iCmd >= SIOCIWFIRST) &&
            (iCmd <= SIOCIWLASTPRIV)) {                                 /*  无线连接设置                */
            LWIP_NETIF_LOCK();                                          /*  进入临界区                  */
            iRet = wext_handle_ioctl(iCmd, (struct ifreq *)pvArg);
            LWIP_NETIF_UNLOCK();                                        /*  退出临界区                  */
            if (iRet) {
                _ErrorHandle(lib_abs(iRet));
                iRet = PX_ERROR;
            }
        } else
#endif                                                                  /*  LW_CFG_NET_WIRELESS_EN > 0  */
        {
            _ErrorHandle(ENOSYS);
        }
        break;
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: __socketInit
** 功能描述: 初始化 sylixos socket 系统
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __socketInit (VOID)
{
    static INT              iDrv = 0;
    struct file_operations  fileop;
    
    if (iDrv > 0) {
        return;
    }
    
    lib_bzero(&fileop, sizeof(struct file_operations));
    
    fileop.owner       = THIS_MODULE;
    fileop.fo_create   = __socketOpen;
    fileop.fo_release  = LW_NULL;
    fileop.fo_open     = __socketOpen;
    fileop.fo_close    = __socketClose;
    fileop.fo_read     = __socketRead;
    fileop.fo_write    = __socketWrite;
    fileop.fo_ioctl    = __socketIoctl;
    fileop.fo_mmap     = __socketMmap;
    fileop.fo_unmap    = __socketUnmap;
    
    iDrv = iosDrvInstallEx2(&fileop, LW_DRV_TYPE_SOCKET);
    if (iDrv < 0) {
        return;
    }
    
    DRIVER_LICENSE(iDrv,     "Dual BSD/GPL->Ver 1.0");
    DRIVER_AUTHOR(iDrv,      "SICS");
    DRIVER_DESCRIPTION(iDrv, "lwip socket driver v2.0");
    
    iosDevAddEx(&_G_devhdrSocket, LWIP_SYLIXOS_SOCKET_NAME, iDrv, DT_SOCK);
    
    _G_hSockMutex = API_SemaphoreMCreate("socket_lock", LW_PRIO_DEF_CEILING, 
                                         LW_OPTION_WAIT_FIFO | LW_OPTION_DELETE_SAFE |
                                         LW_OPTION_INHERIT_PRIORITY | LW_OPTION_OBJECT_GLOBAL,
                                         LW_NULL);
    
    _G_hSockSelMutex = API_SemaphoreMCreate("socksel_lock", LW_PRIO_DEF_CEILING, 
                                            LW_OPTION_WAIT_FIFO | LW_OPTION_DELETE_SAFE |
                                            LW_OPTION_INHERIT_PRIORITY | LW_OPTION_OBJECT_GLOBAL,
                                            LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __socketOpen
** 功能描述: socket open 操作
** 输　入  : pdevhdr   设备
**           pcName    名字
**           iFlag     选项
**           mode      未使用
** 输　出  : socket 结构
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static LONG  __socketOpen (LW_DEV_HDR *pdevhdr, PCHAR  pcName, INT  iFlag, mode_t  mode)
{
    SOCKET_T    *psock;
    
    psock = (SOCKET_T *)__SHEAP_ALLOC(sizeof(SOCKET_T));
    if (psock == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (PX_ERROR);
    }
    
    psock->SOCK_iFamily = AF_UNSPEC;
    psock->SOCK_iLwipFd = PX_ERROR;
    psock->SOCK_iHash   = PX_ERROR;
    
    lib_bzero(&psock->SOCK_selwulist, sizeof(LW_SEL_WAKEUPLIST));
    psock->SOCK_selwulist.SELWUL_hListLock = _G_hSockSelMutex;
    
    return  ((LONG)psock);
}
/*********************************************************************************************************
** 函数名称: __socketClose
** 功能描述: socket close 操作
** 输　入  : psock     socket 结构
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __socketClose (SOCKET_T *psock)
{
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        if (psock->SOCK_pafunix) {
            unix_close(psock->SOCK_pafunix);
        }
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        if (psock->SOCK_pafpacket) {
            packet_close(psock->SOCK_pafpacket);
        }
        break;
        
    default:                                                            /*  其他使用 lwip               */
        if (psock->SOCK_iLwipFd >= 0) {
            lwip_close(psock->SOCK_iLwipFd);
        }
        break;
    }
    
    SEL_WAKE_UP_TERM(&psock->SOCK_selwulist);
    
    __SOCKET_LOCK();
    if (psock->SOCK_iHash >= 0) {
        _List_Line_Del(&psock->SOCK_lineManage, &_G_plineSocket[psock->SOCK_iHash]);
    }
    __SOCKET_UNLOCK();
    
    __SHEAP_FREE(psock);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __socketClose
** 功能描述: socket read 操作
** 输　入  : psock     socket 结构
**           pvBuffer  读数据缓冲
**           stLen     缓冲区大小
** 输　出  : 数据个数
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static ssize_t  __socketRead (SOCKET_T *psock, PVOID  pvBuffer, size_t  stLen)
{
    ssize_t     sstNum = 0;

    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        if (psock->SOCK_pafunix) {
            sstNum = unix_recvfrom(psock->SOCK_pafunix, pvBuffer, stLen, 0, LW_NULL, LW_NULL);
        }
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        if (psock->SOCK_pafpacket) {
            sstNum = packet_recvfrom(psock->SOCK_pafpacket, pvBuffer, stLen, 0, LW_NULL, LW_NULL);
        }
        break;
        
    default:                                                            /*  其他使用 lwip               */
        if (psock->SOCK_iLwipFd >= 0) {
            sstNum = lwip_read(psock->SOCK_iLwipFd, pvBuffer, stLen);
        }
        break;
    }
    
    return  (sstNum);
}
/*********************************************************************************************************
** 函数名称: __socketClose
** 功能描述: socket write 操作
** 输　入  : psock     socket 结构
**           pvBuffer  写数据缓冲
**           stLen     写数据大小
** 输　出  : 数据个数
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static ssize_t  __socketWrite (SOCKET_T *psock, CPVOID  pvBuffer, size_t  stLen)
{
    ssize_t     sstNum = 0;

    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        if (psock->SOCK_pafunix) {
            sstNum = unix_sendto(psock->SOCK_pafunix, pvBuffer, stLen, 0, LW_NULL, 0);
        }
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        if (psock->SOCK_pafpacket) {
            sstNum = packet_sendto(psock->SOCK_pafpacket, pvBuffer, stLen, 0, LW_NULL, 0);
        }
        break;
        
    default:                                                            /*  其他使用 lwip               */
        if (psock->SOCK_iLwipFd >= 0) {
            sstNum = lwip_write(psock->SOCK_iLwipFd, pvBuffer, stLen);
        }
        break;
    }
    
    return  (sstNum);
}
/*********************************************************************************************************
** 函数名称: __socketClose
** 功能描述: socket ioctl 操作
** 输　入  : psock     socket 结构
**           iCmd      命令
**           pvArg     参数
** 输　出  : 数据个数
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __socketIoctl (SOCKET_T *psock, INT  iCmd, PVOID  pvArg)
{
           INT                 iRet = PX_ERROR;
    struct stat               *pstatGet;
           PLW_SEL_WAKEUPNODE  pselwun;
           
    if (iCmd == FIOFSTATGET) {
        pstatGet = (struct stat *)pvArg;
        pstatGet->st_dev     = (dev_t)&_G_devhdrSocket;
        pstatGet->st_ino     = (ino_t)0;                                /*  相当于唯一节点              */
        pstatGet->st_mode    = 0666 | S_IFSOCK;
        pstatGet->st_nlink   = 1;
        pstatGet->st_uid     = 0;
        pstatGet->st_gid     = 0;
        pstatGet->st_rdev    = 1;
        pstatGet->st_size    = 0;
        pstatGet->st_blksize = 0;
        pstatGet->st_blocks  = 0;
        pstatGet->st_atime   = API_RootFsTime(LW_NULL);
        pstatGet->st_mtime   = API_RootFsTime(LW_NULL);
        pstatGet->st_ctime   = API_RootFsTime(LW_NULL);
        return  (ERROR_NONE);
    }
    
    switch (psock->SOCK_iFamily) {
    
    case AF_UNSPEC:                                                     /*  无效                        */
        _ErrorHandle(ENOSYS);
        break;
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        if (psock->SOCK_pafunix) {
            switch (iCmd) {
            
            case FIOSELECT:
                pselwun = (PLW_SEL_WAKEUPNODE)pvArg;
                SEL_WAKE_NODE_ADD(&psock->SOCK_selwulist, pselwun);
                if (__unix_have_event(psock->SOCK_pafunix, 
                                      pselwun->SELWUN_seltypType,
                                      &psock->SOCK_iSoErr)) {
                    SEL_WAKE_UP(pselwun);
                }
                iRet = ERROR_NONE;
                break;
                
            case FIOUNSELECT:
                SEL_WAKE_NODE_DELETE(&psock->SOCK_selwulist, (PLW_SEL_WAKEUPNODE)pvArg);
                iRet = ERROR_NONE;
                break;
                
            default:
                iRet = unix_ioctl(psock->SOCK_pafunix, (long)iCmd, pvArg);
                break;
            }
        }
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        if (psock->SOCK_pafpacket) {
            switch (iCmd) {
            
            case FIOSELECT:
                pselwun = (PLW_SEL_WAKEUPNODE)pvArg;
                SEL_WAKE_NODE_ADD(&psock->SOCK_selwulist, pselwun);
                if (__packet_have_event(psock->SOCK_pafpacket, 
                                        pselwun->SELWUN_seltypType,
                                        &psock->SOCK_iSoErr)) {
                    SEL_WAKE_UP(pselwun);
                }
                iRet = ERROR_NONE;
                break;
                
            case FIOUNSELECT:
                SEL_WAKE_NODE_DELETE(&psock->SOCK_selwulist, (PLW_SEL_WAKEUPNODE)pvArg);
                iRet = ERROR_NONE;
                break;
                
            default:
                iRet = packet_ioctl(psock->SOCK_pafpacket, (long)iCmd, pvArg);
                break;
            }
        }
        break;
        
    default:                                                            /*  其他使用 lwip               */
        if (psock->SOCK_iLwipFd >= 0) {
            switch (iCmd) {
            
            case FIOSELECT:
                pselwun = (PLW_SEL_WAKEUPNODE)pvArg;
                SEL_WAKE_NODE_ADD(&psock->SOCK_selwulist, pselwun);
                if (__lwip_have_event(psock->SOCK_iLwipFd, 
                                      pselwun->SELWUN_seltypType,
                                      &psock->SOCK_iSoErr)) {
                    SEL_WAKE_UP(pselwun);
                }
                iRet = ERROR_NONE;
                break;
            
            case FIOUNSELECT:
                SEL_WAKE_NODE_DELETE(&psock->SOCK_selwulist, (PLW_SEL_WAKEUPNODE)pvArg);
                iRet = ERROR_NONE;
                break;
                
            case FIOGETFL:
                if (pvArg) {
                    *(int *)pvArg  = lwip_fcntl(psock->SOCK_iLwipFd, F_GETFL, 0);
                    *(int *)pvArg |= O_RDWR;
                }
                iRet = ERROR_NONE;
                break;
                
            case FIOSETFL:
                {
                    INT iIsNonBlk = (INT)((INT)pvArg & O_NONBLOCK);     /*  其他位不能存在              */
                    iRet = lwip_fcntl(psock->SOCK_iLwipFd, F_SETFL, iIsNonBlk);
                }
                break;
                
            case FIONREAD:
                if (pvArg) {
                    *(INT *)pvArg = 0;
                }
                iRet = lwip_ioctl(psock->SOCK_iLwipFd, (long)iCmd, pvArg);
                break;
                
            case SIOCGSIZIFCONF:
            case SIOCGIFCONF:
            case SIOCSIFADDR:
            case SIOCSIFNETMASK:
            case SIOCSIFDSTADDR:
            case SIOCSIFBRDADDR:
            case SIOCSIFFLAGS:
            case SIOCGIFADDR:
            case SIOCGIFNETMASK:
            case SIOCGIFDSTADDR:
            case SIOCGIFBRDADDR:
            case SIOCGIFFLAGS:
            case SIOCGIFTYPE:
            case SIOCGIFNAME:
            case SIOCGIFINDEX:
            case SIOCGIFMTU:
            case SIOCSIFMTU:
            case SIOCGIFHWADDR:
            case SIOCSIFHWADDR:
            case SIOCGSIZIFREQ6:
            case SIOCSIFADDR6:
            case SIOCSIFNETMASK6:
            case SIOCSIFDSTADDR6:
            case SIOCGIFADDR6:
            case SIOCGIFNETMASK6:
            case SIOCGIFDSTADDR6:
            case SIOCDIFADDR6:
                iRet = __ifIoctl(psock->SOCK_iLwipFd, iCmd, pvArg);
                break;
            
            default:
#if LW_CFG_NET_WIRELESS_EN > 0
                if ((iCmd >= SIOCIWFIRST) &&
                    (iCmd <= SIOCIWLASTPRIV)) {                         /*  无线连接设置                */
                    iRet = __ifIoctl(psock->SOCK_iLwipFd, iCmd, pvArg);
                } else 
#endif                                                                  /*  LW_CFG_NET_WIRELESS_EN > 0  */
                {
                    iRet = lwip_ioctl(psock->SOCK_iLwipFd, (long)iCmd, pvArg);
                }
                break;
            }
        }
        break;
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: __socketMmap
** 功能描述: socket mmap 操作
** 输　入  : psock         socket 结构
**           pdmap         虚拟空间信息
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __socketMmap (SOCKET_T *psock, PLW_DEV_MMAP_AREA  pdmap)
{
    INT     iRet;

    if (!pdmap) {
        return  (PX_ERROR);
    }

    switch (psock->SOCK_iFamily) {
        
    case AF_PACKET:                                                     /*  PACKET                      */
        if (psock->SOCK_pafpacket) {
            iRet = packet_mmap(psock->SOCK_pafpacket, pdmap);
        } else {
            iRet = PX_ERROR;
        }
        break;
        
    default:
        _ErrorHandle(ENOTSUP);
        iRet = PX_ERROR;
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: __socketUnmap
** 功能描述: socket unmap 操作
** 输　入  : psock         socket 结构
**           pdmap         虚拟空间信息
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __socketUnmap (SOCKET_T *psock, PLW_DEV_MMAP_AREA  pdmap)
{
    INT     iRet;

    if (!pdmap) {
        return  (PX_ERROR);
    }

    switch (psock->SOCK_iFamily) {
        
    case AF_PACKET:                                                     /*  PACKET                      */
        if (psock->SOCK_pafpacket) {
            iRet = packet_unmap(psock->SOCK_pafpacket, pdmap);
        } else {
            iRet = PX_ERROR;
        }
        break;
        
    default:
        _ErrorHandle(ENOTSUP);
        iRet = PX_ERROR;
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: lwip_sendmsg
** 功能描述: lwip sendmsg
** 输　入  : lwipfd      lwip 文件
**           msg         消息
**           flags       flag
** 输　出  : 发送数据长度
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static ssize_t  lwip_sendmsg (int  s, const struct msghdr *msg, int flags)
{
	if (msg->msg_iovlen == 1) {
		return  (lwip_sendto(s, msg->msg_iov->iov_base, msg->msg_iov->iov_len, flags,
				             (const struct sockaddr *)msg->msg_name, msg->msg_namelen));
	
	} else {
	    struct iovec    liovec,*msg_iov;
		size_t          msg_iovlen;
		unsigned int    i, totalsize;
		ssize_t         size;
		char           *lbuf;
		char           *temp;
		
		msg_iov    = msg->msg_iov;
		msg_iovlen = msg->msg_iovlen;
		
		for (i = 0, totalsize = 0; i < msg_iovlen; i++) {
		    if ((msg_iov[i].iov_len == 0) || (msg_iov[i].iov_base == LW_NULL)) {
		        _ErrorHandle(EINVAL);
		        return  (PX_ERROR);
		    }
			totalsize += (unsigned int)msg_iov[i].iov_len;
		}
		
		lbuf = (char *)mem_malloc(totalsize);
        if (lbuf == LW_NULL) {
            _ErrorHandle(ENOMEM);
            return  (PX_ERROR);
        }
		
		liovec.iov_base = (PVOID)lbuf;
		liovec.iov_len  = (size_t)totalsize;
		
		size = totalsize;
		
		temp = lbuf;
		for (i = 0; size > 0 && i < msg_iovlen; i++) {
			int     qty = msg_iov[i].iov_len;
			lib_memcpy(temp, msg_iov[i].iov_base, qty);
			temp += qty;
			size -= qty;
		}
		
		size = lwip_sendto(s, liovec.iov_base, liovec.iov_len, flags, 
		                   (const struct sockaddr *)msg->msg_name, msg->msg_namelen);
		                   
        mem_free(lbuf);
		
		return  (size);
	}
}
/*********************************************************************************************************
** 函数名称: lwip_recvmsg
** 功能描述: lwip recvmsg
** 输　入  : lwipfd      lwip 文件
**           msg         消息
**           flags       flag
** 输　出  : 接收的数据长度
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static ssize_t  lwip_recvmsg (int  s, struct msghdr *msg, int flags)
{
    msg->msg_controllen = 0;
	
	if (msg->msg_iovlen == 1) {
		return  (lwip_recvfrom(s, msg->msg_iov->iov_base, msg->msg_iov->iov_len, flags,
				               (struct sockaddr *)msg->msg_name, &msg->msg_namelen));
    
    } else {
        struct iovec    liovec, *msg_iov;
		size_t          msg_iovlen;
		unsigned int    i, totalsize;
		ssize_t         size;
		char           *lbuf;
		char           *temp;
		
		msg_iov    = msg->msg_iov;
		msg_iovlen = msg->msg_iovlen;
		
		for (i = 0, totalsize = 0; i < msg_iovlen; i++) {
		    if ((msg_iov[i].iov_len == 0) || (msg_iov[i].iov_base == LW_NULL)) {
		        _ErrorHandle(EINVAL);
		        return  (PX_ERROR);
		    }
			totalsize += (unsigned int)msg_iov[i].iov_len;
        }
        
        lbuf = (char *)mem_malloc(totalsize);
        if (lbuf == LW_NULL) {
            _ErrorHandle(ENOMEM);
            return  (PX_ERROR);
        }
        
		liovec.iov_base = (PVOID)lbuf;
		liovec.iov_len  = (size_t)totalsize;
		
		size = lwip_recvfrom(s, liovec.iov_base, liovec.iov_len, flags, 
		                     (struct sockaddr *)msg->msg_name, &msg->msg_namelen);
		
		temp = lbuf;
		for (i = 0; size > 0 && i < msg_iovlen; i++) {
			size_t   qty = (size_t)((size > msg_iov[i].iov_len) ? msg_iov[i].iov_len : size);
			lib_memcpy(msg_iov[i].iov_base, temp, qty);
			temp += qty;
			size -= qty;
		}
		
		mem_free(lbuf);
		
		return  (size);
    }
}
/*********************************************************************************************************
** 函数名称: socketpair
** 功能描述: BSD socketpair
** 输　入  : domain        域
**           type          类型
**           protocol      协议
**           sv            返回两个成对文件描述符
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
int  socketpair (int domain, int type, int protocol, int sv[2])
{
    INT          iError;
    SOCKET_T    *psock[2];
    
    if (!sv) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    if (domain != AF_UNIX) {                                            /*  仅支持 unix 域协议          */
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    sv[0] = socket(AF_UNIX, type, protocol);                            /*  创建 socket                 */
    if (sv[0] < 0) {
        return  (PX_ERROR);
    }
    
    sv[1] = socket(AF_UNIX, type, protocol);                            /*  创建第二个 socket           */
    if (sv[1] < 0) {
        close(sv[0]);
        return  (PX_ERROR);
    }
    
    psock[0] = (SOCKET_T *)iosFdValue(sv[0]);
    psock[1] = (SOCKET_T *)iosFdValue(sv[1]);
    
    __KERNEL_SPACE_ENTER();
    iError = unix_connect2(psock[0]->SOCK_pafunix, psock[1]->SOCK_pafunix);
    __KERNEL_SPACE_EXIT();
    
    if (iError < 0) {
        close(sv[0]);
        close(sv[1]);
        return  (PX_ERROR);
    }
    
    MONITOR_EVT_INT5(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_SOCKPAIR, 
                     domain, type, protocol, sv[0], sv[1], LW_NULL);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: socket
** 功能描述: BSD socket
** 输　入  : domain    协议域
**           type      类型
**           protocol  协议
** 输　出  : fd
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  socket (int domain, int type, int protocol)
{
    INT          iHash;
    INT          iFd     = PX_ERROR;
    INT          iLwipFd = PX_ERROR;
    
    SOCKET_T    *psock     = LW_NULL;
    AF_UNIX_T   *pafunix   = LW_NULL;
    AF_PACKET_T *pafpacket = LW_NULL;
    
    INT          iCloExec;
    BOOL         iNonBlock;
    
    if (type & SOCK_CLOEXEC) {                                          /*  SOCK_CLOEXEC ?              */
        type &= ~SOCK_CLOEXEC;
        iCloExec = FD_CLOEXEC;
    } else {
        iCloExec = 0;
    }
    
    if (type & SOCK_NONBLOCK) {                                         /*  SOCK_NONBLOCK ?             */
        type &= ~SOCK_NONBLOCK;
        iNonBlock = 1;
    } else {
        iNonBlock = 0;
    }

    __KERNEL_SPACE_ENTER();
    switch (domain) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        pafunix = unix_socket(domain, type, protocol);
        if (pafunix == LW_NULL) {
            __KERNEL_SPACE_EXIT();
            goto    __error_handle;
        }
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        pafpacket = packet_socket(domain, type, protocol);
        if (pafpacket == LW_NULL) {
            __KERNEL_SPACE_EXIT();
            goto    __error_handle;
        }
        break;
    
    default:
        iLwipFd = lwip_socket(domain, type, protocol);
        if (iLwipFd < 0) {
            __KERNEL_SPACE_EXIT();
            goto    __error_handle;
        }
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    iFd = open(LWIP_SYLIXOS_SOCKET_NAME, O_RDWR);
    if (iFd < 0) {
        goto    __error_handle;
    }
    psock = (SOCKET_T *)iosFdValue(iFd);
    if (psock == (SOCKET_T *)PX_ERROR) {
        goto    __error_handle;
    }
    psock->SOCK_iFamily = domain;
    
    switch (domain) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        psock->SOCK_pafunix = pafunix;
        iHash = __SOCKET_UNIX_HASH(pafunix);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        psock->SOCK_pafpacket = pafpacket;
        iHash = __SOCKET_PACKET_HASH(pafpacket);
        break;
        
    default:
        psock->SOCK_iLwipFd = iLwipFd;                                  /*  save lwip fd                */
        iHash = __SOCKET_LWIP_HASH(iLwipFd);
        break;
    }
    
    __SOCKET_LOCK();
    psock->SOCK_iHash = iHash;
    _List_Line_Add_Tail(&psock->SOCK_lineManage, &_G_plineSocket[iHash]);
    __SOCKET_UNLOCK();
    
    if (iCloExec) {
        API_IosFdSetCloExec(iFd, iCloExec);
    }
    
    if (iNonBlock) {
        __KERNEL_SPACE_ENTER();
        __socketIoctl(psock, FIONBIO, &iNonBlock);
        __KERNEL_SPACE_EXIT();
    }
    
    MONITOR_EVT_INT4(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_SOCKET, 
                     domain, type, protocol, iFd, LW_NULL);
    
    return  (iFd);
    
__error_handle:
    if (iFd >= 0) {
        close(iFd);
    }
    if (pafunix) {
        unix_close(pafunix);
        
    } else if (pafpacket) {
        packet_close(pafpacket);
        
    } else if (iLwipFd >= 0) {
        lwip_close(iLwipFd);
    }
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: accept4
** 功能描述: BSD accept4
** 输　入  : s         socket fd
**           addr      address
**           addrlen   address len
**           flags     SOCK_CLOEXEC, SOCK_NONBLOCK
** 输　出  : new socket fd
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  accept4 (int s, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
    INT          iHash;
    SOCKET_T    *psock   = (SOCKET_T *)iosFdValue(s);
    SOCKET_T    *psockNew;
    INT          iType;
    
    AF_UNIX_T   *pafunix   = LW_NULL;
    AF_PACKET_T *pafpacket = LW_NULL;
    INT          iRet      = PX_ERROR;
    INT          iFdNew    = PX_ERROR;
    
    INT          iCloExec;
    BOOL         iNonBlock;
    
    __SOCKET_CHECHK();
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    if (flags & SOCK_CLOEXEC) {                                         /*  SOCK_CLOEXEC ?              */
        iCloExec = FD_CLOEXEC;
    } else {
        iCloExec = 0;
    }
    
    if (flags & SOCK_NONBLOCK) {                                        /*  SOCK_NONBLOCK ?             */
        iNonBlock = 1;
    } else {
        iNonBlock = 0;
    }
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        pafunix = unix_accept(psock->SOCK_pafunix, addr, addrlen);
        if (pafunix == LW_NULL) {
            __KERNEL_SPACE_EXIT();
            goto    __error_handle;
        }
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        pafpacket = packet_accept(psock->SOCK_pafpacket, addr, addrlen);
        if (pafpacket == LW_NULL) {
            __KERNEL_SPACE_EXIT();
            goto    __error_handle;
        }
        break;
        
    default:
        iRet = lwip_accept(psock->SOCK_iLwipFd, addr, addrlen);         /*  lwip_accept                 */
        if (iRet < 0) {
            __KERNEL_SPACE_EXIT();
            goto    __error_handle;
        }
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    iFdNew = open(LWIP_SYLIXOS_SOCKET_NAME, O_RDWR);                    /*  new fd                      */
    if (iFdNew < 0) {
        goto    __error_handle;
    }
    psockNew = (SOCKET_T *)iosFdValue(iFdNew);
    psockNew->SOCK_iFamily = psock->SOCK_iFamily;
    
    switch (psockNew->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        psockNew->SOCK_pafunix = pafunix;
        iHash = __SOCKET_UNIX_HASH(pafunix);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        psockNew->SOCK_pafpacket = pafpacket;
        iHash = __SOCKET_PACKET_HASH(pafpacket);
        break;
        
    default:
        psockNew->SOCK_iLwipFd = iRet;                                  /*  save lwip fd                */
        iHash = __SOCKET_LWIP_HASH(iRet);
        break;
    }
    
    __SOCKET_LOCK();
    psockNew->SOCK_iHash = iHash;
    _List_Line_Add_Tail(&psockNew->SOCK_lineManage, &_G_plineSocket[iHash]);
    __SOCKET_UNLOCK();
    
    if (iCloExec) {
        API_IosFdSetCloExec(iFdNew, iCloExec);
    }
    
    if (iNonBlock) {
        __KERNEL_SPACE_ENTER();
        __socketIoctl(psockNew, FIONBIO, &iNonBlock);
        __KERNEL_SPACE_EXIT();
    }
    
    MONITOR_EVT_INT2(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_ACCEPT, 
                     s, iFdNew, LW_NULL);
    
    return  (iFdNew);
    
__error_handle:
    psock->SOCK_iSoErr = errno;
    if (pafunix) {
        unix_close(pafunix);
    
    } else if (pafpacket) {
        packet_close(pafpacket);
    
    } else if (iRet >= 0) {
        lwip_close(iRet);
    }
    
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: accept
** 功能描述: BSD accept
** 输　入  : s         socket fd
**           addr      address
**           addrlen   address len
** 输　出  : new socket fd
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  accept (int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return  (accept4(s, addr, addrlen, 0));
}
/*********************************************************************************************************
** 函数名称: bind
** 功能描述: BSD bind
** 输　入  : s         socket fd
**           name      address
**           namelen   address len
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  bind (int s, const struct sockaddr *name, socklen_t namelen)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    INT         iRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        iRet = unix_bind(psock->SOCK_pafunix, name, namelen);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        iRet = packet_bind(psock->SOCK_pafpacket, name, namelen);
        break;
        
    default:
        iRet = lwip_bind(psock->SOCK_iLwipFd, name, namelen);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (iRet < ERROR_NONE) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_INT1(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_BIND, 
                         s, LW_NULL);
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: bind
** 功能描述: BSD bind
** 输　入  : s         socket fd
**           how       SHUT_RD  SHUT_RDWR  SHUT_WR
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  shutdown (int s, int how)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    INT         iRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        iRet = unix_shutdown(psock->SOCK_pafunix, how);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        iRet = packet_shutdown(psock->SOCK_pafpacket, how);
        break;
        
    default:
        iRet = lwip_shutdown(psock->SOCK_iLwipFd, how);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (iRet < ERROR_NONE) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_INT2(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_SHUTDOWN, 
                         s, how, LW_NULL);
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: connect
** 功能描述: BSD connect
** 输　入  : s         socket fd
**           name      address
**           namelen   address len
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  connect (int s, const struct sockaddr *name, socklen_t namelen)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    INT         iRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        iRet = unix_connect(psock->SOCK_pafunix, name, namelen);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        iRet = packet_connect(psock->SOCK_pafpacket, name, namelen);
        break;
        
    default:
        iRet = lwip_connect(psock->SOCK_iLwipFd, name, namelen);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (iRet < ERROR_NONE) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_INT1(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_CONNECT, 
                         s, LW_NULL);
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: getsockname
** 功能描述: BSD getsockname
** 输　入  : s         socket fd
**           name      address
**           namelen   address len
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  getsockname (int s, struct sockaddr *name, socklen_t *namelen)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    INT         iRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        iRet = unix_getsockname(psock->SOCK_pafunix, name, namelen);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        iRet = packet_getsockname(psock->SOCK_pafpacket, name, namelen);
        break;
        
    default:
        iRet = lwip_getsockname(psock->SOCK_iLwipFd, name, namelen);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (iRet < ERROR_NONE) {
        psock->SOCK_iSoErr = errno;
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: getpeername
** 功能描述: BSD getpeername
** 输　入  : s         socket fd
**           name      address
**           namelen   address len
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  getpeername (int s, struct sockaddr *name, socklen_t *namelen)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    INT         iRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        iRet = unix_getpeername(psock->SOCK_pafunix, name, namelen);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        iRet = packet_getpeername(psock->SOCK_pafpacket, name, namelen);
        break;
        
    default:
        iRet = lwip_getpeername(psock->SOCK_iLwipFd, name, namelen);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (iRet < ERROR_NONE) {
        psock->SOCK_iSoErr = errno;
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: setsockopt
** 功能描述: BSD setsockopt
** 输　入  : s         socket fd
**           level     level
**           optname   option
**           optval    option value
**           optlen    option value len
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  setsockopt (int s, int level, int optname, const void *optval, socklen_t optlen)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    INT         iRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        iRet = unix_setsockopt(psock->SOCK_pafunix, level, optname, optval, optlen);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        iRet = packet_setsockopt(psock->SOCK_pafpacket, level, optname, optval, optlen);
        break;
        
    default:
        iRet = lwip_setsockopt(psock->SOCK_iLwipFd, level, optname, optval, optlen);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (iRet < ERROR_NONE) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_INT3(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_SOCKOPT, 
                         s, level, optname, LW_NULL);
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: getsockopt
** 功能描述: BSD getsockopt
** 输　入  : s         socket fd
**           level     level
**           optname   option
**           optval    option value
**           optlen    option value len
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  getsockopt (int s, int level, int optname, void *optval, socklen_t *optlen)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    INT         iRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    if (level == SOL_SOCKET) {                                          /*  统一处理 SO_ERROR           */
        if (optname == SO_ERROR) {
            if (!optval || *optlen < sizeof(INT)) {
                _ErrorHandle(EINVAL);
                return  (iRet);
            }
            *(INT *)optval = psock->SOCK_iSoErr;
            psock->SOCK_iSoErr = ERROR_NONE;
            return  (ERROR_NONE);
        }
    }
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        iRet = unix_getsockopt(psock->SOCK_pafunix, level, optname, optval, optlen);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        iRet = packet_getsockopt(psock->SOCK_pafpacket, level, optname, optval, optlen);
        break;
        
    default:
        iRet = lwip_getsockopt(psock->SOCK_iLwipFd, level, optname, optval, optlen);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (iRet < ERROR_NONE) {
        psock->SOCK_iSoErr = errno;
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: listen
** 功能描述: BSD listen
** 输　入  : s         socket fd
**           backlog   back log num
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int listen (int s, int backlog)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    INT         iRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        iRet = unix_listen(psock->SOCK_pafunix, backlog);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        iRet = packet_listen(psock->SOCK_pafpacket, backlog);
        break;
        
    default:
        iRet = lwip_listen(psock->SOCK_iLwipFd, backlog);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (iRet < ERROR_NONE) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_INT2(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_LISTEN, 
                         s, backlog, LW_NULL);
    }
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: recv
** 功能描述: BSD recv
** 输　入  : s         socket fd
**           mem       buffer
**           len       buffer len
**           flags     flag
** 输　出  : NUM
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ssize_t  recv (int s, void *mem, size_t len, int flags)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    ssize_t     sstRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        sstRet = (ssize_t)unix_recv(psock->SOCK_pafunix, mem, len, flags);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        sstRet = (ssize_t)packet_recv(psock->SOCK_pafpacket, mem, len, flags);
        break;
        
    default:
        sstRet = (ssize_t)lwip_recv(psock->SOCK_iLwipFd, mem, len, flags);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (sstRet <= 0) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_LONG3(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_RECV, 
                          s, flags, sstRet, LW_NULL);
    }
    
    return  (sstRet);
}
/*********************************************************************************************************
** 函数名称: recvfrom
** 功能描述: BSD recvfrom
** 输　入  : s         socket fd
**           mem       buffer
**           len       buffer len
**           flags     flag
**           from      packet from
**           fromlen   name len
** 输　出  : NUM
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ssize_t  recvfrom (int s, void *mem, size_t len, int flags,
                   struct sockaddr *from, socklen_t *fromlen)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    ssize_t     sstRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        sstRet = (ssize_t)unix_recvfrom(psock->SOCK_pafunix, mem, len, flags, from, fromlen);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        sstRet = (ssize_t)packet_recvfrom(psock->SOCK_pafpacket, mem, len, flags, from, fromlen);
        break;
        
    default:
        sstRet = (ssize_t)lwip_recvfrom(psock->SOCK_iLwipFd, mem, len, flags, from, fromlen);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (sstRet <= 0) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_LONG3(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_RECV, 
                          s, flags, sstRet, LW_NULL);
    }
    
    return  (sstRet);
}
/*********************************************************************************************************
** 函数名称: recvmsg
** 功能描述: BSD recvmsg
** 输　入  : s             套接字
**           msg           消息
**           flags         特殊标志
** 输　出  : NUM (此长度不包含控制信息长度)
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ssize_t  recvmsg (int  s, struct msghdr *msg, int flags)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    ssize_t     sstRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        sstRet = (ssize_t)unix_recvmsg(psock->SOCK_pafunix, msg, flags);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        sstRet = (ssize_t)packet_recvmsg(psock->SOCK_pafpacket, msg, flags);
        break;
        
    default:
        sstRet = (ssize_t)lwip_recvmsg(psock->SOCK_iLwipFd, msg, flags);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (sstRet <= 0) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_LONG3(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_RECV, 
                          s, flags, sstRet, LW_NULL);
    }
    
    return  (sstRet);
}
/*********************************************************************************************************
** 函数名称: send
** 功能描述: BSD send
** 输　入  : s         socket fd
**           data      send buffer
**           size      send len
**           flags     flag
** 输　出  : NUM
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ssize_t  send (int s, const void *data, size_t size, int flags)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    ssize_t     sstRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        sstRet = (ssize_t)unix_send(psock->SOCK_pafunix, data, size, flags);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        sstRet = (ssize_t)packet_send(psock->SOCK_pafpacket, data, size, flags);
        break;
        
    default:
        sstRet = (ssize_t)lwip_send(psock->SOCK_iLwipFd, data, size, flags);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (sstRet <= 0) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_LONG3(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_SEND, 
                          s, flags, sstRet, LW_NULL);
    }
    
    return  (sstRet);
}
/*********************************************************************************************************
** 函数名称: sendto
** 功能描述: BSD sendto
** 输　入  : s         socket fd
**           data      send buffer
**           size      send len
**           flags     flag
**           to        packet to
**           tolen     name len
** 输　出  : NUM
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ssize_t  sendto (int s, const void *data, size_t size, int flags,
                 const struct sockaddr *to, socklen_t tolen)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    ssize_t     sstRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        sstRet = (ssize_t)unix_sendto(psock->SOCK_pafunix, data, size, flags, to, tolen);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        sstRet = (ssize_t)packet_sendto(psock->SOCK_pafpacket, data, size, flags, to, tolen);
        break;
        
    default:
        sstRet = (ssize_t)lwip_sendto(psock->SOCK_iLwipFd, data, size, flags, to, tolen);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (sstRet <= 0) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_LONG3(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_SEND, 
                          s, flags, sstRet, LW_NULL);
    }
    
    return  (sstRet);
}
/*********************************************************************************************************
** 函数名称: sendmsg
** 功能描述: BSD sendmsg
** 输　入  : s             套接字
**           msg           消息
**           flags         特殊标志
** 输　出  : NUM (此长度不包含控制信息长度)
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
ssize_t  sendmsg (int  s, const struct msghdr *msg, int flags)
{
    SOCKET_T   *psock = (SOCKET_T *)iosFdValue(s);
    INT         iType;
    ssize_t     sstRet = PX_ERROR;
    
    __SOCKET_CHECHK();
    
    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */
    
    __KERNEL_SPACE_ENTER();
    switch (psock->SOCK_iFamily) {
    
    case AF_UNIX:                                                       /*  UNIX 域协议                 */
        sstRet = (ssize_t)unix_sendmsg(psock->SOCK_pafunix, msg, flags);
        break;
        
    case AF_PACKET:                                                     /*  PACKET                      */
        sstRet = (ssize_t)packet_sendmsg(psock->SOCK_pafpacket, msg, flags);
        break;
        
    default:
        sstRet = (ssize_t)lwip_sendmsg(psock->SOCK_iLwipFd, msg, flags);
        break;
    }
    __KERNEL_SPACE_EXIT();
    
    if (sstRet <= 0) {
        psock->SOCK_iSoErr = errno;
    
    } else {
        MONITOR_EVT_LONG3(MONITOR_EVENT_ID_NETWORK, MONITOR_EVENT_NETWORK_SEND, 
                          s, flags, sstRet, LW_NULL);
    }
    
    return  (sstRet);
}
/*********************************************************************************************************
** 函数名称: gethostbyname
** 功能描述: BSD gethostbyname
** 输　入  : name      domain name
** 输　出  : hostent
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
struct hostent  *gethostbyname (const char *name)
{
    return  (lwip_gethostbyname(name));
}
/*********************************************************************************************************
** 函数名称: gethostbyname_r
** 功能描述: BSD gethostbyname_r
** 输　入  : name      domain name
**           ret       hostent buffer
**           buf       result buffer
**           buflen    buf len
**           result    result return
**           h_errnop  error
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int gethostbyname_r (const char *name, struct hostent *ret, char *buf,
                     size_t buflen, struct hostent **result, int *h_errnop)
{
    return  (lwip_gethostbyname_r(name, ret, buf, buflen, result, h_errnop));
}
/*********************************************************************************************************
** 函数名称: gethostbyaddr_r
** 功能描述: BSD gethostbyname_r
** 输　入  : addr      domain addr
**           length    socketaddr len
**           type      AF_INET
** 输　出  : hostent
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
struct hostent *gethostbyaddr (const void *addr, socklen_t length, int type)
{
    errno = ENOSYS;
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: gethostbyaddr_r
** 功能描述: BSD gethostbyname_r
** 输　入  : addr      domain addr
**           length    socketaddr len
**           type      AF_INET
**           ret       hostent buffer
**           buf       result buffer
**           buflen    buf len
**           result    result return
**           h_errnop  error
** 输　出  : hostent
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
struct hostent *gethostbyaddr_r (const void *addr, socklen_t length, int type,
                                 struct hostent *ret, char  *buffer, int buflen, int *h_errnop)
{
    errno = ENOSYS;
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: freeaddrinfo
** 功能描述: BSD freeaddrinfo
** 输　入  : ai        addrinfo
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
void  freeaddrinfo (struct addrinfo *ai)
{
    lwip_freeaddrinfo(ai);
}
/*********************************************************************************************************
** 函数名称: getaddrinfo
** 功能描述: BSD getaddrinfo
** 输　入  : nodename  node name
**           servname  server name
**           hints     addrinfo
**           res       result
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  getaddrinfo (const char *nodename, const char *servname,
                  const struct addrinfo *hints, struct addrinfo **res)
{
    return  (lwip_getaddrinfo(nodename, servname, hints, res));
}
#endif                                                                  /*  LW_CFG_NET_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
