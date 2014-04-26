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
** 文   件   名: powerMLib.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 03 月 06 日
**
** 描        述: 系统级功耗管理器.

** BUG
2008.03.16  服务线程不可以使用 select() 调用.
2009.04.09  使用资源有序分配.
2009.08.31  增加注释.
2012.03.12  使用自动 attr
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#define  __POWERM_MAIN_FILE
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁减控制
*********************************************************************************************************/
#if (LW_CFG_POWERM_EN > 0) && (LW_CFG_MAX_POWERM_NODES > 0)
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
LW_CLASS_POWERM_NODE            _G__pmnControlNode[LW_CFG_MAX_POWERM_NODES];
LW_OBJECT_HANDLE                _G_ulPowerMLock;
LW_CLASS_WAKEUP                 _G_wuPowerM;
/*********************************************************************************************************
  函数声明
*********************************************************************************************************/
static PVOID  _PowerMThread(PVOID  pvArg);
/*********************************************************************************************************
** 函数名称: _PowerMListInit
** 功能描述: 初始化系统级功耗管理器链表
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  _PowerMListInit (VOID)
{
#if LW_CFG_MAX_POWERM_NODES == 1                                        /*  仅有一个节点                */

    REGISTER PLW_CLASS_POWERM_NODE      p_pmnTemp1;
    
    _G_resrcPower.RESRC_pmonoFreeHeader = &_G__pmnControlNode[0].PMN_monoResrcList;
    
    p_pmnTemp1 = &_G__pmnControlNode[0];
    p_pmnTemp1->PMN_usIndex = 0;
    
    _INIT_LIST_MONO_HEAD(_G_resrcPower.RESRC_pmonoFreeHeader);
    
    _G_resrcPower.RESRC_pmonoFreeTail = _G_resrcPower.RESRC_pmonoFreeHeader;
    
#else                                                                   /*  包含多个控制节点            */
    
    REGISTER ULONG                  ulI;
    REGISTER PLW_LIST_MONO          pmonoTemp1;
    REGISTER PLW_LIST_MONO          pmonoTemp2;
    REGISTER PLW_CLASS_POWERM_NODE  p_pmnTemp1;
    REGISTER PLW_CLASS_POWERM_NODE  p_pmnTemp2;
    
    _G_resrcPower.RESRC_pmonoFreeHeader = &_G__pmnControlNode[0].PMN_monoResrcList;
    
    p_pmnTemp1 = &_G__pmnControlNode[0];
    p_pmnTemp2 = &_G__pmnControlNode[1];
    
    for (ulI = 0; ulI < ((LW_CFG_MAX_POWERM_NODES) - 1); ulI++) {
    
        p_pmnTemp1->PMN_usIndex = (UINT16)ulI;
        
        pmonoTemp1 = &p_pmnTemp1->PMN_monoResrcList;
        pmonoTemp2 = &p_pmnTemp2->PMN_monoResrcList;
        
        _LIST_MONO_LINK(pmonoTemp1, pmonoTemp2);
        
        p_pmnTemp1++;
        p_pmnTemp2++;
    }
    
    p_pmnTemp1->PMN_usIndex = (UINT16)ulI;
    
    pmonoTemp1 = &p_pmnTemp1->PMN_monoResrcList;
    
    _INIT_LIST_MONO_HEAD(pmonoTemp1);
    
    _G_resrcPower.RESRC_pmonoFreeTail = pmonoTemp1;
    
#endif                                                                  /*  LW_CFG_MAX_POWERM_NODES == 1*/

    _G_resrcPower.RESRC_uiUsed    = 0;
    _G_resrcPower.RESRC_uiMaxUsed = 0;

    __WAKEUP_INIT(&_G_wuPowerM);
}
/*********************************************************************************************************
** 函数名称: _PowerMInit
** 功能描述: 初始化系统级功耗管理器
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _PowerMInit (VOID)
{
    LW_CLASS_THREADATTR       threadattr;

    _PowerMListInit();                                                  /*  初始化空闲链表              */
    
    _G_ulPowerMLock = API_SemaphoreMCreate("power_lock", LW_PRIO_DEF_CEILING,
                               LW_OPTION_WAIT_PRIORITY | LW_OPTION_DELETE_SAFE |
                               LW_OPTION_INHERIT_PRIORITY | LW_OPTION_OBJECT_GLOBAL,
                               LW_NULL);                                /*  建立锁信号量                */

    if (!_G_ulPowerMLock) {
        return;                                                         /*  失败                        */
    }
    
    API_ThreadAttrBuild(&threadattr, 
                        LW_CFG_THREAD_POWERM_STK_SIZE, 
                        LW_PRIO_T_POWER, 
                        LW_OPTION_THREAD_STK_CHK | LW_OPTION_THREAD_SAFE | LW_OPTION_OBJECT_GLOBAL, 
                        LW_NULL);
    
	_S_ulPowerMId = API_ThreadCreate("t_power", 
	                                 _PowerMThread,
	                                 &threadattr,
	                                 LW_NULL);
}
/*********************************************************************************************************
** 函数名称: _PowerMThread
** 功能描述: 初始化系统级功耗管理器
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
** 注  意  : 对定时器的操作没有关闭中断. 所以需要 signal 的操作不能在中断中调用.
*********************************************************************************************************/
static  PVOID  _PowerMThread (PVOID  pvArg)
{
    REGISTER PLW_CLASS_POWERM_NODE  p_pmnNode;
    REGISTER PLW_CLASS_WAKEUP_NODE  pwun;
    
    for (;;) {
        ULONG   ulCounter = LW_CFG_TICKS_PER_SEC;
        
        __POWERM_LOCK();                                                /*  上锁                        */
                           
        __WAKEUP_PASS_FIRST(&_G_wuPowerM, pwun, ulCounter);
        
        p_pmnNode = _LIST_ENTRY(pwun, LW_CLASS_POWERM_NODE, PMN_wunTimer);
        
        _WakeupDel(&_G_wuPowerM, pwun);
        
        if (p_pmnNode->PMN_pfuncCallback) {
            __POWERM_UNLOCK();                                          /*  暂时解锁                    */
            
            p_pmnNode->PMN_pfuncCallback(p_pmnNode->PMN_pvArg);
            
            __POWERM_LOCK();                                            /*  上锁                        */
        }
        
        __WAKEUP_PASS_SECOND();
        
        __WAKEUP_PASS_END();
        
        __POWERM_UNLOCK();                                              /*  解锁                        */
        
        API_TimeSleep(LW_CFG_TICKS_PER_SEC);                            /*  等待一秒                    */
    }
    
    return  (LW_NULL);
}
/*********************************************************************************************************
** 函数名称: _Allocate_PowerM_Object
** 功能描述: 从空闲功耗管理器池中, 获得一个空闲的控制块.
** 输　入  : NONE
** 输　出  : 获得控制块地址, 失败返回 NULL
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PLW_CLASS_POWERM_NODE  _Allocate_PowerM_Object (VOID)
{
    REGISTER PLW_LIST_MONO          pmonoFree;
    REGISTER PLW_CLASS_POWERM_NODE  p_pmnTemp;
    
    if (_LIST_MONO_IS_EMPTY(_G_resrcPower.RESRC_pmonoFreeHeader)) {
        return  (LW_NULL);
    }
    
    pmonoFree = _list_mono_allocate_seq(&_G_resrcPower.RESRC_pmonoFreeHeader, 
                                        &_G_resrcPower.RESRC_pmonoFreeTail);
                                                                        /*  获得资源                    */
    p_pmnTemp = _LIST_ENTRY(pmonoFree, 
                            LW_CLASS_POWERM_NODE, 
                            PMN_monoResrcList);                         /*  获得控制块首地址            */
                            
    _G_resrcPower.RESRC_uiUsed++;
    if (_G_resrcPower.RESRC_uiUsed > _G_resrcPower.RESRC_uiMaxUsed) {
        _G_resrcPower.RESRC_uiMaxUsed = _G_resrcPower.RESRC_uiUsed;
    }
    
    return  (p_pmnTemp);
}
/*********************************************************************************************************
** 函数名称: _Free_PowerM_Object
** 功能描述: 将不再使用的功耗管理器放回空闲块池中
** 输　入  : p_pmnFree     不再使用的功耗管理器
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _Free_PowerM_Object (PLW_CLASS_POWERM_NODE    p_pmnFree)
{
    REGISTER PLW_LIST_MONO          pmonoFree;
    
    pmonoFree = &p_pmnFree->PMN_monoResrcList;
    
    _list_mono_free_seq(&_G_resrcPower.RESRC_pmonoFreeHeader, 
                        &_G_resrcPower.RESRC_pmonoFreeTail, 
                        pmonoFree);
                        
    _G_resrcPower.RESRC_uiUsed--;
}
#endif                                                                  /*  LW_CFG_POWERM_EN            */
                                                                        /*  LW_CFG_MAX_POWERM_NODES     */
/*********************************************************************************************************
  END
*********************************************************************************************************/

