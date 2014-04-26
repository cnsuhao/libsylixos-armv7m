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
** 文   件   名: armCache.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 12 月 09 日
**
** 描        述: ARM 体系构架 CACHE 驱动.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_CACHE_EN > 0
#include "cache/v4/armCacheV4.h"
#include "cache/v5/armCacheV5.h"
#include "cache/v6/armCacheV6.h"
#include "cache/v7/armCacheV7.h"
/*********************************************************************************************************
** 函数名称: archCacheInit
** 功能描述: 初始化 CACHE 
** 输　入  : uiInstruction  指令 CACHE 参数
**           uiData         数据 CACHE 参数
**           pcMachineName  机器名称
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  archCacheInit (CACHE_MODE  uiInstruction, CACHE_MODE  uiData, CPCHAR  pcMachineName)
{
    LW_CACHE_OP *pcacheop = API_CacheGetLibBlock();
    
    _DebugHandle(__LOGMESSAGE_LEVEL, pcMachineName);
    _DebugHandle(__LOGMESSAGE_LEVEL, " L1 cache controller initialization.\r\n");

    if (lib_strcmp(pcMachineName, ARM_MACHINE_920) == 0) {
        armCacheV4Init(pcacheop, uiInstruction, uiData, pcMachineName);
        
    } else if (lib_strcmp(pcMachineName, ARM_MACHINE_926) == 0) {
        armCacheV5Init(pcacheop, uiInstruction, uiData, pcMachineName);
        
    } else if ((lib_strcmp(pcMachineName, ARM_MACHINE_1136) == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_1176) == 0)) {
        armCacheV6Init(pcacheop, uiInstruction, uiData, pcMachineName);
        
    } else if ((lib_strcmp(pcMachineName, ARM_MACHINE_A5)  == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_A7)  == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_A8)  == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_A9)  == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_A15) == 0)) {
        armCacheV7Init(pcacheop, uiInstruction, uiData, pcMachineName);
    
    } else {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "unknown machine name.\r\n");
    }
}
/*********************************************************************************************************
** 函数名称: archCacheReset
** 功能描述: 复位 CACHE, MMU 初始化时需要调用此函数
** 输　入  : pcMachineName  机器名称
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  archCacheReset (CPCHAR  pcMachineName)
{
    if (lib_strcmp(pcMachineName, ARM_MACHINE_920) == 0) {
        armCacheV4Reset(pcMachineName);
        
    } else if (lib_strcmp(pcMachineName, ARM_MACHINE_926) == 0) {
        armCacheV5Reset(pcMachineName);
        
    } else if ((lib_strcmp(pcMachineName, ARM_MACHINE_1136) == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_1176) == 0)) {
        armCacheV6Reset(pcMachineName);
        
    } else if ((lib_strcmp(pcMachineName, ARM_MACHINE_A5)  == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_A7)  == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_A8)  == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_A9)  == 0) ||
               (lib_strcmp(pcMachineName, ARM_MACHINE_A15) == 0)) {
        armCacheV7Reset(pcMachineName);
    
    } else {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "unknown machine name.\r\n");
    }
}

#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
