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
** 文   件   名: powerMShow.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 03 月 06 日
**
** 描        述: 显示当前正在计数的系统级功耗管理器信息.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if (LW_CFG_FIO_LIB_EN > 0) && (LW_CFG_POWERM_EN > 0) && (LW_CFG_MAX_POWERM_NODES > 0)
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
extern LW_OBJECT_HANDLE     _G_ulPowerMLock;
extern LW_CLASS_WAKEUP      _G_wuPowerM;
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
static const CHAR   _G_cPowerMInfoHdr[] = "\n\
     NAME       COUNT   MAX IDLE    CB     PARAM\n\
-------------- -------- -------- -------- --------\n";
/*********************************************************************************************************
** 函数名称: API_PowerMShow
** 功能描述: 显示当前正在计数的系统级功耗管理器信息
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
VOID  API_PowerMShow (VOID)
{
    REGISTER PLW_CLASS_POWERM_NODE  p_pmnTempNode;
    REGISTER PLW_LIST_LINE          plinePowerMOp;
             ULONG                  ulCounter;
    
    printf("active power management node show >>\n");
    printf(_G_cPowerMInfoHdr);                                          /*  打印欢迎信息                */
    
    __POWERM_LOCK();                                                    /*  上锁                        */
        
    for (plinePowerMOp  = _G_wuPowerM.WU_plineHeader;
         plinePowerMOp != LW_NULL;
         plinePowerMOp  = _list_line_get_next(plinePowerMOp)) {
    
        PLW_CLASS_WAKEUP_NODE   pwun = _LIST_ENTRY(plinePowerMOp,
                                                   LW_CLASS_WAKEUP_NODE,
                                                   WUN_lineManage);
        p_pmnTempNode = _LIST_ENTRY(pwun,
                                    LW_CLASS_POWERM_NODE,
                                    PMN_wunTimer);

        if (p_pmnTempNode->PMN_bInQ) {
            _WakeupStatus(&_G_wuPowerM, &p_pmnTempNode->PMN_wunTimer, &ulCounter);
        } else {
            ulCounter = 0ul;
        }
                                    
        printf("%-14s %8lx %8lx %8lx %8lx\n",
                      p_pmnTempNode->PMN_cPowerMName,
                      ulCounter,
                      p_pmnTempNode->PMN_ulCounterSave,
                      (addr_t)p_pmnTempNode->PMN_pfuncCallback,
                      (addr_t)p_pmnTempNode->PMN_pvArg);
    }
    
    __POWERM_UNLOCK();                                                  /*  解锁                        */

    printf("\n");
}
#endif                                                                  /*  LW_CFG_FIO_LIB_EN           */
                                                                        /*  LW_CFG_POWERM_EN            */
                                                                        /*  LW_CFG_MAX_POWERM_NODES     */
/*********************************************************************************************************
  END
*********************************************************************************************************/


