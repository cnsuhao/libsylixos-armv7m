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
** 文   件   名: lwip_config.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 05 月 06 日
**
** 描        述: lwip 裁剪.

** BUG:
2009.07.02  加入了 ETHARP_TRUST_IP_MAC 的配置. windows 系统默认不信任输入 IP 的 MAC 地址.
2009.07.28  将连接文件描述符的名字改为 /socket
2010.02.01  静态地址转换表初始化 none-host 对应 0 无效地址.
2010.02.03  MEMP_NUM_NETDB 默认为 10.
2010.05.27  如果系统支持 SSL VPN 网络, 必须使用 LWIP_TCPIP_CORE_LOCKING 使 lwip 支持安全的多线程操作.
            (SSL VPN 虚拟网卡使用多线程同时操作同一个 socket. LWIP_TCPIP_CORE_LOCKING 仍处于测试阶段!)
2011.05.24  TCP_SND_BUF 始终设置为 64KB - 1, 可以在 non-blocking 时发送最大数据包.
*********************************************************************************************************/

#ifndef __LWIP_CONFIG_H
#define __LWIP_CONFIG_H

#include "SylixOS.h"
#include "string.h"                                                     /*  memcpy                      */

#ifdef __cplusplus
extern "C" {
#endif                                                                  /*  __cplusplus                 */

/*********************************************************************************************************
  Platform specific locking
*********************************************************************************************************/

#define SYS_LIGHTWEIGHT_PROT            1
#define LWIP_MPU_COMPATIBLE             0                               /*  Do not use MPU support.     */

/*********************************************************************************************************
  Memory options
*********************************************************************************************************/

#define MEM_ALIGNMENT                   (LW_CFG_CPU_WORD_LENGHT / NBBY) /*  内存对齐情况                */
#define MEM_SIZE                        LW_CFG_LWIP_MEM_SIZE            /*  malloc 堆大小               */
#define MEMP_NUM_PBUF                   LW_CFG_LWIP_NUM_PBUFS           /*  npbufs                      */
#define PBUF_POOL_SIZE                  LW_CFG_LWIP_NUM_POOLS           /*  pool num                    */
#define PBUF_POOL_BUFSIZE               LW_CFG_LWIP_POOL_SIZE           /*  pool block size             */

#if MEM_SIZE >= (32 * LW_CFG_MB_SIZE)
#define MEMP_NUM_REASSDATA              150                             /*  同时进行重组的 IP 数据包    */
#elif MEM_SIZE >= (1 * LW_CFG_MB_SIZE)
#define MEMP_NUM_REASSDATA              100
#elif MEM_SIZE >= (512 * LW_CFG_KB_SIZE)
#define MEMP_NUM_REASSDATA              80
#elif MEM_SIZE >= (256 * LW_CFG_KB_SIZE)
#define MEMP_NUM_REASSDATA              40
#elif MEM_SIZE >= (128 * LW_CFG_KB_SIZE)
#define MEMP_NUM_REASSDATA              20
#else
#define MEMP_NUM_REASSDATA              5
#endif                                                                  /*  MEM_SIZE >= ...             */

/*********************************************************************************************************
  ...PCB
*********************************************************************************************************/

#define MEMP_NUM_RAW_PCB                LW_CFG_LWIP_RAW_PCB
#define MEMP_NUM_UDP_PCB                LW_CFG_LWIP_UDP_PCB
#define MEMP_NUM_TCP_PCB                LW_CFG_LWIP_TCP_PCB
#define MEMP_NUM_TCP_PCB_LISTEN         LW_CFG_LWIP_TCP_PCB

#define MEMP_NUM_NETCONN                (LW_CFG_LWIP_RAW_PCB + LW_CFG_LWIP_UDP_PCB + \
                                         LW_CFG_LWIP_TCP_PCB)

#define MEMP_NUM_TCPIP_MSG_API          (LW_CFG_LWIP_TCP_PCB + LW_CFG_LWIP_UDP_PCB + LW_CFG_LWIP_RAW_PCB)
#define MEMP_NUM_TCPIP_MSG_INPKT        LW_CFG_LWIP_MSG_SIZE            /*  tcp input msgqueue use      */

/*********************************************************************************************************
  check sum
*********************************************************************************************************/

#if LW_CFG_LWIP_GEN_CHECKSUM == 0
#define CHECKSUM_GEN_IP                 0
#define CHECKSUM_GEN_UDP                0
#define CHECKSUM_GEN_TCP                0
#define CHECKSUM_GEN_ICMP               0
#define CHECKSUM_GEN_ICMP6              0
#endif                                                                  /*  LW_CFG_LWIP_GEN_CHECKSUM    */

#if LW_CFG_LWIP_CHECK_CHECKSUM == 0
#define CHECKSUM_CHECK_IP               0
#define CHECKSUM_CHECK_UDP              0
#define CHECKSUM_CHECK_TCP              0
#define CHECKSUM_CHECK_ICMP             0
#define CHECKSUM_CHECK_ICMP6            0
#endif                                                                  /*  LW_CFG_LWIP_CHECK_CHECKSUM  */

#define LWIP_CHECKSUM_ON_COPY           1                               /*  拷贝数据包同时计算 chksum   */

/*********************************************************************************************************
  sylixos do not use MEMP_NUM_NETCONN, because sylixos use another socket interface.
*********************************************************************************************************/

#define IP_FORWARD                      1                               /*  允许 IP 转发                */
#define IP_REASSEMBLY                   1
#define IP_FRAG                         1
#define IP_REASS_MAX_PBUFS              (MEMP_NUM_PBUF / 2)

#define IP_SOF_BROADCAST                1                               /*  Use the SOF_BROADCAST       */
#define IP_SOF_BROADCAST_RECV           1

#define IP_FORWARD_ALLOW_TX_ON_RX_NETIF 1

/*********************************************************************************************************
  IPv6
*********************************************************************************************************/

#define LWIP_IPV6                       1
#define LWIP_IPV6_MLD                   1
#define LWIP_IPV6_FORWARD               1
#define LWIP_ICMP6                      1
#define LWIP_IPV6_FRAG                  1
#define LWIP_IPV6_REASS                 1

#define MEMP_NUM_MLD6_GROUP             16
#define LWIP_ND6_NUM_NEIGHBORS          LW_CFG_LWIP_ARP_TABLE_SIZE
#define LWIP_ND6_NUM_DESTINATIONS       LW_CFG_LWIP_ARP_TABLE_SIZE
#define LWIP_IPV6_DHCP6                 LW_CFG_LWIP_DHCP

#define LWIP_IPV6_NUM_ADDRESSES         5                               /*  one face max 5 ipv6 addr    */

/*********************************************************************************************************
  dhcp & autoip
*********************************************************************************************************/

#define LWIP_DHCP                       LW_CFG_LWIP_DHCP                /*  DHCP                        */
#define LWIP_DHCP_BOOTP_FILE            0                               /*  not include bootp file now  */
#define LWIP_AUTOIP                     LW_CFG_LWIP_AUTOIP

#if (LWIP_DHCP > 0) && (LWIP_AUTOIP > 0)
#define LWIP_DHCP_AUTOIP_COOP           1
#endif                                                                  /*  (LWIP_DHCP > 0)             */
                                                                        /*  (LWIP_AUTOIP > 0)           */

/*********************************************************************************************************
  timeouts (default + 10, aodv, lowpan ...)
*********************************************************************************************************/

#define MEMP_NUM_SYS_TIMEOUT_DEFAULT    (LWIP_TCP + IP_REASSEMBLY + LWIP_ARP + (2*LWIP_DHCP) + \
                                         LWIP_AUTOIP + LWIP_IGMP + LWIP_DNS + \
                                         (PPP_SUPPORT*6*MEMP_NUM_PPP_PCB) + \
                                         (LWIP_IPV6 ? (1 + LWIP_IPV6_REASS + LWIP_IPV6_MLD) : 0))

#define MEMP_NUM_SYS_TIMEOUT            MEMP_NUM_SYS_TIMEOUT_DEFAULT + 10

#define MEMP_NUM_NETBUF                 LW_CFG_LWIP_NUM_NETBUF

/*********************************************************************************************************
  ICMP
*********************************************************************************************************/

#define LWIP_ICMP                       1

/*********************************************************************************************************
  RAW
*********************************************************************************************************/

#define LWIP_RAW                        1

/*********************************************************************************************************
  SNMP
*********************************************************************************************************/

#define LWIP_SNMP                       LW_CFG_LWIP_SNMP
#define SNMP_PRIVATE_MIB                1                               /*  support now!                */

extern  VOID  __netSnmpGetTimestamp(UINT32  *puiTimestamp);
extern  VOID  __netSnmpPriMibInit(VOID);

#define SNMP_GET_SYSUPTIME(sysuptime)   {                                       \
                                            __netSnmpGetTimestamp(&sysuptime);  \
                                        }
#define SNMP_PRIVATE_MIB_INIT()         __netSnmpPriMibInit()
                                        

/*********************************************************************************************************
  IGMP
*********************************************************************************************************/

#define LWIP_IGMP                       LW_CFG_LWIP_IGMP
#define MEMP_NUM_IGMP_GROUP             LW_CFG_LWIP_IGMP_GROUP

/*********************************************************************************************************
  DNS
*********************************************************************************************************/

#define LWIP_DNS                        1

#ifndef MEMP_NUM_NETDB
#define MEMP_NUM_NETDB                  10
#endif                                                                  /*  MEMP_NUM_NETDB              */

#define DNS_MAX_NAME_LENGTH             PATH_MAX

#define DNS_LOCAL_HOSTLIST              1
#define DNS_LOCAL_HOSTLIST_INIT         {{"none-host", {(u32_t)0}, LW_NULL}}

extern  UINT32  __inetHostTableGetItem(CPCHAR  pcHost);                 /*  本地地址映射表查询          */
                                                                        /*  范围 IPv4 网络字节序地址    */
#define DNS_LOCAL_HOSTLIST_IS_DYNAMIC   1
#define DNS_LOOKUP_LOCAL_EXTERN(x)      __inetHostTableGetItem(x)

/*********************************************************************************************************
  TCP basic
*********************************************************************************************************/

#if LW_CFG_LWIP_TCP_MAXRTX > 0
#define TCP_MAXRTX                      LW_CFG_LWIP_TCP_MAXRTX
#endif

#if LW_CFG_LWIP_TCP_SYNMAXRTX > 0
#define TCP_SYNMAXRTX                   LW_CFG_LWIP_TCP_SYNMAXRTX
#endif

/*********************************************************************************************************
  transmit layer (注意: 如果 TCP_WND 大于网卡接收缓冲, 可能造成批量传输时, 网卡芯片缓冲溢出.
                        如果出现这种情况, 请在这里自行配置 TCP_WND 大小. 并确保大于 2 * TCP_MSS)
*********************************************************************************************************/

#define LWIP_UDP                        1
#define LWIP_UDPLITE                    1
#define LWIP_NETBUF_RECVINFO            1

#define LWIP_TCP                        1
#define TCP_LISTEN_BACKLOG              1
#define LWIP_TCP_TIMESTAMPS             1

#define TCP_QUEUE_OOSEQ                 1

#ifndef TCP_MSS
#define TCP_MSS                         1460                            /*  usually                     */
#endif                                                                  /*  TCP_MSS                     */

#define TCP_CALCULATE_EFF_SEND_MSS      1                               /*  use effective send MSS      */

#if MEM_SIZE >= (8 * LW_CFG_MB_SIZE)
#define TCP_WND                         ((64 * LW_CFG_KB_SIZE) - 1)     /*  MAX WINDOW                  */
#elif MEM_SIZE >= (4 * LW_CFG_MB_SIZE)
#define TCP_WND                         (32  * LW_CFG_KB_SIZE)
#elif MEM_SIZE >= (1 * LW_CFG_MB_SIZE)
#define TCP_WND                         (16  * LW_CFG_KB_SIZE)
#elif MEM_SIZE >= (512 * LW_CFG_KB_SIZE)
#define TCP_WND                         ( 8  * LW_CFG_KB_SIZE)

/*********************************************************************************************************
  MEM_SIZE < 128 KB  SMALL TCP_MSS XXX
*********************************************************************************************************/

#elif MEM_SIZE >= (64 * LW_CFG_KB_SIZE)
#define TCP_WND                         ( 4  * TCP_MSS)
#else
#undef  TCP_MSS
#define TCP_MSS                         536                             /*  small mss                   */
#define TCP_WND                         ( 4  * TCP_MSS)
#endif                                                                  /*  MEM_SIZE >= ...             */

#if LW_CFG_LWIP_TCP_WND > 0
#undef  TCP_WND
#define TCP_WND                         LW_CFG_LWIP_TCP_WND
#endif                                                                  /*  LW_CFG_LWIP_TCP_WND         */

#if TCP_WND < (4  * TCP_MSS)
#undef  TCP_WND
#define TCP_WND                         ( 4  * TCP_MSS)                 /*  must bigger than 4 * TCP_MSS*/
#endif                                                                  /*  TCP_WND < (4  * TCP_MSS)    */

#define LWIP_WND_SCALE                  1
#define TCP_RCV_SCALE                   LW_CFG_LWIP_TCP_RCV_SCALE

#define TCP_SND_BUF                     ((64 * LW_CFG_KB_SIZE) - 1)     /*  tcp snd buf size (max size) */
#define MEMP_NUM_TCP_SEG                (8 * TCP_SND_QUEUELEN)

#define LWIP_TCP_KEEPALIVE              1
#define LWIP_SO_SNDTIMEO                1
#define LWIP_SO_RCVTIMEO                1
#define LWIP_SO_RCVBUF                  1

#define SO_REUSE                        1                               /*  enable SO_REUSEADDR         */
#define SO_REUSE_RXTOALL                1

#define LWIP_FIONREAD_LINUXMODE         1                               /*  linux FIONREAD compatibility*/

/*********************************************************************************************************
  network interfaces options
*********************************************************************************************************/

#define LWIP_NETIF_HOSTNAME             1                               /*  netif have nostname member  */
#define LWIP_NETIF_API                  1                               /*  support netif api           */
#define LWIP_NETIF_STATUS_CALLBACK      1                               /*  interface status change     */
#define LWIP_NETIF_REMOVE_CALLBACK      1                               /*  interface remove hook       */
#define LWIP_NETIF_LINK_CALLBACK        1                               /*  link status change          */
#define LWIP_NETIF_HWADDRHINT           1                               /*  XXX                         */
#define LWIP_NETIF_LOOPBACK             1

/*********************************************************************************************************
  network interfaces max hardware address len
*********************************************************************************************************/

#define NETIF_MAX_HWADDR_LEN            8                               /*  max 8bytes physical addr len*/

/*********************************************************************************************************
  ethernet net options
*********************************************************************************************************/

#define LWIP_ARP                        1
#define ARP_QUEUEING                    1
#define ARP_TABLE_SIZE                  LW_CFG_LWIP_ARP_TABLE_SIZE
#define ETHARP_TRUST_IP_MAC             LW_CFG_LWIP_ARP_TRUST_IP_MAC
#define ETHARP_SUPPORT_VLAN             1                               /*  IEEE 802.1q VLAN            */
#define ETH_PAD_SIZE                    LW_CFG_LWIP_ETH_PAD_SIZE
#define ETHARP_SUPPORT_STATIC_ENTRIES   1

/*********************************************************************************************************
  loop interface
*********************************************************************************************************/

#define LWIP_HAVE_LOOPIF                1                               /*  127.0.0.1                   */

/*********************************************************************************************************
  slip interface
*********************************************************************************************************/

#define LWIP_HAVE_SLIPIF                1

/*********************************************************************************************************
  inet thread
*********************************************************************************************************/

#define TCPIP_THREAD_NAME               "t_netproto"
#define TCPIP_THREAD_STACKSIZE          LW_CFG_LWIP_STK_SIZE
#define TCPIP_THREAD_PRIO               LW_PRIO_T_NETPROTO

#define SLIPIF_THREAD_NAME              "t_slip"
#define SLIPIF_THREAD_STACKSIZE         LW_CFG_LWIP_STK_SIZE
#define SLIPIF_THREAD_PRIO              LW_PRIO_T_NETPROTO

#define PPP_THREAD_NAME                 "t_ppp"
#define PPP_THREAD_STACKSIZE            LW_CFG_LWIP_STK_SIZE
#define PPP_THREAD_PRIO                 LW_PRIO_T_NETPROTO

#define DEFAULT_THREAD_NAME             "t_netdef"
#define DEFAULT_THREAD_STACKSIZE        LW_CFG_LWIP_STK_SIZE
#define DEFAULT_THREAD_PRIO             LW_PRIO_T_NETPROTO

/*********************************************************************************************************
  Socket options 
*********************************************************************************************************/

#define LWIP_SOCKET                     1
#define LWIP_SOCKET_SET_ERRNO           1
#define LWIP_TIMEVAL_PRIVATE            0                               /*  SylixOS has already defined */
#define LWIP_COMPAT_SOCKETS             0                               /*  do not us socket macro      */
#define LWIP_PREFIX_BYTEORDER_FUNCS     1                               /*  do not define macro hto??() */

#define LWIP_POSIX_SOCKETS_IO_NAMES     0                               /*  do not have this!!!         */

/*********************************************************************************************************
  DNS config
*********************************************************************************************************/

#define LWIP_DNS_API_DEFINE_ERRORS      0                               /*  must use sylixos dns errors */
#define LWIP_DNS_API_HOSTENT_STORAGE    1                               /*  have bsd DNS                */
#define LWIP_DNS_API_DECLARE_STRUCTS    1                               /*  use lwip DNS struct         */

/*********************************************************************************************************
  Statistics options
*********************************************************************************************************/

#define LWIP_STATS                      1
#define LWIP_STATS_DISPLAY              1
#define LWIP_STATS_LARGE                1

/*********************************************************************************************************
  PPP options
*********************************************************************************************************/

#define PPP_SUPPORT                     LW_CFG_LWIP_PPP
#define PPPOS_SUPPORT                   LW_CFG_LWIP_PPP
#define PPPOE_SUPPORT                   LW_CFG_LWIP_PPPOE
#define PPPOL2TP_SUPPORT                LW_CFG_LWIP_PPPOL2TP

/*********************************************************************************************************
  [ppp-new] branch
*********************************************************************************************************/

#define __LWIP_USE_PPP_NEW              1
#define PPP_SERVER                      0

/*********************************************************************************************************
  ppp api
*********************************************************************************************************/

#if PPP_SUPPORT > 0 || PPPOE_SUPPORT > 0

#define LWIP_PPP_API                    1

/*********************************************************************************************************
  ppp num and interface
*********************************************************************************************************/

#define MEMP_NUM_PPPOE_INTERFACES       LW_CFG_LWIP_NUM_PPP
#define MEMP_NUM_PPPOL2TP_INTERFACES    LW_CFG_LWIP_NUM_PPP
#define NUM_PPP                         LW_CFG_LWIP_NUM_PPP
#define MEMP_NUM_PPP_PCB                LW_CFG_LWIP_NUM_PPP

/*********************************************************************************************************
  ppp option
*********************************************************************************************************/

#define PPP_INPROC_MULTITHREADED        1                               /*  pppos use pppfd recv-thread */
#define PPP_NOTIFY_PHASE                1
#define PPP_FCS_TABLE                   1

#define PAP_SUPPORT                     1
#define CHAP_SUPPORT                    1

#if __LWIP_USE_PPP_NEW > 0
#define MSCHAP_SUPPORT                  1
#else
#define MSCHAP_SUPPORT                  0
#endif                                                                  /*  __LWIP_USE_PPP_NEW > 0      */

#define EAP_SUPPORT                     0
#define CBCP_SUPPORT                    0
#define CCP_SUPPORT                     0
#define ECP_SUPPORT                     0
#define LQR_SUPPORT                     1
#define VJ_SUPPORT                      1
#define MD5_SUPPORT                     1

#define PPP_MD5_RANDM                   1
#define PPP_IPV6_SUPPORT                1

/*********************************************************************************************************
  [for new ppp] 
  
  use sylixos ssl (external PolarSSL) 
*********************************************************************************************************/

#if __LWIP_USE_PPP_NEW > 0
#define LWIP_INCLUDED_POLARSSL_MD5      0
#define LWIP_INCLUDED_POLARSSL_MD4      0
#define LWIP_INCLUDED_POLARSSL_SHA1     0
#define LWIP_INCLUDED_POLARSSL_DES      0
#endif                                                                  /*  __LWIP_USE_PPP_NEW          */

/*********************************************************************************************************
  ppp timeouts
*********************************************************************************************************/

#define LCP_ECHOINTERVAL                10                              /*  check lcp link per 10seconds*/

/*********************************************************************************************************
  ppp api
*********************************************************************************************************/
#else                                                                   /*  no ppp support              */

#define LWIP_PPP_API                    0

#endif                                                                  /*  PPP_SUPPORT > 0 ||          */
                                                                        /*  PPPOE_SUPPORT > 0           */
/*********************************************************************************************************
  sylixos 需要使用 lwip core lock 模式, 来支持全双工 socket
*********************************************************************************************************/

#define LWIP_TCPIP_CORE_LOCKING         1
                                                                        
/*********************************************************************************************************
  Debugging options (TCP UDP IP debug & only can print message)
*********************************************************************************************************/

#define LWIP_DBG_TYPES_ON               LWIP_DBG_ON

#if LW_CFG_LWIP_DEBUG > 0
#define LWIP_DEBUG
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_OFF              /*  允许全部主动打印信息        */
#define TCP_DEBUG                       (LWIP_DBG_ON | 0)               /*  仅允许 TCP UDP IP debug     */
#define UDP_DEBUG                       (LWIP_DBG_ON | 0)
#define IP_DEBUG                        (LWIP_DBG_ON | 0)
#else
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_SEVERE           /*  不允许主动打印信息          */
#endif                                                                  /*  LW_CFG_LWIP_DEBUG > 0       */

/*********************************************************************************************************
  get_opt 冲突
*********************************************************************************************************/

#ifdef  opterr
#undef  opterr
#endif                                                                  /*  opterr                      */

/*********************************************************************************************************
  sylixos msgqueue
*********************************************************************************************************/

#define LWIP_MSGQUEUE_SIZE              LW_CFG_LWIP_MSG_SIZE            /*  sys_mbox_new size           */

/*********************************************************************************************************
  sylixos tty (serial port) file name
*********************************************************************************************************/

#define LWIP_SYLIXOS_TTY_NAME           "/dev/ttyS"                     /*  串口 TTY 中断文件名前缀     */

/*********************************************************************************************************
  lwip virtual device file name
*********************************************************************************************************/

#define LWIP_SYLIXOS_SOCKET_NAME        "/dev/socket"                   /*  socket file name            */

/*********************************************************************************************************
  lwip create or remove netif hook
*********************************************************************************************************/

extern INT   netif_add_hook(PVOID  pvNetif);
extern VOID  netif_remove_hook(PVOID  pvNetif);
extern UINT  netif_get_num(VOID);
extern PVOID netif_get_by_index(UINT uiIndex);

/*********************************************************************************************************
  lwip ip route hook (return is the netif has been route)
*********************************************************************************************************/

extern PVOID ip_route_hook(PVOID pvDest);
#define LWIP_HOOK_IP4_ROUTE             ip_route_hook

/*********************************************************************************************************
  lwip ip input hook (return is the packet has been eaten)
*********************************************************************************************************/

extern int ip_input_hook(PVOID  pvPBuf, PVOID  pvNetif);
#define LWIP_HOOK_IP4_INPUT             ip_input_hook

/*********************************************************************************************************
  lwip link input/output hook (for AF_PACKET)
*********************************************************************************************************/

extern int link_input_hook(PVOID  pvPBuf, PVOID  pvNetif);
#define LWIP_HOOK_LINK_INPUT            link_input_hook

extern int link_output_hook(PVOID  pvPBuf, PVOID  pvNetif);
#define LWIP_HOOK_LINK_OUTPUT           link_output_hook

/*********************************************************************************************************
  lwip ppp status hook
*********************************************************************************************************/

#if PPP_SUPPORT > 0 || PPPOE_SUPPORT > 0
extern VOID ppp_link_status_hook(PVOID pvPPP, INT iError, PVOID pvArg);
#endif

/*********************************************************************************************************
  lwip socket.h 相关宏重定义, 以兼容绝大多数的应用
*********************************************************************************************************/

#define SHUT_RD                         0
#define SHUT_WR                         1
#define SHUT_RDWR                       2

#ifdef __cplusplus
}
#endif                                                                  /*  __cplusplus                 */

#endif                                                                  /*  __LWIP_CONFIG_H             */
/*********************************************************************************************************
  END
*********************************************************************************************************/
