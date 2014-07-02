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
** 文   件   名: lwip_ppp.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2014 年 01 月 08 日
**
** 描        述: lwip ppp 连接管理器.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "../SylixOS/net/include/net_net.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if (LW_CFG_NET_EN > 0) && (LW_CFG_LWIP_PPP > 0)
#include "lwip/netif.h"
#include "lwip/pppapi.h"
#include "lwip_ppp.h"
#include "net/if_event.h"
/*********************************************************************************************************
  错误打印
*********************************************************************************************************/
#ifndef printk
#define printk
#define KERN_ERR
#endif                                                                  /*  printk                      */
/*********************************************************************************************************
  默认的 PPPoS 缓冲区大小
*********************************************************************************************************/
#define PPPRBUF_SIZE    512
#define PPPWBUF_SIZE    512
/*********************************************************************************************************
  PPP 私有数据
*********************************************************************************************************/
typedef struct {
#define PPP_OS          0
#define PPP_OE          1
#define PPP_OL2TP       2
    UINT                CTXP_uiType;                                    /*  连接类型                    */
    LW_OBJECT_HANDLE    CTXP_ulInput;                                   /*  pppos 输入线程              */
    BOOL                CTXP_bNeedDelete;                               /*  是否需要删除                */
    UINT8               CTXP_ucPhase;                                   /*  最后一次状态                */
    ppp_pcb            *CTXP_pcb;                                       /*  回指向连接控制块            */
} PPP_CTX_PRIV;
/*********************************************************************************************************
** 函数名称: __pppLinkStatCb
** 功能描述: 产生错误码回调
** 输　入  : pcb           pcb 控制块
**           iErrCode      错误码
**           pvCtx         PPP_CTX_PRIV 结构
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID  __pppLinkStatCb (ppp_pcb *pcb, INT iErrCode, PVOID  pvCtx)
{
    switch (iErrCode) {

    case PPPERR_NONE:
        break;

    case PPPERR_PARAM:
        printk(KERN_ERR "[PPP]PPPERR_PARAM\n");
        break;

    case PPPERR_OPEN:
        printk(KERN_ERR "[PPP]PPPERR_OPEN\n");
        break;

    case PPPERR_DEVICE:
        printk(KERN_ERR "[PPP]PPPERR_DEVICE\n");
        break;

    case PPPERR_ALLOC:
        printk(KERN_ERR "[PPP]PPPERR_ALLOC\n");
        break;

    case PPPERR_USER:
        printk(KERN_ERR "[PPP]PPPERR_USER\n");
        break;

    case PPPERR_CONNECT:
        printk(KERN_ERR "[PPP]PPPERR_CONNECT\n");
        break;

    case PPPERR_AUTHFAIL:
        netEventIfAuthFail(ppp_netif(pcb));
        printk(KERN_ERR "[PPP]PPPERR_AUTHFAIL\n");
        break;

    case PPPERR_PROTOCOL:
        printk(KERN_ERR "[PPP]PPPERR_PROTOCOL\n");
        break;

    case PPPERR_PEERDEAD:
        printk(KERN_ERR "[PPP]PPPERR_PEERDEAD\n");
        break;

    case PPPERR_IDLETIMEOUT:
        printk(KERN_ERR "[PPP]PPPERR_IDLETIMEOUT\n");
        break;

    case PPPERR_CONNECTTIME:
        printk(KERN_ERR "[PPP]PPPERR_CONNECTTIME\n");
        break;

    case PPPERR_LOOPBACK:
        printk(KERN_ERR "[PPP]PPPERR_LOOPBACK\n");
        break;

    default:
        printk(KERN_ERR "[PPP]PPPERR_<unknown>\n");
        break;
    }
}
/*********************************************************************************************************
** 函数名称: __pppNotifyPhaseCb
** 功能描述: 状态改变回调
** 输　入  : pcb           pcb 控制块
**           ucPhase       状态码
**           pvCtx         PPP_CTX_PRIV 结构
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID  __pppNotifyPhaseCb (ppp_pcb *pcb, u8_t ucPhase, PVOID  pvCtx)
{
    PPP_CTX_PRIV *pctxp;

    pctxp = (PPP_CTX_PRIV *)pcb->ctx_cb;
    if (pctxp->CTXP_ucPhase == ucPhase) {
        return;
    } else {
        pctxp->CTXP_ucPhase = ucPhase;
    }

    switch (ucPhase) {

    case PPP_PHASE_DEAD:
        netEventIfPppExt(ppp_netif(pcb), NET_EVENT_PPP_DEAD);
        break;

    case PPP_PHASE_INITIALIZE:
        netEventIfPppExt(ppp_netif(pcb), NET_EVENT_PPP_INIT);
        break;

    case PPP_PHASE_SERIALCONN:
        break;

    case PPP_PHASE_DORMANT:
        break;

    case PPP_PHASE_ESTABLISH:
        break;

    case PPP_PHASE_AUTHENTICATE:
        netEventIfPppExt(ppp_netif(pcb), NET_EVENT_PPP_AUTH);
        break;

    case PPP_PHASE_CALLBACK:
        break;

    case PPP_PHASE_NETWORK:
        break;

    case PPP_PHASE_RUNNING:
        netEventIfPppExt(ppp_netif(pcb), NET_EVENT_PPP_RUN);
        break;

    case PPP_PHASE_TERMINATE:
        break;

    case PPP_PHASE_DISCONNECT:
        netEventIfPppExt(ppp_netif(pcb), NET_EVENT_PPP_DISCONN);
        break;

    case PPP_PHASE_HOLDOFF:
        break;

    case PPP_PHASE_MASTER:
        break;

    default:
        printk(KERN_ERR "[PPP]PHASE_<unknown>\n");
        break;
    }
}
/*********************************************************************************************************
** 函数名称: __pppOsThread
** 功能描述: PPPoS 接收线程
** 输　入  : pcb           pcb 控制块
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static VOID  __pppOsThread (ppp_pcb *pcb)
{
    PPP_CTX_PRIV *pctxp;
    ssize_t       sstRead;
    UCHAR         ucBuffer[PPPRBUF_SIZE];

    pctxp = (PPP_CTX_PRIV *)pcb->ctx_cb;

    for (;;) {
        sstRead = read((INT)pcb->fd, ucBuffer, sizeof(ucBuffer));
        if (sstRead > 0) {
            pppos_input(pcb, ucBuffer, (INT)sstRead);
        }

        if ((pctxp->CTXP_bNeedDelete) &&
            (pcb->phase == PPP_PHASE_DEAD)) {                           /*  需要删除 PPP 连接           */
            close((INT)pcb->fd);
            pppapi_delete(pcb);
            __SHEAP_FREE(pctxp);
            break;
        }
    }
}
/*********************************************************************************************************
** 函数名称: __pppGet
** 功能描述: 通过网卡名获取 PPP 控制块
** 输　入  : pcb           pcb 控制块
**           ucPhase       状态码
**           pvCtx         PPP_CTX_PRIV 结构
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static ppp_pcb  *__pppGet (CPCHAR  pcIfName)
{
    ppp_pcb       *pcb;
    struct netif  *netif;

    if ((pcIfName[0] != 'p') ||
        (pcIfName[1] != 'p')) {
        _ErrorHandle(ENODEV);
        return  (LW_NULL);
    }

    netif = netif_find((char *)pcIfName);
    if (netif == LW_NULL) {
        _ErrorHandle(ENODEV);
        return  (LW_NULL);
    }

    pcb = _LIST_ENTRY(netif, ppp_pcb, netif);

    return  (pcb);
}
/*********************************************************************************************************
** 函数名称: API_PppOsCreate
** 功能描述: 创建一个 PPPoS 型连接网卡
** 输　入  : pcSerial      使用的串行接口设备名
**           ptty          串口工作参数
**           pcIfName      如果创建成功, 则返回网络设备名称
**           stMaxSize     ifname 缓冲区大小
** 输　出  : PPP 连接网卡是否安装成功
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_PppOsCreate (CPCHAR  pcSerial, LW_PPP_TTY  *ptty, PCHAR  pcIfName, size_t  stMaxSize)
{
    INT             iErrLevel = 0;
    INT             iFd;
    PPP_CTX_PRIV   *pctxp;
    ppp_pcb        *pcb;
    
    if (!pcSerial || !pcIfName || (stMaxSize < 4)) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    pctxp = (PPP_CTX_PRIV *)__SHEAP_ALLOC(sizeof(PPP_CTX_PRIV));
    if (pctxp == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (PX_ERROR);
    }
    lib_bzero(pctxp, sizeof(PPP_CTX_PRIV));
    
    pcb = pppapi_new();                                                 /*  创建 PPP 网卡               */
    if (pcb == LW_NULL) {
        _ErrorHandle(ENOMEM);
        iErrLevel = 1;
        goto    __error_handle;
    }
    
    __KERNEL_SPACE_ENTER();                                             /*  文件为内核 IO 文件          */
    iFd = open(pcSerial, O_RDWR);
    if (iFd < 0) {
        __KERNEL_SPACE_EXIT();
        iErrLevel = 2;
        goto    __error_handle;
    }
    
    if (!isatty(iFd)) {
        __KERNEL_SPACE_EXIT();
        _ErrorHandle(ENOTTY);
        iErrLevel = 2;
        goto    __error_handle;
    }
    
    ioctl(iFd, FIOSETOPTIONS, OPT_RAW);
    ioctl(iFd, FIORTIMEOUT,   LW_NULL);
    ioctl(iFd, FIOWTIMEOUT,   LW_NULL);
    ioctl(iFd, FIORBUFSET,    PPPRBUF_SIZE);
    ioctl(iFd, FIOWBUFSET,    PPPWBUF_SIZE);

    if (ptty) {                                                         /*  设置串口参数                */
        INT  iHwOpt = CREAD | CS8 | HUPCL;
        if (ptty->stop_bits == 2) {
            iHwOpt |= STOPB;
        }
        if (ptty->parity) {
            iHwOpt |= PARENB;
            if (ptty->parity == 1) {
                iHwOpt |= PARODD;
            }
        }
        ioctl(iFd, FIOBAUDRATE,     ptty->baud);
        ioctl(iFd, SIO_HW_OPTS_SET, iHwOpt);
    }
    __KERNEL_SPACE_EXIT();
    
    pctxp->CTXP_uiType      = PPP_OS;
    pctxp->CTXP_ulInput     = LW_OBJECT_HANDLE_INVALID;
    pctxp->CTXP_bNeedDelete = LW_FALSE;
    pctxp->CTXP_ucPhase     = PPP_PHASE_DEAD;
    pctxp->CTXP_pcb         = pcb;

    if (pppapi_over_serial_create(pcb, (sio_fd_t)iFd, __pppLinkStatCb, pctxp)) {
        _ErrorHandle(ENODEV);
        iErrLevel = 3;
        goto    __error_handle;
    }

    pppapi_set_notify_phase_callback(pcb, __pppNotifyPhaseCb);
    
    pcIfName[0] = pcb->netif.name[0];
    pcIfName[1] = pcb->netif.name[1];
    pcIfName[2] = pcb->netif.num + '0';
    pcIfName[3] = PX_EOS;
    
    return  (ERROR_NONE);
    
__error_handle:
    if (iErrLevel > 2) {
        close(iFd);
    }
    if (iErrLevel > 1) {
        pppapi_delete(pcb);
    }
    if (iErrLevel > 0) {
        __SHEAP_FREE(pctxp);
    }
    
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_PppOeCreate
** 功能描述: 创建一个 PPPoE 型连接网卡
** 输　入  : pcEthIf       使用的以太网网卡名
**           pcIfName      如果创建成功, 则返回网络设备名称
**           stMaxSize     ifname 缓冲区大小
** 输　出  : PPP 连接网卡是否安装成功
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_PppOeCreate (CPCHAR  pcEthIf, PCHAR  pcIfName, size_t  stMaxSize)
{
    INT             iErrLevel = 0;
    PPP_CTX_PRIV   *pctxp;
    ppp_pcb        *pcb;
    struct netif   *netif;

    if (!pcEthIf || !pcIfName || (stMaxSize < 4)) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    pctxp = (PPP_CTX_PRIV *)__SHEAP_ALLOC(sizeof(PPP_CTX_PRIV));
    if (pctxp == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (PX_ERROR);
    }
    lib_bzero(pctxp, sizeof(PPP_CTX_PRIV));

    pcb = pppapi_new();                                                 /*  创建 PPP 网卡               */
    if (pcb == LW_NULL) {
        _ErrorHandle(ENOMEM);
        iErrLevel = 1;
        goto    __error_handle;
    }

    netif = netif_find((char *)pcEthIf);
    if ((netif == LW_NULL) || ((netif->flags & (NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET)) == 0)) {
        _ErrorHandle(ENODEV);
        iErrLevel = 2;
        goto    __error_handle;
    }

    pctxp->CTXP_uiType      = PPP_OE;
    pctxp->CTXP_ulInput     = LW_OBJECT_HANDLE_INVALID;
    pctxp->CTXP_bNeedDelete = LW_FALSE;
    pctxp->CTXP_ucPhase     = PPP_PHASE_DEAD;
    pctxp->CTXP_pcb         = pcb;

    if (pppapi_over_ethernet_create(pcb, netif, LW_NULL, LW_NULL, __pppLinkStatCb, pctxp)) {
        _ErrorHandle(ENODEV);
        iErrLevel = 2;
        goto    __error_handle;
    }

    pppapi_set_notify_phase_callback(pcb, __pppNotifyPhaseCb);

    pcIfName[0] = pcb->netif.name[0];
    pcIfName[1] = pcb->netif.name[1];
    pcIfName[2] = pcb->netif.num + '0';
    pcIfName[3] = PX_EOS;

    return  (ERROR_NONE);

__error_handle:
    if (iErrLevel > 1) {
        pppapi_delete(pcb);
    }
    if (iErrLevel > 0) {
        __SHEAP_FREE(pctxp);
    }

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_PppOl2tpCreate
** 功能描述: 创建一个 PPPoL2TP 型连接网卡
** 输　入  : pcEthIf       使用的以太网网卡名
**           pcIp          服务器地址
**           usPort        服务器端口
**           pcSecret      未使用
**           stSecretLen   未使用
**           pcIfName      如果创建成功, 则返回网络设备名称
**           stMaxSize     ifname 缓冲区大小
** 输　出  : PPP 连接网卡是否安装成功
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_PppOl2tpCreate (CPCHAR  pcEthIf, 
                         CPCHAR  pcIp,
                         UINT16  usPort,
                         CPCHAR  pcSecret,
                         size_t  stSecretLen,
                         PCHAR   pcIfName, 
                         size_t  stMaxSize)
{
    INT             iErrLevel = 0;
    PPP_CTX_PRIV   *pctxp;
    ppp_pcb        *pcb;
    struct netif   *netif;
    ip_addr_t       ipaddr;

    if (!pcEthIf || !pcIp || !usPort || !pcIfName || (stMaxSize < 4)) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    ipaddr.addr = ipaddr_addr(pcIp);
    if (ipaddr.addr == IPADDR_NONE) {                                   /*  服务器地址无效              */
        _ErrorHandle(EADDRNOTAVAIL);
        return  (PX_ERROR);
    }

    pctxp = (PPP_CTX_PRIV *)__SHEAP_ALLOC(sizeof(PPP_CTX_PRIV));
    if (pctxp == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (PX_ERROR);
    }
    lib_bzero(pctxp, sizeof(PPP_CTX_PRIV));

    pcb = pppapi_new();                                                 /*  创建 PPP 网卡               */
    if (pcb == LW_NULL) {
        _ErrorHandle(ENOMEM);
        iErrLevel = 1;
        goto    __error_handle;
    }

    netif = netif_find((char *)pcEthIf);
    if (netif == LW_NULL) {
        _ErrorHandle(ENODEV);
        iErrLevel = 2;
        goto    __error_handle;
    }

    pctxp->CTXP_uiType      = PPP_OL2TP;
    pctxp->CTXP_ulInput     = LW_OBJECT_HANDLE_INVALID;
    pctxp->CTXP_bNeedDelete = LW_FALSE;
    pctxp->CTXP_ucPhase     = PPP_PHASE_DEAD;
    pctxp->CTXP_pcb         = pcb;

    if (pppapi_over_l2tp_create(pcb, netif, &ipaddr, ntohs(usPort),
                                (u8_t *)pcSecret, (u8_t)stSecretLen,
                                __pppLinkStatCb, pctxp)) {
        _ErrorHandle(ENODEV);
        iErrLevel = 2;
        goto    __error_handle;
    }

    pppapi_set_notify_phase_callback(pcb, __pppNotifyPhaseCb);

    pcIfName[0] = pcb->netif.name[0];
    pcIfName[1] = pcb->netif.name[1];
    pcIfName[2] = pcb->netif.num + '0';
    pcIfName[3] = PX_EOS;

    return  (ERROR_NONE);

__error_handle:
    if (iErrLevel > 1) {
        pppapi_delete(pcb);
    }
    if (iErrLevel > 0) {
        __SHEAP_FREE(pctxp);
    }

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_PppDelete
** 功能描述: 删除一个 PPP 型连接网卡
** 输　入  : pcIfName      网络设备名称
** 输　出  : PPP 网卡删除是否成功
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_PppDelete (CPCHAR  pcIfName)
{
    ppp_pcb      *pcb;
    PPP_CTX_PRIV *pctxp;

    if (!pcIfName) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    pcb = __pppGet(pcIfName);
    if (pcb == LW_NULL) {
        return  (PX_ERROR);
    }

    if (pcb->phase != PPP_PHASE_DEAD) {                                 /*  网卡必须彻底关闭才能删除    */
        _ErrorHandle(EBUSY);
        return  (PX_ERROR);
    }

    pctxp = (PPP_CTX_PRIV *)pcb->ctx_cb;
    if (pctxp->CTXP_ulInput) {                                          /*  PPPoS 型连接                */
        pctxp->CTXP_bNeedDelete = LW_TRUE;
        __KERNEL_SPACE_ENTER();
        ioctl((INT)pcb->fd, FIOCANCEL);
        __KERNEL_SPACE_EXIT();
        return  (ERROR_NONE);

    } else {
        pppapi_delete(pcb);                                             /*  直接删除                    */
        __SHEAP_FREE(pctxp);
    }

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_PppConnect
** 功能描述: 拨号连接
** 输　入  : pcIfName      网络设备名称
**           pdial         拨号参数
** 输　出  : 拨号结果
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_PppConnect (CPCHAR  pcIfName, LW_PPP_DIAL *pdial)
{
    PPP_CTX_PRIV        *pctxp;
    ppp_pcb             *pcb;
    LW_CLASS_THREADATTR  attr;

    if (!pcIfName) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    pcb = __pppGet(pcIfName);
    if (pcb == LW_NULL) {
        return  (PX_ERROR);
    }

    if ((pcb->phase != PPP_PHASE_DEAD) &&
        (pcb->phase != PPP_PHASE_HOLDOFF)) {                            /*  非这两种状态, 不能拨号      */
        _ErrorHandle(EBUSY);
        return  (PX_ERROR);
    }

    if (pdial) {
        pppapi_set_auth(pcb, PPPAUTHTYPE_ANY, pdial->user, pdial->passwd);
    }

    pctxp = (PPP_CTX_PRIV *)pcb->ctx_cb;
    if ((pctxp->CTXP_uiType  == PPP_OS) &&
        (pctxp->CTXP_ulInput == LW_OBJECT_HANDLE_INVALID)) {
        API_ThreadAttrBuild(&attr, LW_CFG_LWIP_STK_SIZE, LW_PRIO_T_NETPROTO,
                            LW_OPTION_THREAD_STK_CHK |
                            LW_OPTION_THREAD_SAFE |
                            LW_OPTION_OBJECT_GLOBAL, (PVOID)pcb);
        pctxp->CTXP_ulInput = API_ThreadCreate("t_pppos",
                            (PTHREAD_START_ROUTINE)__pppOsThread,
                            &attr, LW_NULL);                            /*  创建 PPPoS 接收线程         */
    }

    pppapi_open(pcb, 0);                                                /*  拨号连接                    */

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_PppDisconnect
** 功能描述: 挂断连接
** 输　入  : pcIfName      网络设备名称
**           bForce        是否强制挂断
** 输　出  : 挂断结果
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_PppDisconnect (CPCHAR  pcIfName, BOOL  bForce)
{
    ppp_pcb      *pcb;

    if (!pcIfName) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    pcb = __pppGet(pcIfName);
    if (pcb == LW_NULL) {
        return  (PX_ERROR);
    }

    if (pcb->phase == PPP_PHASE_DEAD) {
        return  (ERROR_NONE);
    }

    if (bForce) {
        pppapi_sighup(pcb);                                             /*  强制中断 PPP 连接           */

    } else {
        pppapi_close(pcb);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_PppGetPhase
** 功能描述: 获得当前连接状态
** 输　入  : pcIfName      网络设备名称
**           piPhase       连接状态
** 输　出  : OK or ERROR
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_PppGetPhase (CPCHAR  pcIfName, INT  *piPhase)
{
    ppp_pcb      *pcb;

    if (!pcIfName || !piPhase) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    pcb = __pppGet(pcIfName);
    if (pcb == LW_NULL) {
        return  (PX_ERROR);
    }

    *piPhase = pcb->phase;
    
    return  (ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_LWIP_PPP > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
