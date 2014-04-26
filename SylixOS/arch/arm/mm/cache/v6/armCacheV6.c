/**********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                       SylixOS(TM)
**
**                               Copyright  All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: armCacheV6.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 12 月 09 日
**
** 描        述: ARMv6 体系构架 CACHE 驱动.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_CACHE_EN > 0
#include "../armCacheCommon.h"
#include "../../mmu/armMmuCommon.h"
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
extern VOID  armDCacheV6Disable(VOID);
extern VOID  armDCacheV6Flush(PVOID  pvAdrs, size_t  stBytes);
extern VOID  armDCacheV6Clear(PVOID  pvAdrs, size_t  stBytes);
/*********************************************************************************************************
** 函数名称: armCacheV6Enable
** 功能描述: 使能 CACHE 
** 输　入  : cachetype      INSTRUCTION_CACHE / DATA_CACHE
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  armCacheV6Enable (LW_CACHE_TYPE  cachetype)
{
    if (cachetype == INSTRUCTION_CACHE) {
        armICacheEnable();
    
    } else {
        armDCacheEnable();
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV6Disable
** 功能描述: 禁能 CACHE 
** 输　入  : cachetype      INSTRUCTION_CACHE / DATA_CACHE
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  armCacheV6Disable (LW_CACHE_TYPE  cachetype)
{
    if (cachetype == INSTRUCTION_CACHE) {
        armICacheDisable();
    
    } else {
        armDCacheV6Disable();
    }
     
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV6Flush
** 功能描述: CACHE 脏数据回写
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV6Flush (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    if (cachetype == DATA_CACHE) {
        armDCacheV6Flush(pvAdrs, stBytes);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV6Clear
** 功能描述: 指定类型的 CACHE 使部分或全部清空(回写内存)并无效(访问不命中)
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV6Clear (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    if (cachetype == INSTRUCTION_CACHE) {
        armICacheInvalidate(pvAdrs, stBytes);
    
    } else {
        armDCacheV6Clear(pvAdrs, stBytes);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV6Lock
** 功能描述: 锁定指定类型的 CACHE 
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV6Lock (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    _ErrorHandle(ENOSYS);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: armCacheV6Unlock
** 功能描述: 解锁指定类型的 CACHE 
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV6Unlock (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    _ErrorHandle(ENOSYS);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: armCacheV6TextUpdate
** 功能描述: 清空(回写内存) D CACHE 无效(访问不命中) I CACHE
** 输　入  : pvAdrs                        虚拟地址
**           stBytes                       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
** 注  意  : L2 cache 为统一 CACHE 所以 text update 不需要操作 L2 cache.
*********************************************************************************************************/
static INT	armCacheV6TextUpdate (PVOID  pvAdrs, size_t  stBytes)
{
    armDCacheV6Flush(pvAdrs, stBytes);
    armICacheInvalidate(pvAdrs, stBytes);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: archCacheV6Init
** 功能描述: 初始化 CACHE 
** 输　入  : pcacheop       CACHE 操作函数集
**           uiInstruction  指令 CACHE 参数
**           uiData         数据 CACHE 参数
**           pcMachineName  机器名称
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  armCacheV6Init (LW_CACHE_OP *pcacheop, 
                      CACHE_MODE   uiInstruction, 
                      CACHE_MODE   uiData, 
                      CPCHAR       pcMachineName)
{
    pcacheop->CACHEOP_iILoc      = CACHE_LOCATION_VIPT;
    pcacheop->CACHEOP_iDLoc      = CACHE_LOCATION_VIPT;
    pcacheop->CACHEOP_iCacheLine = 32;
    
    pcacheop->CACHEOP_pfuncEnable  = armCacheV6Enable;
    pcacheop->CACHEOP_pfuncDisable = armCacheV6Disable;
    
    pcacheop->CACHEOP_pfuncLock    = armCacheV6Lock;                    /*  暂时不支持锁定操作          */
    pcacheop->CACHEOP_pfuncUnlock  = armCacheV6Unlock;
    
    pcacheop->CACHEOP_pfuncFlush      = armCacheV6Flush;
    pcacheop->CACHEOP_pfuncInvalidate = armCacheV6Clear;
    pcacheop->CACHEOP_pfuncClear      = armCacheV6Clear;
    pcacheop->CACHEOP_pfuncTextUpdate = armCacheV6TextUpdate;
    
#if LW_CFG_VMM_EN > 0
    pcacheop->CACHEOP_pfuncDmaMalloc      = API_VmmDmaAlloc;
    pcacheop->CACHEOP_pfuncDmaMallocAlign = API_VmmDmaAllocAlign;
    pcacheop->CACHEOP_pfuncDmaFree        = API_VmmDmaFree;
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
}
/*********************************************************************************************************
** 函数名称: archCacheV6Reset
** 功能描述: 复位 CACHE 
** 输　入  : pcMachineName  机器名称
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  armCacheV6Reset (CPCHAR  pcMachineName)
{
    armICacheInvalidate(LW_NULL, (size_t)~0);
    armDCacheV6Disable();
    armICacheDisable();
}

#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
