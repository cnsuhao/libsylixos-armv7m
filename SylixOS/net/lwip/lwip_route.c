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
** 文   件   名: lwip_route.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2011 年 07 月 01 日
**
** 描        述: lwip sylixos 路由表.
                 lwip 路由表接口诞生过程, 请详见: http://savannah.nongnu.org/bugs/?33634

** BUG:
2011.07.07  在 net safe 状态下不允许调用 printf 等使用 IO 的语句.
2011.08.17  __rtSafeRun() 在安装 lo0 网卡时, lwip core lock 还没有创建, 所以这里需要判断一下!
2012.03.29  __aodvEntryPrint() hcnt > 0 即存在转发节点, 应显示为 G.
2013.01.15  加入简单路由操作接口.
2013.01.16  LW_RT_FLAG_G 标志为动态的, 每次遍历时才会更新, 因为网卡参数的改动可能会引起此标志的变化.
2013.01.24  route 命令不再打印 aodv 路由表, 加入 aodvs 命令打印 aodv 路由表.
2013.06.21  adovs 命令加入对目标跳数和单项连接的显示.
2013.08.22  route_msg 加入 metric 字段, 但当前无用.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_NET_EN > 0
#include "sys/route.h"
#include "net/if.h"
#include "lwip/tcpip.h"
#include "lwip/inet.h"
#include "lwip/api.h"
#include "lwip/netif.h"
#include "src/netif/aodv/aodv_route.h"                                  /*  AODV 路由表                 */
/*********************************************************************************************************
  路由表节点
*********************************************************************************************************/
typedef struct {
    LW_LIST_LINE    RTE_lineManage;                                     /*  management list             */
    ip_addr_t       RTE_ipaddrDest;                                     /*  dest address                */
    struct netif   *RTE_pnetif;                                         /*  net device                  */
    CHAR            RTE_cNetifName[IF_NAMESIZE];                        /*  net device name             */
    UINT            RTE_uiFlag;                                         /*  route entry flag            */
} LW_RT_ENTRY;
typedef LW_RT_ENTRY        *PLW_RT_ENTRY;

#define LW_RT_FLAG_U        0x01                                        /*  valid                       */
#define LW_RT_FLAG_G        0x02                                        /*  to route                    */
#define LW_RT_FLAG_H        0x04                                        /*  to host                     */
#define LW_RT_FLAG_D        0x08                                        /*  icmp create                 */
#define LW_RT_FLAG_M        0x10                                        /*  icmp modify                 */
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
static UINT             _G_uiTotalNum;
static UINT             _G_uiActiveNum;

#define LW_RT_TABLESIZE     32                                          /*  2 ^ 5 个入口                */
#define LW_RT_TABLEMASK     (LW_RT_TABLESIZE - 1)
#define LW_RT_TABLESHIFT    (32 - 5)                                    /*  使用 ip 地址高 5 位为 hash  */

#define LW_RT_HASHINDEX(pipaddr)    \
        (((pipaddr)->addr >> LW_RT_TABLESHIFT) & LW_RT_TABLEMASK)
        
static PLW_LIST_LINE    _G_plineRtHashHeader[LW_RT_TABLESIZE];
/*********************************************************************************************************
  CACHE
*********************************************************************************************************/
typedef struct {
    ip_addr_t       RTCACHE_ipaddrDest;
    PLW_RT_ENTRY    RTCACHE_rteCache;
} LW_RT_CACHE;
static LW_RT_CACHE      _G_rtcache;

#define LW_RT_CACHE_INVAL(prte)  \
        if (_G_rtcache.RTCACHE_rteCache == prte) {  \
            _G_rtcache.RTCACHE_rteCache        = LW_NULL;   \
            _G_rtcache.RTCACHE_ipaddrDest.addr = IPADDR_ANY;    \
        }
/*********************************************************************************************************
** 函数名称: __rtSafeRun
** 功能描述: 安全的运行一个回调函数, 不能破坏协议栈
** 输　入  : pfuncHook     执行函数
**           pvArg0...5    参数
** 输　出  : 执行结果
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtSafeRun (FUNCPTR  pfuncHook,
                        PVOID    pvArg0,
                        PVOID    pvArg1,
                        PVOID    pvArg2,
                        PVOID    pvArg3,
                        PVOID    pvArg4,
                        PVOID    pvArg5)
{
#if LWIP_TCPIP_CORE_LOCKING < 1
#error sylixos need LWIP_TCPIP_CORE_LOCKING > 0
#endif                                                                  /*  LWIP_TCPIP_CORE_LOCKING     */

    INT     iError;

    if (sys_mutex_valid(&lock_tcpip_core)) {
        LOCK_TCPIP_CORE();
        iError = pfuncHook(pvArg0, pvArg1, pvArg2, pvArg3, pvArg4, pvArg5);
        UNLOCK_TCPIP_CORE();
    } else {
        iError = pfuncHook(pvArg0, pvArg1, pvArg2, pvArg3, pvArg4, pvArg5);
    }
    
    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __rtMatch
** 功能描述: 匹配一个 sylixos 路由条目 (优先选择主机路由, 然后选择网络路由)
** 输　入  : pipaddrDest    目标地址
** 输　出  : 如果查询到, 则返回路由表节点
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static PLW_RT_ENTRY __rtMatch (ip_addr_t *pipaddrDest)
{
    INT             iHash = LW_RT_HASHINDEX(pipaddrDest);
    PLW_LIST_LINE   plineTemp;
    PLW_RT_ENTRY    prte;
    PLW_RT_ENTRY    prteNet = LW_NULL;                                  /*  缓冲遍历是产生的网络地址    */
    
    for (plineTemp  = _G_plineRtHashHeader[iHash];
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
         
        prte = (PLW_RT_ENTRY)plineTemp;
        if (prte->RTE_uiFlag & LW_RT_FLAG_U) {                          /*  路由节点有效                */
            if ((prte->RTE_uiFlag & LW_RT_FLAG_H) &&
                (prte->RTE_ipaddrDest.addr == pipaddrDest->addr)) {     /*  主机匹配检查                */
                return  (prte);
            
            } else if ((prteNet == LW_NULL) && 
                       (ip_addr_netcmp(pipaddrDest, 
                                       &(prte->RTE_pnetif->ip_addr),
                                       &(prte->RTE_pnetif->netmask)))) {/*  网络匹配检查                */
                prteNet = prte;
            }
        }
    }
    
    return  (prteNet);
}
/*********************************************************************************************************
** 函数名称: __rtFind
** 功能描述: 查找一个 sylixos 路由条目
** 输　入  : pipaddrDest   目标地址
**           ulFlag        需要满足的标志
** 输　出  : 如果查询到, 则返回路由表节点
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static PLW_RT_ENTRY __rtFind (ip_addr_t *pipaddrDest, UINT  ulFlag)
{
    INT             iHash = LW_RT_HASHINDEX(pipaddrDest);
    PLW_LIST_LINE   plineTemp;
    PLW_RT_ENTRY    prte;
    
    for (plineTemp  = _G_plineRtHashHeader[iHash];
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
         
        prte = (PLW_RT_ENTRY)plineTemp;
        
        if ((prte->RTE_ipaddrDest.addr == pipaddrDest->addr) &&
            ((prte->RTE_uiFlag & ulFlag) == ulFlag)) {                  /*  满足条件直接返回            */
            return  (prte);
        }
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: __rtFind
** 功能描述: 遍历 sylixos 路由条目
** 输　入  : pfuncHook     遍历回调函数
**           pvArg0...4    参数
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __rtTraversal (VOIDFUNCPTR  pfuncHook,
                           PVOID        pvArg0,
                           PVOID        pvArg1,
                           PVOID        pvArg2,
                           PVOID        pvArg3,
                           PVOID        pvArg4)
{
    INT             i;
    PLW_LIST_LINE   plineTemp;
    PLW_RT_ENTRY    prte;
    
    for (i = 0; i < LW_RT_TABLESIZE; i++) {
        for (plineTemp  = _G_plineRtHashHeader[i];
             plineTemp != LW_NULL;
             plineTemp  = _list_line_get_next(plineTemp)) {
             
            prte = (PLW_RT_ENTRY)plineTemp;
            if (prte->RTE_pnetif) {
                prte->RTE_uiFlag |= LW_RT_FLAG_U;                       /*  路由有效                    */
                if ((prte->RTE_ipaddrDest.addr != IPADDR_BROADCAST) &&
                    !ip_addr_netcmp(&prte->RTE_ipaddrDest,
                                    &(prte->RTE_pnetif->ip_addr),
                                    &(prte->RTE_pnetif->netmask))) {    /*  不在同一网络                */
                    prte->RTE_uiFlag |= LW_RT_FLAG_G;                   /*  间接路由                    */
                } else {
                    prte->RTE_uiFlag &= ~LW_RT_FLAG_G;
                }
            }
            pfuncHook(prte, pvArg0, pvArg1, pvArg2, pvArg3, pvArg4);
        }
    }
}
/*********************************************************************************************************
** 函数名称: __rtFind
** 功能描述: 遍历 sylixos 路由条目
** 输　入  : pfuncHook     遍历回调函数
**           pvArg0...4    参数
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __rtBuildinTraversal (VOIDFUNCPTR  pfuncHook,
                                  PVOID        pvArg0,
                                  PVOID        pvArg1,
                                  PVOID        pvArg2,
                                  PVOID        pvArg3,
                                  PVOID        pvArg4)
{
    struct netif  *netif;
    LW_RT_ENTRY    rte;
    
    for (netif = netif_list; netif != NULL; netif = netif->next) {
        rte.RTE_ipaddrDest.addr = netif->ip_addr.addr;
        rte.RTE_pnetif          = netif;
        rte.RTE_cNetifName[0]   = netif->name[0];
        rte.RTE_cNetifName[1]   = netif->name[1];
        rte.RTE_cNetifName[2]   = (char)(netif->num + '0');
        rte.RTE_cNetifName[3]   = PX_EOS;
        rte.RTE_uiFlag          = LW_RT_FLAG_U | LW_RT_FLAG_H;
        
        pfuncHook(&rte, pvArg0, pvArg1, pvArg2, pvArg3, pvArg4);
        
        rte.RTE_ipaddrDest.addr = netif->ip_addr.addr & netif->netmask.addr;
        rte.RTE_uiFlag          = LW_RT_FLAG_U;
        
        pfuncHook(&rte, pvArg0, pvArg1, pvArg2, pvArg3, pvArg4);
    }
    
    if (netif_default) {                                                /*  默认出口                    */
        rte.RTE_ipaddrDest.addr = IPADDR_ANY;                           /*  目的地址为 0                */
        rte.RTE_pnetif          = netif_default;
        rte.RTE_cNetifName[0]   = netif_default->name[0];
        rte.RTE_cNetifName[1]   = netif_default->name[1];
        rte.RTE_cNetifName[2]   = (char)(netif_default->num + '0');
        rte.RTE_cNetifName[3]   = PX_EOS;
        rte.RTE_uiFlag          = LW_RT_FLAG_U | LW_RT_FLAG_G;
        
        pfuncHook(&rte, pvArg0, pvArg1, pvArg2, pvArg3, pvArg4);
    }
}
/*********************************************************************************************************
** 函数名称: __rtAddCallback
** 功能描述: 添加一个 sylixos 路由条目(加入链表, 在 TCPIP 上下文中执行)
** 输　入  : ipaddrDest    目的地址
**           uiFlag        flag
**           pcNetifName   路由设备接口名
** 输　出  : ERROR_NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtAddCallback (ip_addr_t *pipaddrDest, UINT  uiFlag, CPCHAR  pcNetifName)
{
    INT             iHash = LW_RT_HASHINDEX(pipaddrDest);
    PLW_RT_ENTRY    prte;
    
    prte = (PLW_RT_ENTRY)__SHEAP_ALLOC(sizeof(LW_RT_ENTRY));
    if (prte == LW_NULL) {
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (PX_ERROR);
    }
    
    prte->RTE_ipaddrDest = *pipaddrDest;
    prte->RTE_pnetif = netif_find((PCHAR)pcNetifName);
    prte->RTE_uiFlag = uiFlag;
    lib_strlcpy(prte->RTE_cNetifName, pcNetifName, IF_NAMESIZE);
    if (prte->RTE_pnetif) {
        prte->RTE_uiFlag |= LW_RT_FLAG_U;                               /*  路由有效                    */
        _G_uiActiveNum++;
    }
    _G_uiTotalNum++;
    
    _List_Line_Add_Ahead(&prte->RTE_lineManage, &_G_plineRtHashHeader[iHash]);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __rtDelCallback
** 功能描述: 删除一个 sylixos 路由条目(从链表中移除, 在 TCPIP 上下文中执行)
** 输　入  : pipaddrDest    目标地址
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtDelCallback (ip_addr_t *pipaddrDest)
{
    PLW_RT_ENTRY    prte;
    
    prte = __rtFind(pipaddrDest, 0);
    if (prte) {
        INT     iHash = LW_RT_HASHINDEX(&prte->RTE_ipaddrDest);
        
        if (prte->RTE_uiFlag & LW_RT_FLAG_U) {
            _G_uiActiveNum--;
        }
        _G_uiTotalNum--;
        
        LW_RT_CACHE_INVAL(prte);
        
        _List_Line_Del(&prte->RTE_lineManage, &_G_plineRtHashHeader[iHash]);
        
        __SHEAP_FREE(prte);
        
        return  (ERROR_NONE);
    }
    
    _ErrorHandle(EINVAL);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __rtChangeCallback
** 功能描述: 修改一个 sylixos 路由条目(在 TCPIP 上下文中执行)
** 输　入  : ipaddrDest    目的地址
**           uiFlag        flag
**           pcNetifName   路由设备接口名
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtChangeCallback (ip_addr_t *pipaddrDest, UINT  uiFlag, CPCHAR  pcNetifName)
{
    PLW_RT_ENTRY    prte;
    
    prte = __rtFind(pipaddrDest, 0);
    if (prte) {
        INT     iHash = LW_RT_HASHINDEX(&prte->RTE_ipaddrDest);
        
        if (prte->RTE_uiFlag & LW_RT_FLAG_U) {
            _G_uiActiveNum--;
        }
        _G_uiTotalNum--;
        
        LW_RT_CACHE_INVAL(prte);
        
        _List_Line_Del(&prte->RTE_lineManage, &_G_plineRtHashHeader[iHash]);
        
        prte->RTE_ipaddrDest = *pipaddrDest;
        prte->RTE_pnetif = netif_find((PCHAR)pcNetifName);
        prte->RTE_uiFlag = uiFlag;
        lib_strlcpy(prte->RTE_cNetifName, pcNetifName, IF_NAMESIZE);
        if (prte->RTE_pnetif) {
            prte->RTE_uiFlag |= LW_RT_FLAG_U;                           /*  路由有效                    */
            _G_uiActiveNum++;
        }
        _G_uiTotalNum++;
        
        iHash = LW_RT_HASHINDEX(&prte->RTE_ipaddrDest);
        
        _List_Line_Add_Ahead(&prte->RTE_lineManage, &_G_plineRtHashHeader[iHash]);
        
        return  (ERROR_NONE);
    }
    
    _ErrorHandle(EINVAL);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __rtSetGwCallback
** 功能描述: 设置网络接口的网关(在 TCPIP 上下文中执行)
** 输　入  : pipaddrGw     网关地址
**           iDefault      是否为默认网关
**           pcNetifName   网络接口名
** 输　出  : ERROR_NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtSetGwCallback (ip_addr_t *pipaddrGw, INT  iDefault, CPCHAR  pcNetifName)
{
    struct netif  *pnetif = netif_find((PCHAR)pcNetifName);
    
    if (pnetif) {
        netif_set_gw(pnetif, pipaddrGw);
        if (iDefault) {
            netif_set_default(pnetif);
        }
        return  (ERROR_NONE);
    
    } else {
        _ErrorHandle(ENXIO);
        return  (PX_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: __rtGetCallback
** 功能描述: 获取路由表接口(在 TCPIP 上下文中执行)
** 输　入  : prte          路由条目
**           flag          获取的类型掩码 
**           msgbuf        缓冲表
**           num           缓冲表总数
**           piIndex       当前下标
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __rtGetCallback (PLW_RT_ENTRY       prte, 
                             u_char             flag, 
                             struct route_msg  *msgbuf, 
                             size_t             num, 
                             int               *piIndex)
{
    struct  netif  *pnetif = prte->RTE_pnetif;
    u_char  ucGetFlag = 0;

    if (*piIndex >= num) {
        return;
    }
    
    if (prte->RTE_uiFlag & LW_RT_FLAG_U) {                              /*  有效路由                    */
        ucGetFlag |= ROUTE_RTF_UP;
    }
    if (prte->RTE_uiFlag & LW_RT_FLAG_H) {
        ucGetFlag |= ROUTE_RTF_HOST;                                    /*  主机路由                    */
    }
    if (prte->RTE_uiFlag & LW_RT_FLAG_G) {
        ucGetFlag |= ROUTE_RTF_GATEWAY;                                 /*  目的是一个网关的路由        */
    }
    
    if ((ucGetFlag & flag) == flag) {                                   /*  满足获取条件                */
        msgbuf[*piIndex].rm_flag        = ucGetFlag;
        msgbuf[*piIndex].rm_metric      = 1;
        msgbuf[*piIndex].rm_dst.s_addr  = prte->RTE_ipaddrDest.addr;    /*  目的地址                    */
        if (pnetif) {
            msgbuf[*piIndex].rm_gw.s_addr    = pnetif->gw.addr;         /*  网关地址                    */
            msgbuf[*piIndex].rm_mask.s_addr  = pnetif->netmask.addr;    /*  子网掩码                    */
            msgbuf[*piIndex].rm_if.s_addr    = pnetif->ip_addr.addr;    /*  网络接口 IP                 */
            
        } else {
            msgbuf[*piIndex].rm_gw.s_addr   = IPADDR_ANY;
            msgbuf[*piIndex].rm_mask.s_addr = IPADDR_ANY;
            msgbuf[*piIndex].rm_if.s_addr   = IPADDR_ANY;
        }
        lib_memcpy(msgbuf[*piIndex].rm_ifname, 
                   prte->RTE_cNetifName, IF_NAMESIZE);                  /*  网卡名称                    */
        (*piIndex)++;                                                   /*  索引向下移动                */
    }
}
/*********************************************************************************************************
** 函数名称: __rtAdd
** 功能描述: 添加一个 sylixos 路由条目
** 输　入  : ipaddrDest    目的地址
**           uiFlag        flag
**           pcNetifName   路由设备接口名
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtAdd (ip_addr_t *pipaddrDest, UINT  uiFlag, CPCHAR  pcNetifName)
{
    INT     iError;
    
    if (pipaddrDest == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    iError = __rtSafeRun(__rtAddCallback, pipaddrDest, (PVOID)uiFlag, (PVOID)pcNetifName, 0, 0, 0);
    
    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __rtDelete
** 功能描述: 移除一个 sylixos 路由条目
** 输　入  : ipaddrDest    目标地址
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtDel (ip_addr_t *pipaddrDest)
{
    INT     iError;

    if (pipaddrDest == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    iError = __rtSafeRun(__rtDelCallback, pipaddrDest, 0, 0, 0, 0, 0);

    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __rtChange
** 功能描述: 改变一个 sylixos 路由条目
** 输　入  : ipaddrDest    目的地址
**           uiFlag        flag
**           pcNetifName   路由设备接口名
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtChange (ip_addr_t *pipaddrDest, UINT  uiFlag, CPCHAR  pcNetifName)
{
    INT     iError;
    
    if (pipaddrDest == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    iError = __rtSafeRun(__rtChangeCallback, pipaddrDest, (PVOID)uiFlag, (PVOID)pcNetifName, 0, 0, 0);
    
    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __rtSetGw
** 功能描述: 设置一个网络接口的网关
** 输　入  : ipaddrDest    网关地址
**           iDefault      是否为默认网关
**           pcNetifName   设备接口名
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtSetGw (ip_addr_t *pipaddrDest, INT  iDefault, CPCHAR  pcNetifName)
{
    INT     iError;
    
    if (pipaddrDest == LW_NULL) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    iError = __rtSafeRun(__rtSetGwCallback, pipaddrDest, (PVOID)iDefault, (PVOID)pcNetifName, 0, 0, 0);
    
    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __rtGet
** 功能描述: 获取路由表
** 输　入  : flag          获取的类型掩码 
**           msgbuf        路由表缓冲区
**           num           缓冲数量
** 输　出  : 获取的数量
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT __rtGet (u_char  flag, struct route_msg  *msgbuf, size_t  num)
{
    INT     iNum = 0;
    
    __rtSafeRun((FUNCPTR)__rtTraversal, (void *)__rtGetCallback,
                (PVOID)(ULONG)flag, (PVOID)msgbuf, (PVOID)num, (PVOID)&iNum, 0);
                
    __rtSafeRun((FUNCPTR)__rtBuildinTraversal, (void *)__rtGetCallback,
                (PVOID)(ULONG)flag, (PVOID)msgbuf, (PVOID)num, (PVOID)&iNum, 0);

    return  (iNum);
}
/*********************************************************************************************************
** 函数名称: __rtNeifAddCallback
** 功能描述: 添加网络接口回调(网络安全)
** 输　入  : netif     网络接口
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID __rtNeifAddCallback (struct netif *netif)
{
    INT             i;
    PLW_LIST_LINE   plineTemp;
    PLW_RT_ENTRY    prte;
    CHAR            cNetifName[IF_NAMESIZE];

    if_indextoname(netif->num, cNetifName);

    for (i = 0; i < LW_RT_TABLESIZE; i++) {
        for (plineTemp  = _G_plineRtHashHeader[i];
             plineTemp != LW_NULL;
             plineTemp  = _list_line_get_next(plineTemp)) {

            prte = (PLW_RT_ENTRY)plineTemp;
            if (lib_strncmp(cNetifName, prte->RTE_cNetifName, IF_NAMESIZE) == 0) {
                prte->RTE_pnetif  = netif;                              /*  对应的路由条目有效          */
                prte->RTE_uiFlag |= LW_RT_FLAG_U;
                _G_uiActiveNum++;
            }
        }
    }
}
/*********************************************************************************************************
** 函数名称: __rtNeifRemoveCallback
** 功能描述: 移除网络接口回调(网络安全)
** 输　入  : netif     网络接口
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID __rtNeifRemoveCallback (struct netif *netif)
{
    INT             i;
    PLW_LIST_LINE   plineTemp;
    PLW_RT_ENTRY    prte;

    for (i = 0; i < LW_RT_TABLESIZE; i++) {
        for (plineTemp  = _G_plineRtHashHeader[i];
             plineTemp != LW_NULL;
             plineTemp  = _list_line_get_next(plineTemp)) {

            prte = (PLW_RT_ENTRY)plineTemp;
            if (prte->RTE_pnetif == netif) {                            /*  对应的路由条目应该无效      */
                prte->RTE_pnetif  = LW_NULL;
                prte->RTE_uiFlag &= ~LW_RT_FLAG_U;

                LW_RT_CACHE_INVAL(prte);                                /*  无效对应 cache              */
                _G_uiActiveNum--;
            }
        }
    }
}
/*********************************************************************************************************
** 函数名称: __rtNeifAddHook
** 功能描述: 添加网络接口回调
** 输　入  : netif     网络接口
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID rt_netif_add_hook (struct netif *netif)
{
    __rtSafeRun((FUNCPTR)__rtNeifAddCallback, (PVOID)netif, 0, 0, 0, 0, 0);
}
/*********************************************************************************************************
** 函数名称: __rtNeifRemoveHook
** 功能描述: 移除网络接口回调
** 输　入  : netif     网络接口
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID rt_netif_remove_hook (struct netif *netif)
{
    __rtSafeRun((FUNCPTR)__rtNeifRemoveCallback, (PVOID)netif, 0, 0, 0, 0, 0);
}
/*********************************************************************************************************
** 函数名称: sys_ip_route_hook
** 功能描述: sylixos 路由查找接口
** 输　入  : ipaddrDest    目标地址
** 输　出  : 如果查询到, 则返回路由接口
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
struct netif *sys_ip_route_hook (ip_addr_t *pipaddrDest)
{
    PLW_RT_ENTRY    prte;

    if (_G_uiActiveNum == 0) {
        return  (LW_NULL);
    }
    
    if ((_G_rtcache.RTCACHE_rteCache) &&
        (_G_rtcache.RTCACHE_rteCache->RTE_uiFlag & LW_RT_FLAG_U) &&
        (_G_rtcache.RTCACHE_ipaddrDest.addr == pipaddrDest->addr)) {    /*  首先判断 CACHE 是否命中     */
        return  (_G_rtcache.RTCACHE_rteCache->RTE_pnetif);
    }
    
    prte = __rtMatch(pipaddrDest);                                      /*  寻找有效目的路由            */
    if (prte) {
        _G_rtcache.RTCACHE_rteCache = prte;
        _G_rtcache.RTCACHE_ipaddrDest.addr = pipaddrDest->addr;
        return  (prte->RTE_pnetif);
    }
    
    return  (LW_NULL);                                                  /*  使用 lwip 内建路由          */
}
/*********************************************************************************************************
** 函数名称: __rtEntryPrint
** 功能描述: 打印一条路由信息
** 输　入  : prte      路由表节点
**           pcBuffer  缓冲区
**           stSize    缓冲区大小
**           pstOffset 偏移量
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_SHELL_EN > 0

static VOID __rtEntryPrint (PLW_RT_ENTRY prte, PCHAR  pcBuffer, size_t  stSize, size_t *pstOffset)
{
    CHAR    cIpDest[INET_ADDRSTRLEN];
    CHAR    cGateway[INET_ADDRSTRLEN] = "*";
    CHAR    cMask[INET_ADDRSTRLEN]    = "*";
    CHAR    cFlag[6] = "\0";
    
    ipaddr_ntoa_r(&prte->RTE_ipaddrDest, cIpDest, INET_ADDRSTRLEN);
    if (prte->RTE_pnetif) {
        ipaddr_ntoa_r(&prte->RTE_pnetif->gw, cGateway, INET_ADDRSTRLEN);
        ipaddr_ntoa_r(&prte->RTE_pnetif->netmask, cMask, INET_ADDRSTRLEN);
    }
    
    if (prte->RTE_uiFlag & LW_RT_FLAG_U) {
        lib_strcat(cFlag, "U");
    }
    if (prte->RTE_uiFlag & LW_RT_FLAG_G) {
        lib_strcat(cFlag, "G");
    } else {
        lib_strcpy(cGateway, "*");                                      /*  直接路由 (不需要打印网关)   */
    }
    if (prte->RTE_uiFlag & LW_RT_FLAG_H) {
        lib_strcat(cFlag, "H");
    }
    if (prte->RTE_uiFlag & LW_RT_FLAG_D) {
        lib_strcat(cFlag, "D");
    }
    if (prte->RTE_uiFlag & LW_RT_FLAG_M) {
        lib_strcat(cFlag, "M");
    }
    
    *pstOffset = bnprintf(pcBuffer, stSize, *pstOffset,
                          "%-18s %-18s %-18s %-8s %-3s\n",
                          cIpDest, cGateway, cMask, cFlag, prte->RTE_cNetifName);
}
/*********************************************************************************************************
** 函数名称: __aodvEntryPrint
** 功能描述: 打印 aodv 路由信息
** 输　入  : rt        aodv 路由条目
**           pcBuffer  缓冲区
**           stSize    缓冲区大小
**           pstOffset 偏移量
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __aodvEntryPrint (struct aodv_rtnode *rt, PCHAR  pcBuffer, size_t  stSize, size_t *pstOffset)
{
    CHAR    cIpDest[INET_ADDRSTRLEN];
    CHAR    cNextHop[INET_ADDRSTRLEN];
    CHAR    cMask[INET_ADDRSTRLEN];
    CHAR    cFlag[16] = "\0";
    CHAR    cIfName[IF_NAMESIZE] = "\0";
    
    inet_ntoa_r(rt->dest_addr, cIpDest, INET_ADDRSTRLEN);
    inet_ntoa_r(rt->next_hop, cNextHop, INET_ADDRSTRLEN);
    
    if (rt->state & AODV_VALID) {
        lib_strcat(cFlag, "U");
    }
    if (rt->hcnt > 0) {
        lib_strcat(cFlag, "G");
    }
    if ((rt->flags & AODV_RT_GATEWAY) == 0) {
        lib_strcat(cFlag, "H");
    }
    if ((rt->flags & AODV_RT_UNIDIR) == 0) {                            /*  单向连接                    */
        lib_strcat(cFlag, "-ud");
    }
    
    /*
     *  aodv 路由节点网络接口一定有效
     */
    ipaddr_ntoa_r(&rt->netif->netmask, cMask, INET_ADDRSTRLEN);
    if_indextoname(rt->netif->num, cIfName);
    
    *pstOffset = bnprintf(pcBuffer, stSize, *pstOffset,
                          "%-18s %-18s %-18s %-8s %8d %-3s\n",
                          cIpDest, cNextHop, cMask, cFlag, rt->hcnt, cIfName);
}
/*********************************************************************************************************
** 函数名称: __buildinRtPrint
** 功能描述: 打印 lwip 内建路由信息
** 输　入  : rt            aodv 路由条目
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __buildinRtPrint (PCHAR  pcBuffer, size_t  stSize, size_t *pstOffset)
{
    struct netif *netif;
    ip_addr_t     ipaddr;
    
    CHAR    cIpDest[INET_ADDRSTRLEN];
    CHAR    cGateway[INET_ADDRSTRLEN] = "*";
    CHAR    cMask[INET_ADDRSTRLEN];
    CHAR    cFlag[6];
    CHAR    cIfName[IF_NAMESIZE] = "\0";
    
    for (netif = netif_list; netif != NULL; netif = netif->next) {
        ipaddr.addr = netif->ip_addr.addr & netif->netmask.addr;
        ipaddr_ntoa_r(&ipaddr, cIpDest, INET_ADDRSTRLEN);
        ipaddr_ntoa_r(&netif->netmask, cMask, INET_ADDRSTRLEN);
        if_indextoname(netif->num, cIfName);
        lib_strcpy(cFlag, "U");
        
        *pstOffset = bnprintf(pcBuffer, stSize, *pstOffset,
                              "%-18s %-18s %-18s %-8s %-3s\n",
                              cIpDest, cGateway, cMask, cFlag, cIfName);

        ipaddr_ntoa_r(&netif->ip_addr, cIpDest, INET_ADDRSTRLEN);
        lib_strcpy(cFlag, "UH");
        
        *pstOffset = bnprintf(pcBuffer, stSize, *pstOffset,
                              "%-18s %-18s %-18s %-8s %-3s\n",
                              cIpDest, cGateway, cMask, cFlag, cIfName);
    }
    
    if (netif_default) {
        ipaddr_ntoa_r(&netif_default->gw, cGateway, INET_ADDRSTRLEN);
        ipaddr_ntoa_r(&netif_default->netmask, cMask, INET_ADDRSTRLEN);
        if_indextoname(netif_default->num, cIfName);
        lib_strcpy(cFlag, "UG");

        *pstOffset = bnprintf(pcBuffer, stSize, *pstOffset,
                              "%-18s %-18s %-18s %-8s %-3s\n",
                              "default", cGateway, cMask, cFlag, cIfName);
    }
}
/*********************************************************************************************************
** 函数名称: __rtTotalEntry
** 功能描述: 获得三组路由(包含 kernel, aodv, build-in)入口, 最大的一组个数
** 输　入  : NONE
** 输　出  : 最大的一组个数
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static UINT __rtEntryMaxNum (VOID)
{
    UINT    uiMax;

    /*
     *  total entry include : kernel + aodv + (netifnum * 2) + default
     */
    uiMax = _G_uiTotalNum;                                              /*  kernel route entry          */

    if (uiMax < AODV_RT_NUM_ENTRIES()) {
        uiMax = AODV_RT_NUM_ENTRIES();                                  /*  aodv route entry            */
    }

    if (uiMax < (netif_get_num() * 2) + 1) {                            /*  lwip build-in route entry   */
        uiMax = (netif_get_num() * 2) + 1;
    }

    return  (uiMax);
}
/*********************************************************************************************************
** 函数名称: __tshellRoute
** 功能描述: 系统命令 "route"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  __tshellRoute (INT  iArgC, PCHAR  *ppcArgV)
{
#define LW_RT_PRINT_SIZE        74

    INT         iError;
    FUNCPTR     pfuncAddOrChange;
    PCHAR       pcOpAddorChange;

    UINT        uiFlag = 0;
    ip_addr_t   ipaddr;
    CHAR        cNetifName[IF_NAMESIZE];

    if (iArgC == 1) {
        PCHAR   pcBuffer;
        size_t  stSize;
        size_t  stOffset;
        UINT    uiNumEntries;

        uiNumEntries = (UINT)__rtSafeRun((FUNCPTR)__rtEntryMaxNum, 0, 0, 0, 0, 0, 0);
        if (uiNumEntries == 0) {
            printf("no route entry.\n");
            return  (ERROR_NONE);
        }

        /*
         *  在 net safe 状态下不允许调用 printf 等使用 IO 的语句. 所以只能打印到缓冲区中,
         *  然后统一使用 IO 打印.
         */
        stSize   = LW_RT_PRINT_SIZE * uiNumEntries;
        pcBuffer = (PCHAR)__SHEAP_ALLOC(stSize);
        if (pcBuffer == LW_NULL) {
            printf("system low memory.\n");
            return  (PX_ERROR);
        }

        /*
         *  打印 kernel route entry
         */
        stOffset = 0;
        printf("kernel routing tables\n");
        printf("Destination        Gateway            Mask               Flag     Interface\n");
        __rtSafeRun((FUNCPTR)__rtTraversal, (void *)__rtEntryPrint,
                    pcBuffer, (PVOID)stSize, &stOffset, 0, 0);
        if (stOffset > 0) {
            fwrite(pcBuffer, stOffset, 1, stdout);
            fflush(stdout);                                             /*  这里必须确保输出完成        */
        }
        
        /*
         *  打印 lwip build-in route entry
         */
        stOffset = 0;
        printf("\nbuild-in routing tables\n");
        printf("Destination        Gateway            Mask               Flag     Interface\n");
        __rtSafeRun((FUNCPTR)__buildinRtPrint,
                    pcBuffer, (PVOID)stSize, &stOffset, 0, 0, 0);
        if (stOffset > 0) {
            fwrite(pcBuffer, stOffset, 1, stdout);
            fflush(stdout);                                             /*  这里必须确保输出完成        */
        }

        __SHEAP_FREE(pcBuffer);

        return  (ERROR_NONE);
        
    } else {

        if (lib_strcmp(ppcArgV[1], "add") == 0) {
            pfuncAddOrChange = __rtAdd;
            pcOpAddorChange  = "add";

        } else if (lib_strcmp(ppcArgV[1], "change") == 0) {
            pfuncAddOrChange = __rtChange;
            pcOpAddorChange  = "change";

        } else {
            pfuncAddOrChange = LW_NULL;
        }

        if (pfuncAddOrChange && iArgC == 6) {                           /*  添加或者修改路由表          */

            if (!ipaddr_aton(ppcArgV[3], &ipaddr)) {
                printf("inet address format error.\n");
                goto    __error_handle;
            }

            if (lib_strcmp(ppcArgV[4], "if") == 0) {                    /*  使用 ifindex 查询网卡       */
                INT   iIndex = lib_atoi(ppcArgV[5]);
                if (if_indextoname(iIndex, cNetifName) == LW_NULL) {
                    printf("can not find net interface with ifindex %d.\n", iIndex);
                    goto    __error_handle;
                }

            } else if (lib_strcmp(ppcArgV[4], "dev") == 0) {
                lib_strlcpy(cNetifName, ppcArgV[5], IF_NAMESIZE);

            } else {
                printf("net interface argument error.\n");
                goto    __error_handle;
            }

            if (lib_strcmp(ppcArgV[2], "-host") == 0) {                 /*  主机路由                    */
                uiFlag |= LW_RT_FLAG_H;

            } else if (lib_strcmp(ppcArgV[2], "-net") != 0) {           /*  非网络路由                  */
                printf("route add must determine -host or -net.\n");
                goto    __error_handle;
            }

            iError = pfuncAddOrChange(&ipaddr, uiFlag, cNetifName);     /*  操作路由表                  */
            if (iError >= 0) {
                printf("kernel route entry %s %s successful.\n", ppcArgV[3], pcOpAddorChange);
                return  (ERROR_NONE);
            }

        } else if ((lib_strcmp(ppcArgV[1], "del") == 0) && iArgC == 3) {/*  删除一个路由表项            */

            if (!ipaddr_aton(ppcArgV[2], &ipaddr)) {
                printf("inet address format error.\n");
                goto    __error_handle;
            }

            iError = __rtDel(&ipaddr);
            if (iError >= 0) {
                printf("kernel route entry %s delete successful.\n", ppcArgV[2]);
                return  (ERROR_NONE);
            }
        }
    }
    
__error_handle:
    printf("argments error!\n");
    return  (-ERROR_TSHELL_EPARAM);
}
/*********************************************************************************************************
** 函数名称: __tshellAodvs
** 功能描述: 系统命令 "aodvs"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellAodvs (INT  iArgC, PCHAR  *ppcArgV)
{
    PCHAR   pcBuffer;
    size_t  stSize;
    size_t  stOffset;
    UINT    uiNumEntries;

    uiNumEntries = (UINT)__rtSafeRun((FUNCPTR)__rtEntryMaxNum, 0, 0, 0, 0, 0, 0);
    if (uiNumEntries == 0) {
        printf("no route entry.\n");
        return  (ERROR_NONE);
    }
    
    /*
     *  在 net safe 状态下不允许调用 printf 等使用 IO 的语句. 所以只能打印到缓冲区中,
     *  然后统一使用 IO 打印.
     */
    stSize   = LW_RT_PRINT_SIZE * uiNumEntries;
    pcBuffer = (PCHAR)__SHEAP_ALLOC(stSize);
    if (pcBuffer == LW_NULL) {
        printf("system low memory.\n");
        return  (PX_ERROR);
    }

    /*
     *  打印 aodv route entry
     */
    stOffset = 0;
    printf("aodv routing tables\n");
    printf("Destination        Gateway            Mask               Flag     Hops     Interface\n");
    __rtSafeRun((FUNCPTR)aodv_rt_traversal, (void *)__aodvEntryPrint,
                pcBuffer, (PVOID)stSize, &stOffset, 0, 0);
    if (stOffset > 0) {
        fwrite(pcBuffer, stOffset, 1, stdout);
        fflush(stdout);                                                 /*  这里必须确保输出完成        */
    }

    __SHEAP_FREE(pcBuffer);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellRouteInit
** 功能描述: 注册路由器命令
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID __tshellRouteInit (VOID)
{
    API_TShellKeywordAdd("route", __tshellRoute);
    API_TShellFormatAdd("route", " [add | del | change] {-host | -net} [addr] [[dev] | if]");
    API_TShellHelpAdd("route",   "show, add, del, change route table\n"
                                 "eg. route\n"
                                 "    route add -host 255.255.255.255 dev en1\n"
                                 "    route add -net 192.168.3.0 dev en1\n"
                                 "    route change -net 192.168.3.4 dev en2\n"
                                 "    route del 145.26.122.35\n");
                                 
    API_TShellKeywordAdd("aodvs", __tshellAodvs);
    API_TShellHelpAdd("aodvs",   "show AODV route table\n");
}
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
/*********************************************************************************************************
  以下代码为 2013.01.16 添加
*********************************************************************************************************/
/*********************************************************************************************************
** 函数名称: route_add
** 功能描述: 加入一条路由信息
** 输　入  : pinaddr       路由地址
**           type          HOST / NET / GATEWAY
**           ifname        目标网卡
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API
int  route_add (struct in_addr *pinaddr, int  type, const char *ifname)
{
    ip_addr_t   ipaddr;
    INT         iError;

    if (!pinaddr) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    ipaddr.addr = pinaddr->s_addr;
    
    switch (type) {
    
    case ROUTE_TYPE_HOST:                                               /*  添加一条主机路由            */
        iError = __rtAdd(&ipaddr, LW_RT_FLAG_H, ifname);
        break;
        
    case ROUTE_TYPE_NET:                                                /*  添加一条网络路由            */
        iError = __rtAdd(&ipaddr, 0ul, ifname);
        break;
        
    case ROUTE_TYPE_GATEWAY:                                            /*  添加一个网关                */
        iError = __rtSetGw(&ipaddr, 0, ifname);
        break;
        
    case ROUTE_TYPE_DEFAULT:                                            /*  设置默认网关                */
        iError = __rtSetGw(&ipaddr, 1, ifname);
        break;
        
    default:
        iError = PX_ERROR;
        _ErrorHandle(EINVAL);
    }
    
    return  (iError);
}
/*********************************************************************************************************
** 函数名称: route_delete
** 功能描述: 删除一条路由信息
** 输　入  : pinaddr       路由地址 (HOST or NET)
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API
int  route_delete (struct in_addr *pinaddr)
{
    ip_addr_t   ipaddr;

    if (!pinaddr) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    ipaddr.addr = pinaddr->s_addr;
    
    return  (__rtDel(&ipaddr));
}
/*********************************************************************************************************
** 函数名称: route_change
** 功能描述: 修改一条路由信息
** 输　入  : pinaddr       路由地址
**           type          HOST / NET / GATEWAY
**           ifname        目标网卡
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API
int  route_change (struct in_addr *pinaddr, int  type, const char *ifname)
{
    ip_addr_t   ipaddr;
    INT         iError;

    if (!pinaddr) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    ipaddr.addr = pinaddr->s_addr;
    
    switch (type) {
    
    case ROUTE_TYPE_HOST:                                               /*  修改一条主机路由            */
        iError = __rtChange(&ipaddr, LW_RT_FLAG_H, ifname);
        break;
        
    case ROUTE_TYPE_NET:                                                /*  修改一条网络路由            */
        iError = __rtChange(&ipaddr, 0ul, ifname);
        break;
        
    case ROUTE_TYPE_GATEWAY:                                            /*  修改一个网关                */
        iError = __rtSetGw(&ipaddr, 0, ifname);
        break;
        
    case ROUTE_TYPE_DEFAULT:                                            /*  设置默认网关                */
        iError = __rtSetGw(&ipaddr, 1, ifname);
        break;
        
    default:
        iError = PX_ERROR;
        _ErrorHandle(EINVAL);
    }
    
    return  (iError);
}
/*********************************************************************************************************
** 函数名称: route_getnum
** 功能描述: 获取路由表信息的总数量 (不包含 aodv)
** 输　入  : NONE
** 输　出  : 数量
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API
int  route_getnum (void)
{
    INT     iNum = 1;                                                   /*  1 default gw                */
    
    iNum += (netif_get_num() * 2);                                      /*  每一个网卡两个 buildin 条目 */
    iNum += _G_uiTotalNum;                                              /*  路由数量                    */
    
    return  (iNum);
}
/*********************************************************************************************************
** 函数名称: route_get
** 功能描述: 获取路由表信息
** 输　入  : flag          获取的类型掩码 
**           msgbuf        路由表缓冲区
**           num           缓冲数量
** 输　出  : 获取的数量
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API
int  route_get (u_char flag, struct route_msg  *msgbuf, size_t  num)
{
    return  (__rtGet(flag, msgbuf, num));
}
#endif                                                                  /*  LW_CFG_NET_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
