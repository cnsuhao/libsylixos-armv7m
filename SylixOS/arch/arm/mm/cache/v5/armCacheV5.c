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
** 文   件   名: armCacheV5.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 12 月 09 日
**
** 描        述: ARMv5 体系构架 CACHE 驱动.
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
extern VOID  armDCacheV5Disable(VOID);
extern VOID  armDCacheV5FlushAll(VOID);
extern VOID  armDCacheV5ClearAll(VOID);
/*********************************************************************************************************
  CACHE 参数
*********************************************************************************************************/
#define ARMv5_CACHE_LINE_SIZE           32
#define ARMv5_CACHE_LOOP_OP_MAX_SIZE    (16 * LW_CFG_KB_SIZE)
/*********************************************************************************************************
** 函数名称: armCacheV5Enable
** 功能描述: 使能 CACHE 
** 输　入  : cachetype      INSTRUCTION_CACHE / DATA_CACHE
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  armCacheV5Enable (LW_CACHE_TYPE  cachetype)
{
    if (cachetype == INSTRUCTION_CACHE) {
        armICacheEnable();
    
    } else {
        armDCacheEnable();
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV5Disable
** 功能描述: 禁能 CACHE 
** 输　入  : cachetype      INSTRUCTION_CACHE / DATA_CACHE
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  armCacheV5Disable (LW_CACHE_TYPE  cachetype)
{
    if (cachetype == INSTRUCTION_CACHE) {
        armICacheDisable();
    
    } else {
        armDCacheV5Disable();
    }
     
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV5Flush
** 功能描述: CACHE 脏数据回写
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV5Flush (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    addr_t  ulEnd;
    
    if (cachetype == DATA_CACHE) {
        if (stBytes >= ARMv5_CACHE_LOOP_OP_MAX_SIZE) {
            armDCacheV5FlushAll();
            
        } else {
            if (stBytes >= sizeof(PVOID)) {
                ulEnd = (addr_t)pvAdrs + stBytes - sizeof(PVOID);
            } else {
                ulEnd = (addr_t)pvAdrs;
            }
            armDCacheFlush(pvAdrs, (PVOID)ulEnd, ARMv5_CACHE_LINE_SIZE);/*  部分回写                    */
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV5Invalidate
** 功能描述: 指定类型的 CACHE 使部分无效(访问不命中)
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV5Invalidate (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    addr_t  ulEnd;

    if (cachetype == INSTRUCTION_CACHE) {
        if (stBytes >= ARMv5_CACHE_LOOP_OP_MAX_SIZE) {
            armICacheInvalidateAll();                                   /*  ICACHE 全部无效             */
        
        } else {
            if (stBytes >= sizeof(PVOID)) {
                ulEnd = (addr_t)pvAdrs + stBytes - sizeof(PVOID);
            } else {
                ulEnd = (addr_t)pvAdrs;
            }
            armICacheInvalidate(pvAdrs, (PVOID)ulEnd, ARMv5_CACHE_LINE_SIZE);
        }
    } else {
        if (stBytes >= sizeof(PVOID)) {
            ulEnd = (addr_t)pvAdrs + stBytes - sizeof(PVOID);
            armDCacheInvalidate(pvAdrs, (PVOID)ulEnd, ARMv5_CACHE_LINE_SIZE);
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV5Clear
** 功能描述: 指定类型的 CACHE 使部分或全部清空(回写内存)并无效(访问不命中)
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV5Clear (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    addr_t  ulEnd;

    if (cachetype == INSTRUCTION_CACHE) {
        if (stBytes >= ARMv5_CACHE_LOOP_OP_MAX_SIZE) {
            armICacheInvalidateAll();                                   /*  ICACHE 全部无效             */
            
        } else {
            if (stBytes >= sizeof(PVOID)) {
                ulEnd = (addr_t)pvAdrs + stBytes - sizeof(PVOID);
            } else {
                ulEnd = (addr_t)pvAdrs;
            }
            armICacheInvalidate(pvAdrs, (PVOID)ulEnd, ARMv5_CACHE_LINE_SIZE);
        }
    } else {
        if (stBytes >= ARMv5_CACHE_LOOP_OP_MAX_SIZE) {
            armDCacheV5ClearAll();                                      /*  全部回写并无效              */
        
        } else {
            if (stBytes >= sizeof(PVOID)) {
                ulEnd = (addr_t)pvAdrs + stBytes - sizeof(PVOID);
            } else {
                ulEnd = (addr_t)pvAdrs;
            }
            armDCacheClear(pvAdrs, (PVOID)ulEnd, ARMv5_CACHE_LINE_SIZE);/*  部分回写并无效              */
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV5Lock
** 功能描述: 锁定指定类型的 CACHE 
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV5Lock (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    _ErrorHandle(ENOSYS);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: armCacheV5Unlock
** 功能描述: 解锁指定类型的 CACHE 
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV5Unlock (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    _ErrorHandle(ENOSYS);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: armCacheV5TextUpdate
** 功能描述: 清空(回写内存) D CACHE 无效(访问不命中) I CACHE
** 输　入  : pvAdrs                        虚拟地址
**           stBytes                       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
** 注  意  : L2 cache 为统一 CACHE 所以 text update 不需要操作 L2 cache.
*********************************************************************************************************/
static INT	armCacheV5TextUpdate (PVOID  pvAdrs, size_t  stBytes)
{
    addr_t  ulEnd;
    
    if (stBytes >= ARMv5_CACHE_LOOP_OP_MAX_SIZE) {
        armDCacheV5FlushAll();                                          /*  DCACHE 全部回写             */
        armICacheInvalidateAll();                                       /*  ICACHE 全部无效             */
        
    } else {
        if (stBytes >= sizeof(PVOID)) {
            ulEnd = (addr_t)pvAdrs + stBytes - sizeof(PVOID);
        } else {
            ulEnd = (addr_t)pvAdrs;
        }
        armDCacheFlush(pvAdrs, (PVOID)ulEnd, ARMv5_CACHE_LINE_SIZE);    /*  部分回写                    */
        armICacheInvalidate(pvAdrs, (PVOID)ulEnd, ARMv5_CACHE_LINE_SIZE);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV5VmmAreaInv
** 功能描述: 指定类型的 CACHE 使部分或全部清空(回写内存)并无效(访问不命中)
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV5VmmAreaInv (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    if (cachetype == INSTRUCTION_CACHE) {
        armICacheInvalidateAll();                                       /*  ICACHE 全部无效             */
        
    } else {
        armDCacheV5ClearAll();                                          /*  全部回写并无效              */
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: archCacheV5Init
** 功能描述: 初始化 CACHE 
** 输　入  : pcacheop       CACHE 操作函数集
**           uiInstruction  指令 CACHE 参数
**           uiData         数据 CACHE 参数
**           pcMachineName  机器名称
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  armCacheV5Init (LW_CACHE_OP *pcacheop, 
                      CACHE_MODE   uiInstruction, 
                      CACHE_MODE   uiData, 
                      CPCHAR       pcMachineName)
{
    pcacheop->CACHEOP_iILoc      = CACHE_LOCATION_VIVT;
    pcacheop->CACHEOP_iDLoc      = CACHE_LOCATION_VIVT;
    pcacheop->CACHEOP_iCacheLine = ARMv5_CACHE_LINE_SIZE;
    
    pcacheop->CACHEOP_pfuncEnable  = armCacheV5Enable;
    pcacheop->CACHEOP_pfuncDisable = armCacheV5Disable;
    
    pcacheop->CACHEOP_pfuncLock    = armCacheV5Lock;                    /*  暂时不支持锁定操作          */
    pcacheop->CACHEOP_pfuncUnlock  = armCacheV5Unlock;
    
    pcacheop->CACHEOP_pfuncFlush      = armCacheV5Flush;
    pcacheop->CACHEOP_pfuncInvalidate = armCacheV5Invalidate;
    pcacheop->CACHEOP_pfuncClear      = armCacheV5Clear;
    pcacheop->CACHEOP_pfuncTextUpdate = armCacheV5TextUpdate;
    pcacheop->CACHEOP_pfuncVmmAreaInv = armCacheV5VmmAreaInv;
    
#if LW_CFG_VMM_EN > 0
    pcacheop->CACHEOP_pfuncDmaMalloc      = API_VmmDmaAlloc;
    pcacheop->CACHEOP_pfuncDmaMallocAlign = API_VmmDmaAllocAlign;
    pcacheop->CACHEOP_pfuncDmaFree        = API_VmmDmaFree;
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
}
/*********************************************************************************************************
** 函数名称: archCacheV5Reset
** 功能描述: 复位 CACHE 
** 输　入  : pcMachineName  机器名称
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  armCacheV5Reset (CPCHAR  pcMachineName)
{
    armICacheInvalidateAll();
    armDCacheV5Disable();
    armICacheDisable();
}

#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
