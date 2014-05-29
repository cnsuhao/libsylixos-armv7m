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
** 文   件   名: powerM.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 03 月 06 日
**
** 描        述: 系统级功耗管理器. 根据用户配置, 可分为多个功耗控制点, 每个控制点可以设置延迟时间.
                 例如, 系统中含有 LCD, 那么可以分为两个功耗控制点. 当经历一段时间无人操作后, 率先
                 关闭 LCD 背光, 在经历一段时间无人操作后, 可关闭 LCD 显示系统以降低功耗.
                 
** BUG
2008.05.18  使用 __KERNEL_ENTER() 代替 ThreadLock();
2008.05.31  使用 __KERNEL_MODE_...().
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁减控制
*********************************************************************************************************/
#if (LW_CFG_POWERM_EN > 0) && (LW_CFG_MAX_POWERM_NODES > 0)
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
extern LW_CLASS_POWERM_NODE _G__pmnControlNode[LW_CFG_MAX_POWERM_NODES];
extern LW_OBJECT_HANDLE     _G_ulPowerMLock;
extern LW_CLASS_WAKEUP      _G_wuPowerM;
/*********************************************************************************************************
  内部函数
*********************************************************************************************************/
PLW_CLASS_POWERM_NODE  _Allocate_PowerM_Object(VOID);
VOID                   _Free_PowerM_Object(PLW_CLASS_POWERM_NODE    p_pmnFree);
/*********************************************************************************************************
** 函数名称: API_PowerMCreate
** 功能描述: 创建一个功耗控制定时器
** 输　入  : 
**           pcName              功耗控制点的名字
**           ulOption            附加选项
**           pulId               Id指针
** 输　出  : 功耗控制定时器
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
LW_OBJECT_HANDLE  API_PowerMCreate (PCHAR            pcName,
                                    ULONG            ulOption,
                                    LW_OBJECT_ID    *pulId)
{
    REGISTER ULONG                  ulIdTemp;
    REGISTER PLW_CLASS_POWERM_NODE  p_pmnNewNode;
    
    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (LW_OBJECT_HANDLE_INVALID);
    }
    
    if (_Object_Name_Invalid(pcName)) {                                 /*  检查名字有效性              */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "name too long.\r\n");
        _ErrorHandle(ERROR_KERNEL_PNAME_TOO_LONG);
        return  (LW_OBJECT_HANDLE_INVALID);
    }
    
    __KERNEL_MODE_PROC(
        p_pmnNewNode = _Allocate_PowerM_Object();                       /*  获得一个控制块              */
    );
    
    if (!p_pmnNewNode) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "powerm node full.\r\n");
        _ErrorHandle(ERROR_POWERM_FULL);
        return  (LW_OBJECT_HANDLE_INVALID);
    }
    
    if (pcName) {                                                       /*  拷贝名字                    */
        lib_strcpy(p_pmnNewNode->PMN_cPowerMName, pcName);
    } else {
        p_pmnNewNode->PMN_cPowerMName[0] = PX_EOS;                      /*  清空名字                    */
    }
    
    p_pmnNewNode->PMN_ulCounter     = 0;
    p_pmnNewNode->PMN_ulCounterSave = 0;
    p_pmnNewNode->PMN_pvArg         = LW_NULL;
    p_pmnNewNode->PMN_pfuncCallback = LW_NULL;
    p_pmnNewNode->PMN_bIsUsing      = LW_TRUE;
    
    ulIdTemp = _MakeObjectId(_OBJECT_POWERM, 
                             LW_CFG_PROCESSOR_NUMBER, 
                             p_pmnNewNode->PMN_usIndex);                /*  构建对象 id                 */
    
    if (pulId) {
        *pulId = ulIdTemp;
    }
    
    __LW_OBJECT_CREATE_HOOK(ulIdTemp, ulOption);
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "powerm node \"");
    _DebugHandle(__LOGMESSAGE_LEVEL, (pcName ? pcName : ""));
    _DebugHandle(__LOGMESSAGE_LEVEL, "\" has been create.\r\n");
    
    return  (ulIdTemp);
}
/*********************************************************************************************************
** 函数名称: API_PowerMDelete
** 功能描述: 删除一个功耗控制定时器
** 输　入  : pulId               句柄指针
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
ULONG  API_PowerMDelete (LW_OBJECT_HANDLE   *pulId)
{
    REGISTER LW_OBJECT_HANDLE          ulId;
    REGISTER UINT16                    usIndex;
    REGISTER PLW_CLASS_POWERM_NODE     p_pmnDelNode;
    
    ulId = *pulId;
    
    usIndex = _ObjectGetIndex(ulId);
    
    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (ERROR_KERNEL_IN_ISR);
    }
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!_ObjectClassOK(ulId, _OBJECT_POWERM)) {                        /*  对象类型检查                */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    if (_PowerM_Index_Invalid(usIndex)) {                               /*  缓冲区索引检查              */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
#endif
    
    __POWERM_LOCK();                                                    /*  上锁                        */

    p_pmnDelNode = &_G__pmnControlNode[usIndex];
    
    if (!p_pmnDelNode->PMN_bIsUsing) {
        __POWERM_UNLOCK();                                              /*  解锁                        */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    
    if (p_pmnDelNode->PMN_bInQ) {                                       /*  已经加入了扫描链表          */
        _WakeupDel(&_G_wuPowerM, &p_pmnDelNode->PMN_wunTimer);
    }
    
    p_pmnDelNode->PMN_ulCounterSave = 0;
    p_pmnDelNode->PMN_bIsUsing      = LW_FALSE;
    
    _ObjectCloseId(pulId);
    
    __POWERM_UNLOCK();                                                  /*  解锁                        */
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "powerm node \"");
    _DebugHandle(__LOGMESSAGE_LEVEL, p_pmnDelNode->PMN_cPowerMName);
    _DebugHandle(__LOGMESSAGE_LEVEL, "\" has been delete.\r\n");
    
    __KERNEL_MODE_PROC(
        _Free_PowerM_Object(p_pmnDelNode);
    );
    
    __LW_OBJECT_DELETE_HOOK(ulId);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_PowerMStart
** 功能描述: 启动一个指定节点的功耗控制定时器
** 输　入  : ulId                功耗控制定时器句柄
**           ulMaxIdleTime       该控制点最长的空闲时间
**           pfuncCallback       空闲时间到时执行的函数
**           pvArg               回调函数参数
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
ULONG  API_PowerMStart (LW_OBJECT_HANDLE    ulId,
                        ULONG               ulMaxIdleTime,
                        LW_HOOK_FUNC        pfuncCallback,
                        PVOID               pvArg)
{
    REGISTER UINT16                    usIndex;
    REGISTER PLW_CLASS_POWERM_NODE     p_pmnNode;
    
    usIndex = _ObjectGetIndex(ulId);

    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (ERROR_KERNEL_IN_ISR);
    }
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!ulMaxIdleTime) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "ulMaxIdleTime invalidate.\r\n");
        _ErrorHandle(ERROR_POWERM_TIME);
        return  (ERROR_POWERM_TIME);
    }
    if (!pfuncCallback) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pfuncCallback invalidate.\r\n");
        _ErrorHandle(ERROR_POWERM_FUNCTION);
        return  (ERROR_POWERM_FUNCTION);
    }
    if (!_ObjectClassOK(ulId, _OBJECT_POWERM)) {                        /*  对象类型检查                */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    if (_PowerM_Index_Invalid(usIndex)) {                               /*  缓冲区索引检查              */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
#endif

    __POWERM_LOCK();                                                    /*  上锁                        */

    p_pmnNode = &_G__pmnControlNode[usIndex];
    
    if (!p_pmnNode->PMN_bIsUsing) {
        __POWERM_UNLOCK();                                              /*  解锁                        */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    
    if (p_pmnNode->PMN_bInQ) {                                          /*  已经加入了扫描链表          */
        _WakeupDel(&_G_wuPowerM, &p_pmnNode->PMN_wunTimer);
        p_pmnNode->PMN_ulCounterSave = ulMaxIdleTime;                   /*  重新赋值                    */
        p_pmnNode->PMN_pfuncCallback = pfuncCallback;
        p_pmnNode->PMN_pvArg         = pvArg;
        p_pmnNode->PMN_ulCounter     = ulMaxIdleTime;
    
    } else {
        p_pmnNode->PMN_ulCounterSave = ulMaxIdleTime;                   /*  重新赋值                    */
        p_pmnNode->PMN_pfuncCallback = pfuncCallback;
        p_pmnNode->PMN_pvArg         = pvArg;
        p_pmnNode->PMN_ulCounter     = ulMaxIdleTime;
    }
    _WakeupAdd(&_G_wuPowerM, &p_pmnNode->PMN_wunTimer);

    __POWERM_UNLOCK();                                                  /*  解锁                        */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_PowerMCancel
** 功能描述: 当指定功耗控制定时器停止
** 输　入  : ulId                功耗控制定时器句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
ULONG  API_PowerMCancel (LW_OBJECT_HANDLE    ulId)
{
    REGISTER UINT16                    usIndex;
    REGISTER PLW_CLASS_POWERM_NODE     p_pmnNode;
    
    usIndex = _ObjectGetIndex(ulId);

    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (ERROR_KERNEL_IN_ISR);
    }
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!_ObjectClassOK(ulId, _OBJECT_POWERM)) {                        /*  对象类型检查                */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    if (_PowerM_Index_Invalid(usIndex)) {                               /*  缓冲区索引检查              */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
#endif

    __POWERM_LOCK();                                                    /*  上锁                        */

    p_pmnNode = &_G__pmnControlNode[usIndex];
    
    if (!p_pmnNode->PMN_bIsUsing) {
        __POWERM_UNLOCK();                                              /*  解锁                        */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    
    if (p_pmnNode->PMN_bInQ) {                                          /*  已经加入了扫描链表          */
        p_pmnNode->PMN_ulCounterSave = 0;
        p_pmnNode->PMN_ulCounter     = 0;
        _WakeupDel(&_G_wuPowerM, &p_pmnNode->PMN_wunTimer);
    }
    
    __POWERM_UNLOCK();                                                  /*  解锁                        */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_PowerMConnect
** 功能描述: 指定功耗控制定时器回调函数和参数
** 输　入  : ulId                功耗控制定时器句柄
**           pfuncCallback       新回调函数
**           pvArg               回调函数参数
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_PowerMConnect (LW_OBJECT_HANDLE    ulId,
                          LW_HOOK_FUNC        pfuncCallback,
                          PVOID               pvArg)
{
    REGISTER UINT16                    usIndex;
    REGISTER PLW_CLASS_POWERM_NODE     p_pmnNode;
    
    usIndex = _ObjectGetIndex(ulId);

    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (ERROR_KERNEL_IN_ISR);
    }
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!pfuncCallback) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pfuncCallback invalidate.\r\n");
        _ErrorHandle(ERROR_POWERM_FUNCTION);
        return  (ERROR_POWERM_FUNCTION);
    }
    if (!_ObjectClassOK(ulId, _OBJECT_POWERM)) {                        /*  对象类型检查                */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    if (_PowerM_Index_Invalid(usIndex)) {                               /*  缓冲区索引检查              */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
#endif

    __POWERM_LOCK();                                                    /*  上锁                        */

    p_pmnNode = &_G__pmnControlNode[usIndex];
    
    if (!p_pmnNode->PMN_bIsUsing) {
        __POWERM_UNLOCK();                                              /*  解锁                        */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    
    p_pmnNode->PMN_pfuncCallback = pfuncCallback;
    p_pmnNode->PMN_pvArg         = pvArg;
    
    __POWERM_UNLOCK();                                                  /*  解锁                        */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_PowerMSignal
** 功能描述: 当指定功耗控制定时器正在计时时, 这个函数可以定时器复位, 重新计时.
** 输　入  : ulId                功耗控制定时器句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
** 注  意  : 此函数尽量不能再中断中调用, 有可能会导致 excJob 队列溢出, 造成操作丢失问题.
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_PowerMSignal (LW_OBJECT_HANDLE    ulId)
{
    REGISTER UINT16                    usIndex;
    REGISTER PLW_CLASS_POWERM_NODE     p_pmnNode;
    REGISTER INT                       iError;
    
    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  处于中断中                  */
        iError = _excJobAdd((VOIDFUNCPTR)API_PowerMSignal, 
                            (PVOID)ulId, 0, 0, 0,
                            0, 0);                                      /*  延迟处理                    */
        if (iError) {
            return  (ERROR_EXCE_LOST);
        } else {
            return  (ERROR_NONE);
        }
    }
    
    usIndex = _ObjectGetIndex(ulId);
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!_ObjectClassOK(ulId, _OBJECT_POWERM)) {                        /*  对象类型检查                */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    if (_PowerM_Index_Invalid(usIndex)) {                               /*  缓冲区索引检查              */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
#endif

    __POWERM_LOCK();                                                    /*  上锁                        */
    
    p_pmnNode = &_G__pmnControlNode[usIndex];
    
    if (!p_pmnNode->PMN_bIsUsing) {
        __POWERM_UNLOCK();                                              /*  解锁                        */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    
    if (p_pmnNode->PMN_bInQ) {                                          /*  已经加入了扫描链表          */
        _WakeupDel(&_G_wuPowerM, &p_pmnNode->PMN_wunTimer);
    }
        
    p_pmnNode->PMN_ulCounter = p_pmnNode->PMN_ulCounterSave;            /*  复位定时器                  */
   
    _WakeupAdd(&_G_wuPowerM, &p_pmnNode->PMN_wunTimer);
    
    __POWERM_UNLOCK();                                                  /*  解锁                        */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_PowerMSignalFast
** 功能描述: 当需要以极高的频率调用 API_PowerMSignal 函数时, 在确保句柄准确无误的情况下, 
             可以使用这个函数加快速度, 例如: TOUCH SCREEN 的扫描程序.
** 输　入  : ulId                功耗控制定时器句柄
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
** 注  意  : 此函数不能再中断中调用.
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG   API_PowerMSignalFast (LW_OBJECT_HANDLE    ulId)
{
    REGISTER UINT16                    usIndex;
    REGISTER PLW_CLASS_POWERM_NODE     p_pmnNode;
    
    usIndex = _ObjectGetIndex(ulId);
    
    __POWERM_LOCK();                                                    /*  上锁                        */
    
    p_pmnNode = &_G__pmnControlNode[usIndex];
    
    if (p_pmnNode->PMN_bInQ) {
        _WakeupDel(&_G_wuPowerM, &p_pmnNode->PMN_wunTimer);
    }
        
    p_pmnNode->PMN_ulCounter = p_pmnNode->PMN_ulCounterSave;            /*  复位定时器                  */
    
    _WakeupAdd(&_G_wuPowerM, &p_pmnNode->PMN_wunTimer);
    
    __POWERM_UNLOCK();                                                  /*  解锁                        */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_PowerMStatus
** 功能描述: 获得指定功耗控制定时器的工作状态
** 输　入  : ulId                功耗控制定时器句柄
**           pulCounter          当前计数值
**           pulMaxIdleTime      最大空闲时间
**           ppfuncCallback      回调函数
**           ppvArg              对调函数参数
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_PowerMStatus (LW_OBJECT_HANDLE    ulId,
                         ULONG              *pulCounter,
                         ULONG              *pulMaxIdleTime,
                         LW_HOOK_FUNC       *ppfuncCallback,
                         PVOID              *ppvArg)
{
    REGISTER UINT16                    usIndex;
    REGISTER PLW_CLASS_POWERM_NODE     p_pmnNode;
    
    usIndex = _ObjectGetIndex(ulId);

    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (ERROR_KERNEL_IN_ISR);
    }
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!_ObjectClassOK(ulId, _OBJECT_POWERM)) {                        /*  对象类型检查                */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    if (_PowerM_Index_Invalid(usIndex)) {                               /*  缓冲区索引检查              */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
#endif

    __POWERM_LOCK();                                                    /*  上锁                        */

    p_pmnNode = &_G__pmnControlNode[usIndex];
    
    if (!p_pmnNode->PMN_bIsUsing) {
        __POWERM_UNLOCK();                                              /*  解锁                        */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    
    if (pulCounter) {
        if (p_pmnNode->PMN_bInQ) {
            _WakeupStatus(&_G_wuPowerM, &p_pmnNode->PMN_wunTimer, pulCounter);
        } else {
            *pulCounter = 0ul;
        }
    }
    if (pulMaxIdleTime) {
        *pulMaxIdleTime = p_pmnNode->PMN_ulCounterSave;
    }
    if (ppfuncCallback) {
        *ppfuncCallback = p_pmnNode->PMN_pfuncCallback;
    }
    if (ppvArg) {
        *ppvArg = p_pmnNode->PMN_pvArg;
    }
    
    __POWERM_UNLOCK();                                                  /*  解锁                        */
    
    return  (ERROR_NONE);
}
#endif                                                                  /*  LW_CFG_POWERM_EN            */
                                                                        /*  LW_CFG_MAX_POWERM_NODES     */
/*********************************************************************************************************
  END
*********************************************************************************************************/
