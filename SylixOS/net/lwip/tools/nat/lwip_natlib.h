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
** 文   件   名: lwip_natlib.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2010 年 03 月 19 日
**
** 描        述: lwip NAT 支持包内部库.
*********************************************************************************************************/

#ifndef __LWIP_NATLIB_H
#define __LWIP_NATLIB_H

/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if (LW_CFG_NET_EN > 0) && (LW_CFG_NET_NAT_EN > 0)
/*********************************************************************************************************
  NAT 控制块
*********************************************************************************************************/
typedef struct {
    LW_LIST_LINE        NAT_lineManage;                                 /*  NAT 控制块链表              */
    
    u8_t                NAT_ucProto;                                    /*  协议                        */
    ip_addr_t           NAT_ipaddrLocalIp;                              /*  本地 IP 地址                */
    u16_t               NAT_usLocalPort;                                /*  本地端口号                  */
    u16_t               NAT_usAssPort;                                  /*  映射端口号 (唯一的)         */
    
    ULONG               NAT_ulIdleTimer;                                /*  空闲定时器                  */
    ULONG               NAT_ulTermTimer;                                /*  断链定时器                  */
    INT                 NAT_iStatus;                                    /*  通信状态                    */
#define __NAT_STATUS_OPEN           0
#define __NAT_STATUS_FIN            1
#define __NAT_STATUS_CLOSING        2
} __NAT_CB;
typedef __NAT_CB       *__PNAT_CB;
/*********************************************************************************************************
  NAT 操作锁
*********************************************************************************************************/
#define __NAT_OP_LOCK()     API_SemaphoreMPend(_G_ulNatOpLock, LW_OPTION_WAIT_INFINITE)
#define __NAT_OP_UNLOCK()   API_SemaphoreMPost(_G_ulNatOpLock)
#endif                                                                  /*  LW_CFG_NET_EN > 0           */
                                                                        /*  LW_CFG_NET_NAT_EN > 0       */
#endif                                                                  /*  __LWIP_NATLIB_H             */
/*********************************************************************************************************
  END
*********************************************************************************************************/
