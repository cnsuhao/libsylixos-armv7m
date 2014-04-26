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
** 文   件   名: powerM.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 03 月 06 日
**
** 描        述: 系统级功耗管理器. 根据用户配置, 可分为多个功耗控制点, 每个控制点可以设置延迟时间.
                 例如, 系统中含有 LCD, 那么可以分为两个功耗控制点. 当经历一段时间无人操作后, 率先
                 关闭 LCD 背光, 在经历一段时间无人操作后, 可关闭 LCD 显示系统以降低功耗.
*********************************************************************************************************/

#ifndef __POWERM_H
#define __POWERM_H

/*********************************************************************************************************
  裁减控制
*********************************************************************************************************/
#if (LW_CFG_POWERM_EN > 0) && (LW_CFG_MAX_POWERM_NODES > 0)

/*********************************************************************************************************
  资源管理
*********************************************************************************************************/

#ifdef __SYLIXOS_KERNEL

#ifdef  __POWERM_MAIN_FILE
       LW_CLASS_OBJECT_RESRC    _G_resrcPower;
#else
extern LW_CLASS_OBJECT_RESRC    _G_resrcPower;
#endif                                                                  /*  __POWERM_MAIN_FILE          */

#define __POWERM_LOCK()         API_SemaphoreMPend(_G_ulPowerMLock, LW_OPTION_WAIT_INFINITE)
#define __POWERM_UNLOCK()       API_SemaphoreMPost(_G_ulPowerMLock)

#endif                                                                  /*  __SYLIXOS_KERNEL            */

/*********************************************************************************************************
  功耗管理节点
*********************************************************************************************************/
typedef struct {
    LW_LIST_MONO            PMN_monoResrcList;                          /*  资源链表                    */
    BOOL                    PMN_bIsUsing;                               /*  是否被创建了                */
    
    LW_CLASS_WAKEUP_NODE    PMN_wunTimer;
#define PMN_bInQ            PMN_wunTimer.WUN_bInQ
#define PMN_ulCounter       PMN_wunTimer.WUN_ulCounter
    
    ULONG                   PMN_ulCounterSave;                          /*  计数保留值                  */
    PVOID                   PMN_pvArg;                                  /*  回调参数                    */
    LW_HOOK_FUNC            PMN_pfuncCallback;                          /*  回调函数                    */
    
    UINT16                  PMN_usIndex;                                /*  索引下标                    */
    CHAR                    PMN_cPowerMName[LW_CFG_OBJECT_NAME_SIZE];   /*  名字                        */
} LW_CLASS_POWERM_NODE;
typedef LW_CLASS_POWERM_NODE    *PLW_CLASS_POWERM_NODE;

/*********************************************************************************************************
  INLINE FUNCTION
*********************************************************************************************************/

static LW_INLINE INT  _PowerM_Index_Invalid (UINT16    usIndex)
{
    return  (usIndex >= LW_CFG_MAX_POWERM_NODES);
}

#endif                                                                  /*  LW_CFG_POWERM_EN            */
                                                                        /*  LW_CFG_MAX_POWERM_NODES     */
#endif                                                                  /*  __POWERM_H                  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
