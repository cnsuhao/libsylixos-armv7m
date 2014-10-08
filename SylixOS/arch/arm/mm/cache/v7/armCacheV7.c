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
** 文   件   名: armCacheV7.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 12 月 09 日
**
** 描        述: ARMv7 体系构架 CACHE 驱动.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_CACHE_EN > 0
#include "../armCacheCommon.h"
#include "../../mmu/armMmuCommon.h"
#include "../../../common/cp15/armCp15.h"
/*********************************************************************************************************
  L2 CACHE 支持
*********************************************************************************************************/
#if LW_CFG_ARM_CACHE_L2 > 0
#include "../l2/armL2.h"
/*********************************************************************************************************
  L1 CACHE 状态
*********************************************************************************************************/
static INT      iCacheStatus = 0;
#define L1_CACHE_I_EN   0x01
#define L1_CACHE_D_EN   0x02
#define L1_CACHE_EN     (L1_CACHE_I_EN | L1_CACHE_D_EN)
#define L1_CACHE_DIS    0x00
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
#define ARMV7_CSSELR_IND_DATA_UNIFIED   0
#define ARMV7_CSSELR_IND_INSTRUCTION    1

extern VOID     armDCacheV7Disable(VOID);
extern VOID     armDCacheV7FlushAll(VOID);
extern VOID     armDCacheV7ClearAll(VOID);
/*********************************************************************************************************
  CACHE 参数
*********************************************************************************************************/
static UINT32                           uiArmV7CacheLineSize;
#define ARMv7_CACHE_LOOP_OP_MAX_SIZE    (32 * LW_CFG_KB_SIZE)
/*********************************************************************************************************
** 函数名称: armCacheV7Enable
** 功能描述: 使能 CACHE 
** 输　入  : cachetype      INSTRUCTION_CACHE / DATA_CACHE
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  armCacheV7Enable (LW_CACHE_TYPE  cachetype)
{
    if (cachetype == INSTRUCTION_CACHE) {
        armICacheEnable();
#if LW_CFG_ARM_CACHE_L2 > 0
        iCacheStatus |= L1_CACHE_I_EN;
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
        armBranchPredictionEnable();

    } else {
        armDCacheEnable();
#if LW_CFG_ARM_CACHE_L2 > 0
        iCacheStatus |= L1_CACHE_D_EN;
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
    }
    
#if LW_CFG_ARM_CACHE_L2 > 0
    if ((iCacheStatus == L1_CACHE_EN) && !armL2IsEnable()) {
        armL2Enable();
    }
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV7Disable
** 功能描述: 禁能 CACHE 
** 输　入  : cachetype      INSTRUCTION_CACHE / DATA_CACHE
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  armCacheV7Disable (LW_CACHE_TYPE  cachetype)
{
    if (cachetype == INSTRUCTION_CACHE) {
        armICacheDisable();
#if LW_CFG_ARM_CACHE_L2 > 0
        iCacheStatus &= ~L1_CACHE_I_EN;
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
        armBranchPredictionDisable();
        
    } else {
        armDCacheV7Disable();
#if LW_CFG_ARM_CACHE_L2 > 0
        iCacheStatus &= ~L1_CACHE_D_EN;
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
    }
    
#if LW_CFG_ARM_CACHE_L2 > 0
    if ((iCacheStatus == L1_CACHE_DIS) && armL2IsEnable()) {
        armL2Disable();
    }
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
     
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV7Flush
** 功能描述: CACHE 脏数据回写
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
** 注  意  : 由于 L2 为物理地址 tag 所以这里暂时使用 L2 全部回写指令.
*********************************************************************************************************/
static INT	armCacheV7Flush (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    addr_t  ulEnd;
    
    if (cachetype == DATA_CACHE) {
        if (stBytes >= ARMv7_CACHE_LOOP_OP_MAX_SIZE) {
            armDCacheV7FlushAll();                                      /*  全部回写                    */
        
        } else {
            ARM_CACHE_GET_END(pvAdrs, stBytes, ulEnd, uiArmV7CacheLineSize);
            armDCacheFlush(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize); /*  部分回写                    */
        }
        
#if LW_CFG_ARM_CACHE_L2 > 0
        armL2FlushAll();
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV7FlushPage
** 功能描述: CACHE 脏数据回写
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           pvPdrs        物理地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV7FlushPage (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, PVOID  pvPdrs, size_t  stBytes)
{
    addr_t  ulEnd;
    
    if (cachetype == DATA_CACHE) {
        if (stBytes >= ARMv7_CACHE_LOOP_OP_MAX_SIZE) {
            armDCacheV7FlushAll();                                      /*  全部回写                    */
        
        } else {
            ARM_CACHE_GET_END(pvAdrs, stBytes, ulEnd, uiArmV7CacheLineSize);
            armDCacheFlush(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize); /*  部分回写                    */
        }
        
#if LW_CFG_ARM_CACHE_L2 > 0
        armL2Flush(pvPdrs, stBytes);
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV7Invalidate
** 功能描述: 指定类型的 CACHE 使部分无效(访问不命中)
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址 (pvAdrs 必须等于物理地址)
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
** 注  意  : 此函数如果操作 DCACHE pvAdrs 虚拟地址与物理地址必须相同.
*********************************************************************************************************/
static INT	armCacheV7Invalidate (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    addr_t  ulEnd;

    if (cachetype == INSTRUCTION_CACHE) {
        if (stBytes >= ARMv7_CACHE_LOOP_OP_MAX_SIZE) {
            armICacheInvalidateAll();                                   /*  ICACHE 全部无效             */
        
        } else {
            ARM_CACHE_GET_END(pvAdrs, stBytes, ulEnd, uiArmV7CacheLineSize);
            armICacheInvalidate(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize);
        }
    } else {
        if (stBytes > 0) {                                              /*  必须 > 0                    */
            addr_t  ulStart = (addr_t)pvAdrs;
            
            if (ulStart & ((addr_t)uiArmV7CacheLineSize - 1)) {         /*  起始地址非 cache line 对齐  */
                ulStart &= ~((addr_t)uiArmV7CacheLineSize - 1);
                armDCacheClear((PVOID)ulStart, (PVOID)ulStart, uiArmV7CacheLineSize);
                ulStart += uiArmV7CacheLineSize;
            }
            
            ulEnd = ulStart + stBytes;
            if (ulEnd & ((addr_t)uiArmV7CacheLineSize - 1)) {           /*  结束地址非 cache line 对齐  */
                ulEnd &= ~((addr_t)uiArmV7CacheLineSize - 1);
                armDCacheClear((PVOID)ulEnd, (PVOID)ulEnd, uiArmV7CacheLineSize);
            }
                                                                        /*  仅无效对齐部分              */
            armDCacheInvalidate((PVOID)ulStart, (PVOID)ulEnd, uiArmV7CacheLineSize);
            
#if LW_CFG_ARM_CACHE_L2 > 0
            armL2Invalidate(pvAdrs, stBytes);                           /*  虚拟与物理地址必须相同      */
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
        } else {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "stBytes == 0.\r\n");
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV7InvalidatePage
** 功能描述: 指定类型的 CACHE 使部分无效(访问不命中)
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           pvPdrs        物理地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV7InvalidatePage (LW_CACHE_TYPE cachetype, PVOID pvAdrs, PVOID pvPdrs, size_t stBytes)
{
    addr_t  ulEnd;
    
    if (cachetype == INSTRUCTION_CACHE) {
        if (stBytes >= ARMv7_CACHE_LOOP_OP_MAX_SIZE) {
            armICacheInvalidateAll();                                   /*  ICACHE 全部无效             */
        
        } else {
            ARM_CACHE_GET_END(pvAdrs, stBytes, ulEnd, uiArmV7CacheLineSize);
            armICacheInvalidate(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize);
        }
    } else {
        if (stBytes > 0) {                                              /*  必须 > 0                    */
            addr_t  ulStart = (addr_t)pvAdrs;
            
            if (ulStart & ((addr_t)uiArmV7CacheLineSize - 1)) {         /*  起始地址非 cache line 对齐  */
                ulStart &= ~((addr_t)uiArmV7CacheLineSize - 1);
                armDCacheClear((PVOID)ulStart, (PVOID)ulStart, uiArmV7CacheLineSize);
                ulStart += uiArmV7CacheLineSize;
            }
            
            ulEnd = ulStart + stBytes;
            if (ulEnd & ((addr_t)uiArmV7CacheLineSize - 1)) {           /*  结束地址非 cache line 对齐  */
                ulEnd &= ~((addr_t)uiArmV7CacheLineSize - 1);
                armDCacheClear((PVOID)ulEnd, (PVOID)ulEnd, uiArmV7CacheLineSize);
            }
                                                                        /*  仅无效对齐部分              */
            armDCacheInvalidate((PVOID)ulStart, (PVOID)ulEnd, uiArmV7CacheLineSize);
            
#if LW_CFG_ARM_CACHE_L2 > 0
            armL2Invalidate(pvPdrs, stBytes);                           /*  虚拟与物理地址必须相同      */
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
        } else {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "stBytes == 0.\r\n");
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV7Clear
** 功能描述: 指定类型的 CACHE 使部分或全部清空(回写内存)并无效(访问不命中)
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
** 注  意  : 由于 L2 为物理地址 tag 所以这里暂时使用 L2 全部回写并无效指令.
*********************************************************************************************************/
static INT	armCacheV7Clear (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    addr_t  ulEnd;

    if (cachetype == INSTRUCTION_CACHE) {
        if (stBytes >= ARMv7_CACHE_LOOP_OP_MAX_SIZE) {
            armICacheInvalidateAll();                                   /*  ICACHE 全部无效             */
            
        } else {
            ARM_CACHE_GET_END(pvAdrs, stBytes, ulEnd, uiArmV7CacheLineSize);
            armICacheInvalidate(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize);
        }
    } else {
        if (stBytes >= ARMv7_CACHE_LOOP_OP_MAX_SIZE) {
            armDCacheV7ClearAll();                                      /*  全部回写并无效              */
        
        } else {
            ARM_CACHE_GET_END(pvAdrs, stBytes, ulEnd, uiArmV7CacheLineSize);
            armDCacheClear(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize); /*  部分回写并无效              */
        }
        
#if LW_CFG_ARM_CACHE_L2 > 0
        armL2ClearAll();
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV7ClearPage
** 功能描述: 指定类型的 CACHE 使部分或全部清空(回写内存)并无效(访问不命中)
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           pvPdrs        物理地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV7ClearPage (LW_CACHE_TYPE cachetype, PVOID pvAdrs, PVOID pvPdrs, size_t stBytes)
{
    addr_t  ulEnd;
    
    if (cachetype == INSTRUCTION_CACHE) {
        if (stBytes >= ARMv7_CACHE_LOOP_OP_MAX_SIZE) {
            armICacheInvalidateAll();                                   /*  ICACHE 全部无效             */
            
        } else {
            ARM_CACHE_GET_END(pvAdrs, stBytes, ulEnd, uiArmV7CacheLineSize);
            armICacheInvalidate(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize);
        }
    } else {
        if (stBytes >= ARMv7_CACHE_LOOP_OP_MAX_SIZE) {
            armDCacheV7ClearAll();                                      /*  全部回写并无效              */
        
        } else {
            ARM_CACHE_GET_END(pvAdrs, stBytes, ulEnd, uiArmV7CacheLineSize);
            armDCacheClear(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize); /*  部分回写并无效              */
        }
        
#if LW_CFG_ARM_CACHE_L2 > 0
        armL2Clear(pvPdrs, stBytes);
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: armCacheV7Lock
** 功能描述: 锁定指定类型的 CACHE 
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV7Lock (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    _ErrorHandle(ENOSYS);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: armCacheV7Unlock
** 功能描述: 解锁指定类型的 CACHE 
** 输　入  : cachetype     CACHE 类型
**           pvAdrs        虚拟地址
**           stBytes       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT	armCacheV7Unlock (LW_CACHE_TYPE  cachetype, PVOID  pvAdrs, size_t  stBytes)
{
    _ErrorHandle(ENOSYS);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: armCacheV7TextUpdate
** 功能描述: 清空(回写内存) D CACHE 无效(访问不命中) I CACHE
** 输　入  : pvAdrs                        虚拟地址
**           stBytes                       长度
** 输　出  : ERROR or OK
** 全局变量: 
** 调用模块: 
** 注  意  : L2 cache 为统一 CACHE 所以 text update 不需要操作 L2 cache.
*********************************************************************************************************/
static INT	armCacheV7TextUpdate (PVOID  pvAdrs, size_t  stBytes)
{
    addr_t  ulEnd;
    
    if (stBytes >= ARMv7_CACHE_LOOP_OP_MAX_SIZE) {
        armDCacheV7FlushAll();                                          /*  DCACHE 全部回写             */
        armICacheInvalidateAll();                                       /*  ICACHE 全部无效             */
        
    } else {
        ARM_CACHE_GET_END(pvAdrs, stBytes, ulEnd, uiArmV7CacheLineSize);
        armDCacheFlush(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize);     /*  部分回写                    */
        armICacheInvalidate(pvAdrs, (PVOID)ulEnd, uiArmV7CacheLineSize);
    }
    
    if (LW_NCPUS > 1) {
        armBranchPredictorInvalidateInnerShareable();                   /*  所有核清除分支预测          */
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: archCacheV7Init
** 功能描述: 初始化 CACHE 
** 输　入  : pcacheop       CACHE 操作函数集
**           uiInstruction  指令 CACHE 参数
**           uiData         数据 CACHE 参数
**           pcMachineName  机器名称
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  armCacheV7Init (LW_CACHE_OP *pcacheop, 
                      CACHE_MODE   uiInstruction, 
                      CACHE_MODE   uiData, 
                      CPCHAR       pcMachineName)
{
#if LW_CFG_ARM_CACHE_L2 > 0
    armL2Init(uiInstruction, uiData, pcMachineName);
#endif                                                                  /*  LW_CFG_ARM_CACHE_L2 > 0     */

    if ((lib_strcmp(pcMachineName, ARM_MACHINE_A5) == 0) ||
        (lib_strcmp(pcMachineName, ARM_MACHINE_A7) == 0)) {
        pcacheop->CACHEOP_iILoc      = CACHE_LOCATION_VIPT;
        pcacheop->CACHEOP_iDLoc      = CACHE_LOCATION_PIPT;
        pcacheop->CACHEOP_iCacheLine = 32;
        
    } else if (lib_strcmp(pcMachineName, ARM_MACHINE_A9) == 0) {
        pcacheop->CACHEOP_iILoc      = CACHE_LOCATION_VIPT;
        pcacheop->CACHEOP_iDLoc      = CACHE_LOCATION_PIPT;
        pcacheop->CACHEOP_iCacheLine = 32;
        armAuxControlFeatureEnable(AUX_CTRL_A9_L1_PREFETCH);            /*  Cortex-A9 使能 L1 预取      */
    
    } else if (lib_strcmp(pcMachineName, ARM_MACHINE_A8) == 0) {
        pcacheop->CACHEOP_iILoc      = CACHE_LOCATION_VIPT;
        pcacheop->CACHEOP_iDLoc      = CACHE_LOCATION_PIPT;
        pcacheop->CACHEOP_iCacheLine = 64;
        armAuxControlFeatureEnable(AUX_CTRL_A8_FORCE_ETM_CLK |
                                   AUX_CTRL_A8_FORCE_MAIN_CLK |
                                   AUX_CTRL_A8_L1NEON |
                                   AUX_CTRL_A8_FORCE_NEON_CLK |
                                   AUX_CTRL_A8_FORCE_NEON_SIGNAL);
    
    } else if (lib_strcmp(pcMachineName, ARM_MACHINE_A15) == 0) {
        pcacheop->CACHEOP_iILoc      = CACHE_LOCATION_PIPT;
        pcacheop->CACHEOP_iDLoc      = CACHE_LOCATION_PIPT;
        pcacheop->CACHEOP_iCacheLine = 64;
        armAuxControlFeatureEnable(AUX_CTRL_A15_FORCE_MAIN_CLK |
                                   AUX_CTRL_A15_FORCE_NEON_CLK);
    }
    
    uiArmV7CacheLineSize = (UINT32)pcacheop->CACHEOP_iCacheLine;
    
    pcacheop->CACHEOP_pfuncEnable  = armCacheV7Enable;
    pcacheop->CACHEOP_pfuncDisable = armCacheV7Disable;
    
    pcacheop->CACHEOP_pfuncLock    = armCacheV7Lock;                    /*  暂时不支持锁定操作          */
    pcacheop->CACHEOP_pfuncUnlock  = armCacheV7Unlock;
    
    pcacheop->CACHEOP_pfuncFlush          = armCacheV7Flush;
    pcacheop->CACHEOP_pfuncFlushPage      = armCacheV7FlushPage;
    pcacheop->CACHEOP_pfuncInvalidate     = armCacheV7Invalidate;
    pcacheop->CACHEOP_pfuncInvalidatePage = armCacheV7InvalidatePage;
    pcacheop->CACHEOP_pfuncClear          = armCacheV7Clear;
    pcacheop->CACHEOP_pfuncClearPage      = armCacheV7ClearPage;
    pcacheop->CACHEOP_pfuncTextUpdate     = armCacheV7TextUpdate;
    
#if LW_CFG_VMM_EN > 0
    pcacheop->CACHEOP_pfuncDmaMalloc      = API_VmmDmaAlloc;
    pcacheop->CACHEOP_pfuncDmaMallocAlign = API_VmmDmaAllocAlign;
    pcacheop->CACHEOP_pfuncDmaFree        = API_VmmDmaFree;
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
}
/*********************************************************************************************************
** 函数名称: archCacheV7Reset
** 功能描述: 复位 CACHE 
** 输　入  : pcMachineName  机器名称
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
** 注  意  : 如果有 lockdown 必须首先 unlock & invalidate 才能启动
*********************************************************************************************************/
VOID  armCacheV7Reset (CPCHAR  pcMachineName)
{
    armICacheInvalidateAll();
    armDCacheV7Disable();
    armICacheDisable();
    armBranchPredictorInvalidate();
}

#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
