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
** 文   件   名: _PriorityInit.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 14 日
**
** 描        述: 这是系统优先级控制块初始化函数库。
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: _PriorityInit
** 功能描述: 初始化优先级控制块(附加初始化 _K_ptcbTCBIdTable )  一共初始化 LW_CFG_LOWEST_PRIO + 1 个元素
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _PriorityInit (VOID)
{
    REGISTER ULONG    ulI;
    
    for (ulI = 0; ulI < (LW_PRIO_LOWEST + 1); ulI++) {
        _K_pcbPriorityTable[ulI].PCB_pringReadyHeader     = LW_NULL;
        
        _K_pcbPriorityTable[ulI].PCB_ucPriority           = (UINT8)ulI;
        _K_pcbPriorityTable[ulI].PCB_usThreadCounter      = 0;
        _K_pcbPriorityTable[ulI].PCB_usThreadReadyCounter = 0;
        _K_pcbPriorityTable[ulI].PCB_usThreadCandCounter  = 0;
        
        _K_pcbPriorityTable[ulI].PCB_ucZ        = (UINT8)(ulI >> 6);
        _K_pcbPriorityTable[ulI].PCB_ucMaskBitZ = (UINT8)(1 << _K_pcbPriorityTable[ulI].PCB_ucZ);

        _K_pcbPriorityTable[ulI].PCB_ucY        = (UINT8)((ulI >> 3) & 0x07);
        _K_pcbPriorityTable[ulI].PCB_ucMaskBitY = (UINT8)(1 << _K_pcbPriorityTable[ulI].PCB_ucY);
        
        _K_pcbPriorityTable[ulI].PCB_ucX        = (UINT8)(ulI & 0x07);
        _K_pcbPriorityTable[ulI].PCB_ucMaskBitX = (UINT8)(1 << _K_pcbPriorityTable[ulI].PCB_ucX);
    }
    
    for (ulI = 0; ulI < LW_CFG_MAX_THREADS; ulI++) {
        _K_ptcbTCBIdTable[ulI] = LW_NULL;
    }
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
