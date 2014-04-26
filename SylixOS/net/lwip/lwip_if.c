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
** 文   件   名: lwip_if.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2011 年 03 月 10 日
**
** 描        述: posix net/if 接口.

** BUG:
2011.07.07  _G_ulNetifLock 改名.
2014.03.22  优化获得网络接口.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_NET_EN > 0
#include "lwip/netif.h"
#include "net/if.h"
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
extern LW_OBJECT_HANDLE     _G_ulNetifLock;
/*********************************************************************************************************
  网络接口锁
*********************************************************************************************************/
#define LWIP_NETIF_LOCK()   API_SemaphoreBPend(_G_ulNetifLock, LW_OPTION_WAIT_INFINITE)
#define LWIP_NETIF_UNLOCK() API_SemaphoreBPost(_G_ulNetifLock)
/*********************************************************************************************************
** 函数名称: if_nametoindex
** 功能描述: map a network interface name to its corresponding index
** 输　入  : ifname        if name
** 输　出  : index
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
unsigned  if_nametoindex (const char *ifname)
{
    struct netif    *pnetif;
    unsigned         uiIndex = 0;
    
    LWIP_NETIF_LOCK();                                                  /*  进入临界区                  */
    pnetif = netif_find((char *)ifname);
    if (pnetif) {
        uiIndex = (unsigned)pnetif->num;
    }
    LWIP_NETIF_UNLOCK();                                                /*  退出临界区                  */
    
    return  (uiIndex);
}
/*********************************************************************************************************
** 函数名称: if_indextoname
** 功能描述: map a network interface index to its corresponding name
** 输　入  : ifindex       if index
**           ifname        if name buffer at least {IF_NAMESIZE} bytes
** 输　出  : index
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
char *if_indextoname (unsigned  ifindex, char *ifname)
{
    struct netif    *pnetif;

    if (!ifname) {
        errno = EINVAL;
    }
    
    LWIP_NETIF_LOCK();                                                  /*  进入临界区                  */
    pnetif = (struct netif *)netif_get_by_index(ifindex);
    if (pnetif) {
        ifname[0] = pnetif->name[0];
        ifname[1] = pnetif->name[1];
        ifname[2] = (char)(pnetif->num + '0');
        ifname[3] = PX_EOS;
    }
    LWIP_NETIF_UNLOCK();                                                /*  退出临界区                  */
    
    if (pnetif) {
        return  (ifname);
    } else {
        errno = ENXIO;
        return  (LW_NULL);
    }
}
/*********************************************************************************************************
** 函数名称: if_nameindex
** 功能描述: return all network interface names and indexes
** 输　入  : NONE
** 输　出  : An array of structures identifying local interfaces
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
struct if_nameindex *if_nameindex (void)
{
    struct netif           *pnetif;
    int                     iNum = 1;                                   /*  需要一个空闲的位置          */
    struct if_nameindex    *pifnameindexArry;
    
    LWIP_NETIF_LOCK();                                                  /*  进入临界区                  */
    for(pnetif = netif_list; pnetif != LW_NULL; pnetif = pnetif->next) {
        iNum++;
    }
    pifnameindexArry = (struct if_nameindex *)__SHEAP_ALLOC(sizeof(struct if_nameindex) * (size_t)iNum);
    if (pifnameindexArry) {
        int     i = 0;
        
        for(pnetif  = netif_list; 
            pnetif != LW_NULL; 
            pnetif  = pnetif->next) {
            
            pifnameindexArry[i].if_index = (unsigned)pnetif->num;
            pifnameindexArry[i].if_name_buf[0] = pnetif->name[0];
            pifnameindexArry[i].if_name_buf[1] = pnetif->name[1];
            pifnameindexArry[i].if_name_buf[2] = (char)(pnetif->num + '0');
            pifnameindexArry[i].if_name_buf[3] = PX_EOS;
            pifnameindexArry[i].if_name = pifnameindexArry[i].if_name_buf;
            i++;
        }
        
        pifnameindexArry[i].if_index = 0;
        pifnameindexArry[i].if_name_buf[0] = PX_EOS;
        pifnameindexArry[i].if_name = pifnameindexArry[i].if_name_buf;
        
    } else {
        errno = ENOMEM;
    }
    LWIP_NETIF_UNLOCK();                                                /*  退出临界区                  */
    
    return  (pifnameindexArry);
}
/*********************************************************************************************************
** 函数名称: if_freenameindex
** 功能描述: free memory allocated by if_nameindex
             the application shall not use the array of which ptr is the address.
** 输　入  : ptr           shall be a pointer that was returned by if_nameindex().
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
void  if_freenameindex (struct if_nameindex *ptr)
{
    if (ptr) {
        __SHEAP_FREE(ptr);
    }
}
#endif                                                                  /*  LW_CFG_NET_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
