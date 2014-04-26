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
** 文   件   名: lwip_pppfd.c
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
#include "lwip_pppfd.h"
/*********************************************************************************************************
  PPP 基本参数
*********************************************************************************************************/
#define LW_PPPFD_BMSG_BUFSIZE   512
/*********************************************************************************************************
  PPP 设备结构
*********************************************************************************************************/
typedef struct {
    LW_DEV_HDR          PD_devhdrHdr;                                   /*  设备头                      */
    LW_LIST_LINE        PD_lineManage;                                  /*  所有 PPP 连接链表           */
    ppp_pcb            *PD_pcb;                                         /*  PPP 控制块                  */
    PLW_BMSG            PD_pbmsg;                                       /*  PPP 错误号与阶段消息        */
    INT                 PD_iFlag;
    INT                 PD_iSerial;                                     /*  PPPoS 串口文件描述符        */
    LW_OBJECT_HANDLE    PD_ulInput;                                     /*  PPPoS 输入线程              */
    LW_OBJECT_HANDLE    PD_ulSignal;                                    /*  PPP 事件同步                */
} LW_PPPFD_DEV;
typedef LW_PPPFD_DEV   *PLW_PPPFD_DEV;
/*********************************************************************************************************
  驱动程序全局变量
*********************************************************************************************************/
static INT                  _G_iPppfdDrvNum = PX_ERROR;
static LW_LIST_LINE_HEADER  _G_plinePppfd;
static LW_OBJECT_HANDLE     _G_hPppfdSelMutex;
/*********************************************************************************************************
  驱动程序
*********************************************************************************************************/
static LONG     _pppfdOpen(PLW_PPPFD_DEV   p_pppfddev, PCHAR  pcName, INT  iFlags, INT  iMode);
static INT      _pppfdClose(PLW_PPPFD_DEV  p_pppfddev);
static ssize_t  _pppfdRead(PLW_PPPFD_DEV   p_pppfddev, PCHAR  pcBuffer, size_t  stMaxBytes);
static INT      _pppfdIoctl(PLW_PPPFD_DEV  p_pppfddev, INT    iRequest, LONG  lArg);
/*********************************************************************************************************
  锁操作
*********************************************************************************************************/
#define LW_PPPFD_LOCK()             API_SemaphoreMPend(_G_hPppfdSelMutex, LW_OPTION_WAIT_INFINITE)
#define LW_PPPFD_UNLOCK()           API_SemaphoreMPost(_G_hPppfdSelMutex)
/*********************************************************************************************************
** 函数名称: API_PppfdDrvInstall
** 功能描述: 安装 pppfd 设备驱动程序
** 输　入  : NONE
** 输　出  : 驱动是否安装成功
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
INT  API_PppfdDrvInstall (VOID)
{
    if (_G_iPppfdDrvNum == PX_ERROR) {
        _G_iPppfdDrvNum =  iosDrvInstall(LW_NULL,
                                         LW_NULL,
                                         _pppfdOpen,
                                         _pppfdClose,
                                         _pppfdRead,
                                         LW_NULL,
                                         _pppfdIoctl);
        DRIVER_LICENSE(_G_iPppfdDrvNum,     "Dual BSD/GPL->Ver 1.0");
        DRIVER_AUTHOR(_G_iPppfdDrvNum,      "Han.hui");
        DRIVER_DESCRIPTION(_G_iPppfdDrvNum, "pppfd driver.");
    }

    if (_G_hPppfdSelMutex == LW_OBJECT_HANDLE_INVALID) {
        _G_hPppfdSelMutex =  API_SemaphoreMCreate("pppfdsel_lock", LW_PRIO_DEF_CEILING,
                                                  LW_OPTION_WAIT_PRIORITY | LW_OPTION_DELETE_SAFE |
                                                  LW_OPTION_INHERIT_PRIORITY | LW_OPTION_OBJECT_GLOBAL,
                                                  LW_NULL);
    }

    return  ((_G_iPppfdDrvNum == (PX_ERROR)) ? (PX_ERROR) : (ERROR_NONE));
}
/*********************************************************************************************************
** 函数名称: __pppfd_status_cb
** 功能描述: PPP 连接状态变化回调
** 输　入  : pcb           连接控制块
**           errcode       错误编码
**           p_pppfddev    PPP 连接设备
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static void  __pppfd_status_cb (ppp_pcb *pcb, u8_t errcode, PLW_PPPFD_DEV  p_pppfddev)
{

}
/*********************************************************************************************************
** 函数名称: __pppfd_phase_cb
** 功能描述: PPP 连接过程变化回调
** 输　入  : pcb           连接控制块
**           phase         过程编码
**           p_pppfddev    PPP 连接设备
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static void  __pppfd_phase_cb (ppp_pcb *pcb, u8_t phase, PLW_PPPFD_DEV  p_pppfddev)
{

}
/*********************************************************************************************************
** 函数名称: pppfd_os_create
** 功能描述: 创建一个 PPPoS 型连接网卡
** 输　入  : serial        使用的串行接口设备名
**           pppif         如果创建成功, 则返回网络设备名称
**           bufsize       ifname 缓冲区大小
** 输　出  : PPP 连接网卡是否安装成功
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
int  pppfd_os_create (const char *serial, char *pppif, size_t bufsize)
{
    char            cDevName[32];
    PLW_PPPFD_DEV   p_pppfddev;
    INT             iFd;
    INT             iErrLevel = 0;

    if (!serial || !pppif || (bufsize < 4)) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    iFd = open(serial, O_RDWR);
    if (iFd < 0) {
        return  (PX_ERROR);
    }

    p_pppfddev = (PLW_PPPFD_DEV)__SHEAP_ALLOC(sizeof(LW_PPPFD_DEV));
    if (p_pppfddev == LW_NULL) {
        _ErrorHandle(ENOMEM);
        iErrLevel = 1;
        goto    __error_handle;
    }
    lib_bzero(p_pppfddev, sizeof(LW_PPPFD_DEV));

    p_pppfddev->PD_iSerial  = iFd;
    p_pppfddev->PD_ulSignal = API_SemaphoreBCreate("ppp_signal", LW_FALSE,
                                                   LW_OPTION_OBJECT_GLOBAL, LW_NULL);
    if (p_pppfddev->PD_ulSignal == LW_OBJECT_HANDLE_INVALID) {
        iErrLevel = 2;
        goto    __error_handle;
    }

    p_pppfddev->PD_pcb = pppapi_new();
    if (p_pppfddev->PD_pcb == LW_NULL) {
        _ErrorHandle(EMFILE);
        iErrLevel = 3;
        goto    __error_handle;
    }

    if (pppapi_over_serial_create(p_pppfddev->PD_pcb, (sio_fd_t)iFd,
                                  __pppfd_status_cb, (void *)p_pppfddev)) {
        _ErrorHandle(ENODEV);
        iErrLevel = 4;
        goto    __error_handle;
    }

    pppapi_set_notify_phase_callback(p_pppfddev->PD_pcb, __pppfd_phase_cb);

    snprintf(pppif, bufsize, "%c%c%d", p_pppfddev->PD_pcb->netif.name[0],
                                       p_pppfddev->PD_pcb->netif.name[1],
                                       p_pppfddev->PD_pcb->netif.num);
    snprintf(cDevName, sizeof(cDevName), "/dev/ppp/%s", pppif);

    if (iosDevAddEx(&p_pppfddev->PD_devhdrHdr, cDevName, _G_iPppfdDrvNum, DT_CHR) < ERROR_NONE) {
        iErrLevel = 5;
        goto    __error_handle;
    }

    ioctl(iFd, FIOSETOPTIONS, OPT_RAW);

    LW_PPPFD_LOCK();
    _List_Line_Add_Tail(&p_pppfddev->PD_lineManage, &_G_plinePppfd);
    LW_PPPFD_UNLOCK();

    return  (ERROR_NONE);

__error_handle:
    if (iErrLevel > 3) {
        pppapi_delete(p_pppfddev->PD_pcb);
    }
    if (iErrLevel > 2) {
        API_SemaphoreBDelete(&p_pppfddev->PD_ulSignal);
    }
    if (iErrLevel > 1) {
        __SHEAP_FREE(p_pppfddev);
    }
    if (iErrLevel > 0) {
        close(iFd);
    }

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: pppfd_oe_create
** 功能描述: 创建一个 PPPoE 型连接网卡
** 输　入  : ethif              使用的以太网络接口名 例如: en1
**           service_name       暂时未使用
**           concentrator_name  暂时未使用
**           pppif              如果创建成功, 则返回网络设备名称
**           bufsize            ifname 缓冲区大小
** 输　出  : PPP 连接网卡是否安装成功
** 全局变量:
** 调用模块:
** 注  意  : 由于 LWIP PPP 设计问题, 这里从查找以太网络接口到创建 PPPoE 连接间, 不能有网卡的添加删除操作.

                                           API 函数
*********************************************************************************************************/
LW_API
int  pppfd_oe_create (const char *ethif, const char *service_name, const char *concentrator_name,
                      char       *pppif, size_t      bufsize)
{
    char            cDevName[32];
    PLW_PPPFD_DEV   p_pppfddev;
    struct netif   *netif;
    INT             iErrLevel = 0;

    if (!ethif || !pppif || (bufsize < 4)) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    netif = netif_find((char *)ethif);
    if (netif == LW_NULL) {
        _ErrorHandle(ENODEV);
        return  (PX_ERROR);
    }

    p_pppfddev = (PLW_PPPFD_DEV)__SHEAP_ALLOC(sizeof(LW_PPPFD_DEV));
    if (p_pppfddev == LW_NULL) {
        _ErrorHandle(ENOMEM);
        iErrLevel = 1;
        goto    __error_handle;
    }
    lib_bzero(p_pppfddev, sizeof(LW_PPPFD_DEV));

    p_pppfddev->PD_iSerial  = PX_ERROR;
    p_pppfddev->PD_ulSignal = API_SemaphoreBCreate("ppp_signal", LW_FALSE,
                                                   LW_OPTION_OBJECT_GLOBAL, LW_NULL);
    if (p_pppfddev->PD_ulSignal == LW_OBJECT_HANDLE_INVALID) {
        iErrLevel = 2;
        goto    __error_handle;
    }

    p_pppfddev->PD_pcb = pppapi_new();
    if (p_pppfddev->PD_pcb == LW_NULL) {
        _ErrorHandle(EMFILE);
        iErrLevel = 3;
        goto    __error_handle;
    }

    if (pppapi_over_ethernet_create(p_pppfddev->PD_pcb, ethif, service_name,
                                    concentrator_name, __pppfd_status_cb, (void *)p_pppfddev)) {
        _ErrorHandle(ENODEV);
        iErrLevel = 4;
        goto    __error_handle;
    }

    pppapi_set_notify_phase_callback(p_pppfddev->PD_pcb, __pppfd_phase_cb);

    snprintf(pppif, bufsize, "%c%c%d", p_pppfddev->PD_pcb->netif.name[0],
                                           p_pppfddev->PD_pcb->netif.name[1],
                                           p_pppfddev->PD_pcb->netif.num);
    snprintf(cDevName, sizeof(cDevName), "/dev/ppp/%s", pppif);

    if (iosDevAddEx(&p_pppfddev->PD_devhdrHdr, cDevName, _G_iPppfdDrvNum, DT_CHR) < ERROR_NONE) {
        iErrLevel = 5;
        goto    __error_handle;
    }

    LW_PPPFD_LOCK();
    _List_Line_Add_Tail(&p_pppfddev->PD_lineManage, &_G_plinePppfd);
    LW_PPPFD_UNLOCK();

    return  (ERROR_NONE);

__error_handle:
    if (iErrLevel > 3) {
        pppapi_delete(p_pppfddev->PD_pcb);
    }
    if (iErrLevel > 2) {
        API_SemaphoreBDelete(&p_pppfddev->PD_ulSignal);
    }
    if (iErrLevel > 1) {
        __SHEAP_FREE(p_pppfddev);
    }

    return  (PX_ERROR);

}
/*********************************************************************************************************
** 函数名称: pppfd_ol2tp_create
** 功能描述: 创建一个 PPPoL2TP 型连接网卡
** 输　入  : serial        使用的串行接口设备名
**           ipaddr        服务器 IP 地址 (*.*.*.* 格式)
**           port          服务器端口号 (主机字节序)
**           secret        L2TP 密码
**           secret_len    L2TP 密码长度
**           pppif         如果创建成功, 则返回网络设备名称
**           bufsize       ifname 缓冲区大小
** 输　出  : PPP 连接网卡是否安装成功
** 全局变量:
** 调用模块:
** 注  意  : 由于 LWIP PPP 设计问题, 这里从查找以太网络接口到创建 PPPoE 连接间, 不能有网卡的添加删除操作.

                                           API 函数
*********************************************************************************************************/
LW_API
int  pppfd_ol2tp_create (const char *ethif,
                         const char *ipaddr, short   port,
                         const char *secret, size_t  secret_len,
                         char       *pppif,  size_t  bufsize)
{
    char            cDevName[32];
    PLW_PPPFD_DEV   p_pppfddev;
    struct netif   *netif;
    ip_addr_t       ipaddr_l2tp;
    INT             iErrLevel = 0;

    if (!ethif || !pppif || (bufsize < 4)) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    if (ipaddr_aton(ipaddr, &ipaddr_l2tp) == 0) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    netif = netif_find((char *)ethif);
    if (netif == LW_NULL) {
        _ErrorHandle(ENODEV);
        return  (PX_ERROR);
    }

    p_pppfddev = (PLW_PPPFD_DEV)__SHEAP_ALLOC(sizeof(LW_PPPFD_DEV));
    if (p_pppfddev == LW_NULL) {
        _ErrorHandle(ENOMEM);
        iErrLevel = 1;
        goto    __error_handle;
    }
    lib_bzero(p_pppfddev, sizeof(LW_PPPFD_DEV));

    p_pppfddev->PD_iSerial  = PX_ERROR;
    p_pppfddev->PD_ulSignal = API_SemaphoreBCreate("ppp_signal", LW_FALSE,
                                                   LW_OPTION_OBJECT_GLOBAL, LW_NULL);
    if (p_pppfddev->PD_ulSignal == LW_OBJECT_HANDLE_INVALID) {
        iErrLevel = 2;
        goto    __error_handle;
    }

    p_pppfddev->PD_pcb = pppapi_new();
    if (p_pppfddev->PD_pcb == LW_NULL) {
        _ErrorHandle(EMFILE);
        iErrLevel = 3;
        goto    __error_handle;
    }

    if (pppapi_over_l2tp_create(p_pppfddev->PD_pcb, ethif, &ipaddr_l2tp, port,
                                secret, secret_len,
                                __pppfd_status_cb, (void *)p_pppfddev)) {
        _ErrorHandle(ENODEV);
        iErrLevel = 4;
        goto    __error_handle;
    }

    pppapi_set_notify_phase_callback(p_pppfddev->PD_pcb, __pppfd_phase_cb);

    snprintf(pppif, bufsize, "%c%c%d", p_pppfddev->PD_pcb->netif.name[0],
                                       p_pppfddev->PD_pcb->netif.name[1],
                                       p_pppfddev->PD_pcb->netif.num);
    snprintf(cDevName, sizeof(cDevName), "/dev/ppp/%s", pppif);

    if (iosDevAddEx(&p_pppfddev->PD_devhdrHdr, cDevName, _G_iPppfdDrvNum, DT_CHR) < ERROR_NONE) {
        iErrLevel = 5;
        goto    __error_handle;
    }

    LW_PPPFD_LOCK();
    _List_Line_Add_Tail(&p_pppfddev->PD_lineManage, &_G_plinePppfd);
    LW_PPPFD_UNLOCK();

    return  (ERROR_NONE);

__error_handle:
    if (iErrLevel > 3) {
        pppapi_delete(p_pppfddev->PD_pcb);
    }
    if (iErrLevel > 2) {
        API_SemaphoreBDelete(&p_pppfddev->PD_ulSignal);
    }
    if (iErrLevel > 1) {
        __SHEAP_FREE(p_pppfddev);
    }

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: _pppfdOpen
** 功能描述: 打开 pppfd 设备
** 输　入  : p_pppfddev    pppfd 设备
**           pcName        设备名称
**           iFlags        O_...
**           iMode         0666 0444 ...
** 输　出  : pppfd 设备
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static LONG  _pppfdOpen (PLW_PPPFD_DEV   p_pppfddev, PCHAR  pcName, INT  iFlags, INT  iMode)
{
    if (LW_DEV_INC_USE_COUNT(&p_pppfddev->PD_devhdrHdr) == 1) {
        p_pppfddev->PD_pbmsg = _bmsgCreate(LW_PPPFD_BMSG_BUFSIZE);
        if (p_pppfddev->PD_pbmsg == LW_NULL) {
            LW_DEV_DEC_USE_COUNT(&p_pppfddev->PD_devhdrHdr);
            return  (PX_ERROR);
        }
        p_pppfddev->PD_iFlag = iFlags;
        return  ((LONG)p_pppfddev);

    } else {
        LW_DEV_DEC_USE_COUNT(&p_pppfddev->PD_devhdrHdr);
        _ErrorHandle(EBUSY);                                            /*  只允许打开一次              */
        return  (PX_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: _pppfdClose
** 功能描述: 关闭一个已经打开的 pppfd 设备
** 输　入  : p_pppfddev    pppfd 设备
** 输　出  : ERROR_NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT  _pppfdClose (PLW_PPPFD_DEV  p_pppfddev)
{
    if (LW_DEV_GET_USE_COUNT(&p_pppfddev->PD_devhdrHdr)) {
        _bmsgDelete(p_pppfddev->PD_pbmsg);
        p_pppfddev->PD_pbmsg = LW_NULL;
        LW_DEV_DEC_USE_COUNT(&p_pppfddev->PD_devhdrHdr);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: _pppfdRead
** 功能描述: 读取 pppfd 设备消息
** 输　入  : p_pppfddev    pppfd 设备
**           pcBuffer      接收缓冲区
**           stMaxBytes    接收缓冲区大小
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static ssize_t  _pppfdRead (PLW_PPPFD_DEV   p_pppfddev, PCHAR  pcBuffer, size_t  stMaxBytes)
{
    ULONG      ulErrCode;
    ULONG      ulTimeout;
    size_t     stMsgLen;
    ssize_t    sstRet;

    if (!pcBuffer || !stMaxBytes) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    if (p_pppfddev->PD_iFlag & O_NONBLOCK) {                            /*  非阻塞 IO                   */
        ulTimeout = LW_OPTION_NOT_WAIT;
    } else {
        ulTimeout = LW_OPTION_WAIT_INFINITE;
    }

    for (;;) {
        LW_PPPFD_LOCK();
        stMsgLen = (size_t)_bmsgNBytesNext(p_pppfddev->PD_pbmsg);
        if (stMsgLen > stMaxBytes) {
            LW_PPPFD_UNLOCK();
            _ErrorHandle(EMSGSIZE);                                     /*  缓冲区太小                  */
            return  (PX_ERROR);

        } else if (stMsgLen) {
            break;                                                      /*  数据可读                    */
        }
        LW_PPPFD_UNLOCK();

        ulErrCode = API_SemaphoreBPend(p_pppfddev->PD_ulSignal,         /*  等待数据有效                */
                                       ulTimeout);
        if (ulErrCode != ERROR_NONE) {                                  /*  超时                        */
            _ErrorHandle(EAGAIN);
            return  (0);
        }
    }

    sstRet = (ssize_t)_bmsgGet(p_pppfddev->PD_pbmsg, pcBuffer, stMaxBytes);

    LW_PPPFD_UNLOCK();

    return  (sstRet);
}
/*********************************************************************************************************
** 函数名称: _pppfdInput
** 功能描述: pppfd 设备拨号
** 输　入  : p_pppfddev    pppfd 设备
**           iRequest      控制命令
**           lArg          命令参数
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static PVOID _pppfdInput (PLW_PPPFD_DEV  p_pppfddev)
{
    UINT8   ucBuffer[256];
    ssize_t sstNum;

    for (;;) {
        sstNum = read(p_pppfddev->PD_iSerial, ucBuffer, sizeof(ucBuffer));
        if (sstNum > 0) {
            pppos_input(p_pppfddev->PD_pcb, ucBuffer, (int)sstNum);

        } else {
            break;
        }
    }
}
/*********************************************************************************************************
** 函数名称: _pppfdDail
** 功能描述: pppfd 设备拨号
** 输　入  : p_pppfddev    pppfd 设备
**           iRequest      控制命令
**           lArg          命令参数
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT  _pppfdDail (PLW_PPPFD_DEV  p_pppfddev)
{

}
/*********************************************************************************************************
** 函数名称: _pppfdIoctl
** 功能描述: 控制 pppfd 设备
** 输　入  : p_pppfddev    pppfd 设备
**           iRequest      控制命令
**           lArg          命令参数
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT  _pppfdIoctl (PLW_PPPFD_DEV  p_pppfddev, INT  iRequest, LONG  lArg)
{

}
#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_LWIP_PPP > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
