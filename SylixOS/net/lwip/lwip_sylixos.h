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
** 文   件   名: lwip_sylixos.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 05 月 06 日
**
** 描        述: sylixos inet (lwip)
*********************************************************************************************************/

#ifndef __LWIP_SYLIXOS_H
#define __LWIP_SYLIXOS_H

/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_NET_EN > 0

/*********************************************************************************************************
  网络初始化及网卡工作队列.
*********************************************************************************************************/

LW_API VOID         API_NetInit(VOID);                                  /*  安装网络组件                */

LW_API INT          API_NetJobAdd(VOIDFUNCPTR  pfunc, 
                                  PVOID        pvArg0,
                                  PVOID        pvArg1,
                                  PVOID        pvArg2,
                                  PVOID        pvArg3,
                                  PVOID        pvArg4,
                                  PVOID        pvArg5);                 /*  net job add                 */
                                  
LW_API size_t       API_NetJobGetLost(VOID);

#define netInit             API_NetInit
#define netJobAdd           API_NetJobAdd
#define netJobGetLost       API_NetJobGetLost

/*********************************************************************************************************
  SNMP private mib 操作接口.
  注意: API_NetSnmpPriMibGet() 返回值需强转成 struct mib_array_node * 类型
        API_NetSnmpSetPriMibInitHook() 需要在 API_NetInit() 调用前初始化.
*********************************************************************************************************/

#if LW_CFG_LWIP_SNMP > 0

LW_API PVOID        API_NetSnmpPriMibGet(VOID);
LW_API VOID         API_NetSnmpSetPriMibInitHook(VOIDFUNCPTR  pfuncPriMibInit);

#define netSnmpPriMibGet                API_NetSnmpPriMibGet
#define netSnmpSetPriMibInitHook        API_NetSnmpSetPriMibInitHook

#endif                                                                  /*  LW_CFG_LWIP_SNMP > 0        */

#endif                                                                  /*  LW_CFG_NET_EN               */
#endif                                                                  /*  __LWIP_SYLIXOS_H            */
/*********************************************************************************************************
  END
*********************************************************************************************************/
