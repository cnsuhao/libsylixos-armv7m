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
** 文   件   名: armL2x0.c
**
** 创   建   人: Jiao.Jinxing (焦进星)
**
** 文件创建日期: 2013 年 12 月 09 日
**
** 描        述: ARM 体系构架 L2 CACHE 控制器 PL210 / PL220 / PL310 驱动.
** 注        意: 无论处理器是大端还是小端, L2 控制器均为小端.
*********************************************************************************************************/
#define  __SYLIXOS_IO
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
/*********************************************************************************************************
  裁减配置
*********************************************************************************************************/
#if LW_CFG_CACHE_EN > 0 && LW_CFG_ARM_CACHE_L2 > 0
#include "armL2.h"
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
static VOID armL2x0Sync(L2C_DRVIER  *pl2cdrv);
static VOID armL2x0InvalidateAll(L2C_DRVIER  *pl2cdrv);
static VOID armL2x0ClearAll(L2C_DRVIER  *pl2cdrv);
/*********************************************************************************************************
** 函数名称: armL2x0Enable
** 功能描述: 使能 L2 CACHE 控制器
** 输　入  : pl2cdrv            驱动结构
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
** 注  意  : 如果有 lockdown 必须首先 unlock & invalidate 才能启动 L2.
*********************************************************************************************************/
static VOID armL2x0Enable (L2C_DRVIER  *pl2cdrv)
{
    while (!(read32_le(L2C_BASE(pl2cdrv) + L2C_CTRL) & 0x01)) {
        write32_le(L2C_AUX(pl2cdrv), L2C_BASE(pl2cdrv) + L2C_AUX_CTRL); /*  有些处理器需要此操作        */
        armL2x0InvalidateAll(pl2cdrv);
        write32_le(0x01, L2C_BASE(pl2cdrv) + L2C_CTRL);
        armL2x0Sync(pl2cdrv);
    }
}
/*********************************************************************************************************
** 函数名称: armL2x0Disable
** 功能描述: 禁能 L2 CACHE 控制器
** 输　入  : pl2cdrv            驱动结构
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID armL2x0Disable (L2C_DRVIER  *pl2cdrv)
{
    while (read32_le(L2C_BASE(pl2cdrv) + L2C_CTRL) & 0x01) {
        armL2x0ClearAll(pl2cdrv);
        write32_le(0x00, L2C_BASE(pl2cdrv) + L2C_CTRL);
    }
}
/*********************************************************************************************************
** 函数名称: armL2x0IsEnable
** 功能描述: 检查 L2 CACHE 控制器是否使能
** 输　入  : pl2cdrv            驱动结构
** 输　出  : 是否使能
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static BOOL armL2x0IsEnable (L2C_DRVIER  *pl2cdrv)
{
    return  (read32_le(L2C_BASE(pl2cdrv) + L2C_CTRL) & 0x01);
}
/*********************************************************************************************************
** 函数名称: armL2x0Sync
** 功能描述: L2 CACHE 控制器同步 (理论上 PL310 不需要等待)
** 输　入  : pl2cdrv            驱动结构
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID armL2x0Sync (L2C_DRVIER  *pl2cdrv)
{
    while (read32_le(L2C_BASE(pl2cdrv) + L2C_CACHE_SYNC) & 0x01);
}
/*********************************************************************************************************
** 函数名称: armPl310FlushAll
** 功能描述: L2 CACHE 控制器回写所有数据
** 输　入  : pl2cdrv            驱动结构
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID armL2x0FlushAll (L2C_DRVIER  *pl2cdrv)
{
    write32_le(L2C_WAYMASK(pl2cdrv), L2C_BASE(pl2cdrv) + L2C_CLEAN_WAY);
    while (read32_le(L2C_BASE(pl2cdrv) + L2C_CLEAN_WAY));
    armL2x0Sync(pl2cdrv);
}
/*********************************************************************************************************
** 函数名称: armPl310InvalidateAll
** 功能描述: L2 CACHE 控制器无效所有数据
** 输　入  : pl2cdrv            驱动结构
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID armL2x0InvalidateAll (L2C_DRVIER  *pl2cdrv)
{
    write32_le(L2C_WAYMASK(pl2cdrv), L2C_BASE(pl2cdrv) + L2C_INV_WAY);
    while (read32_le(L2C_BASE(pl2cdrv) + L2C_INV_WAY));
    armL2x0Sync(pl2cdrv);
}
/*********************************************************************************************************
** 函数名称: armPl310ClearAll
** 功能描述: L2 CACHE 控制器回写并无效所有数据
** 输　入  : pl2cdrv            驱动结构
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID armL2x0ClearAll (L2C_DRVIER  *pl2cdrv)
{
    write32_le(L2C_WAYMASK(pl2cdrv), L2C_BASE(pl2cdrv) + L2C_CLEAN_INV_WAY);
    while (read32_le(L2C_BASE(pl2cdrv) + L2C_CLEAN_INV_WAY));
    armL2x0Sync(pl2cdrv);
}
/*********************************************************************************************************
** 函数名称: armL2x0Init
** 功能描述: 初始化 L2 CACHE 控制器
** 输　入  : pl2cdrv            驱动结构
**           uiInstruction      指令 CACHE 类型
**           uiData             数据 CACHE 类型
**           pcMachineName      机器名称
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID armL2x0Init (L2C_DRVIER  *pl2cdrv,
                  CACHE_MODE   uiInstruction,
                  CACHE_MODE   uiData,
                  CPCHAR       pcMachineName)
{
    pl2cdrv->L2CD_pfuncEnable        = armL2x0Enable;
    pl2cdrv->L2CD_pfuncDisable       = armL2x0Disable;
    pl2cdrv->L2CD_pfuncIsEnable      = armL2x0IsEnable;
    pl2cdrv->L2CD_pfuncSync          = armL2x0Sync;
    pl2cdrv->L2CD_pfuncFlush         = LW_NULL;
    pl2cdrv->L2CD_pfuncFlushAll      = armL2x0FlushAll;
    pl2cdrv->L2CD_pfuncInvalidate    = LW_NULL;
    pl2cdrv->L2CD_pfuncInvalidateAll = armL2x0InvalidateAll;
    pl2cdrv->L2CD_pfuncClear         = LW_NULL;
    pl2cdrv->L2CD_pfuncClearAll      = armL2x0ClearAll;
}

#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */
                                                                        /*  LW_CFG_ARM_CACHE_L2 > 0     */
/*********************************************************************************************************
  END
*********************************************************************************************************/
