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
** 文   件   名: sysHookListLib.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 04 月 01 日
**
** 描        述: 系统钩子函数链内部库

** BUG
2007.11.07  _SysCreateHook() 加入建立时的 option 选项.
2007.11.13  使用链表库对链表操作进行完全封装.
2007.11.18  整理注释.
2008.03.02  加入系统重新启动回调.
2008.03.10  使用安全机制的回调链操作.
2009.04.09  修改回调参数.
2010.08.03  每个回调控制块使用独立的 spinlock.
2012.09.23  初始化时并不设置系统回调, 而是当用户第一次调用 hook add 操作时再安装.
2013.03.16  加入进程创建和删除回调.
2013.12.12  中断 hook 加入向量与嵌套层数参数.
2014.01.07  修正部分 hook 函数参数.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#define  __SYSHOOKLIST_MAIN_FILE
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  hook 服务函数模板
*********************************************************************************************************/
#define __HOOK_TEMPLATE(hookcb, param)  \
        INTREG           iregInterLevel;    \
        PLW_FUNC_NODE    pfuncnode; \
            \
        LW_SPIN_LOCK_IRQ(&hookcb.HOOKCB_slHook, &iregInterLevel);   \
        hookcb.HOOKCB_plineHookOp = hookcb.HOOKCB_plineHookHeader;  \
        while (hookcb.HOOKCB_plineHookOp) { \
            \
            pfuncnode = (PLW_FUNC_NODE)hookcb.HOOKCB_plineHookOp;    \
            hookcb.HOOKCB_plineHookOp =  \
                _list_line_get_next(hookcb.HOOKCB_plineHookOp);  \
                \
            KN_INT_ENABLE(iregInterLevel);  \
            pfuncnode->FUNCNODE_hookfuncPtr param;  \
            iregInterLevel = KN_INT_DISABLE();  \
        }   \
        LW_SPIN_UNLOCK_IRQ(&hookcb.HOOKCB_slHook, iregInterLevel);
/*********************************************************************************************************
** 函数名称: _HookCbInit
** 功能描述: 初始化钩子控制块
** 输　入  : phookcb       hook 控制块
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID _HookCbInit (PLW_HOOK_CB   phookcb)
{
    phookcb->HOOKCB_plineHookHeader = LW_NULL;
    phookcb->HOOKCB_plineHookOp     = LW_NULL;
    LW_SPIN_INIT(&phookcb->HOOKCB_slHook);
}
/*********************************************************************************************************
** 函数名称: _HookListInit
** 功能描述: 初始化钩子函数链
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID _HookListInit (VOID)
{
    _HookCbInit(&_G_hookcbCreate);
    _HookCbInit(&_G_hookcbDelete);
    _HookCbInit(&_G_hookcbSwap);
    _HookCbInit(&_G_hookcbTick);
    _HookCbInit(&_G_hookcbInit);
    _HookCbInit(&_G_hookcbIdle);
    _HookCbInit(&_G_hookcbInitBegin);
    _HookCbInit(&_G_hookcbInitEnd);
    _HookCbInit(&_G_hookcbReboot);
    _HookCbInit(&_G_hookcbWatchDog);
    
    _HookCbInit(&_G_hookcbObjectCreate);
    _HookCbInit(&_G_hookcbObjectDelete);
    _HookCbInit(&_G_hookcbFdCreate);
    _HookCbInit(&_G_hookcbFdDelete);
    
    _HookCbInit(&_G_hookcbCpuIdleEnter);
    _HookCbInit(&_G_hookcbCpuIdleExit);
    _HookCbInit(&_G_hookcbCpuIntEnter);
    _HookCbInit(&_G_hookcbCpuIntExit);
    
    _HookCbInit(&_G_hookcbVpCreate);
    _HookCbInit(&_G_hookcbVpDelete);
}
/*********************************************************************************************************
** 函数名称: _SysCreateHook
** 功能描述: 线程建立钩子服务函数
** 输　入  : ulId                      线程 Id
             ulOption                  建立选项
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysCreateHook (LW_OBJECT_HANDLE  ulId, ULONG   ulOption)
{
    __HOOK_TEMPLATE(_G_hookcbCreate, (ulId, ulOption));
}
/*********************************************************************************************************
** 函数名称: _SysDeleteHook
** 功能描述: 线程删除钩子服务函数
** 输　入  : 
**           ulId                      线程 Id
**           pvReturnVal               线程返回值
**           ptcb                      线程 TCB
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysDeleteHook (LW_OBJECT_HANDLE  ulId, PVOID  pvReturnVal, PLW_CLASS_TCB  ptcb)
{
    __HOOK_TEMPLATE(_G_hookcbDelete, (ulId, pvReturnVal, ptcb));
}
/*********************************************************************************************************
** 函数名称: _SysSwapHook
** 功能描述: 线程删除钩子服务函数
** 输　入  : hOldThread        老线程
**           hNewThread        新线程
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysSwapHook (LW_OBJECT_HANDLE   hOldThread, LW_OBJECT_HANDLE   hNewThread)
{
    INTREG          iregInterLevel;
    PLW_FUNC_NODE   pfuncnode;
    
    LW_SPIN_LOCK_QUICK(&_G_hookcbSwap.HOOKCB_slHook, &iregInterLevel);
    _G_hookcbSwap.HOOKCB_plineHookOp = _G_hookcbSwap.HOOKCB_plineHookHeader;
    while (_G_hookcbSwap.HOOKCB_plineHookOp) {
        
        pfuncnode = (PLW_FUNC_NODE)_G_hookcbSwap.HOOKCB_plineHookOp;
        _G_hookcbSwap.HOOKCB_plineHookOp = 
            _list_line_get_next(_G_hookcbSwap.HOOKCB_plineHookOp);
        
        KN_INT_ENABLE(iregInterLevel);
        (pfuncnode->FUNCNODE_hookfuncPtr)(hOldThread, hNewThread);      /*  不允许改变任务状态          */
        iregInterLevel = KN_INT_DISABLE();
    }
    LW_SPIN_UNLOCK_QUICK(&_G_hookcbSwap.HOOKCB_slHook, iregInterLevel);
}
/*********************************************************************************************************
** 函数名称: _SysTickHook
** 功能描述: 时钟中断钩子服务函数
** 输　入  : i64Tick   当前 tick
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysTickHook (INT64   i64Tick)
{
    INTREG           iregInterLevel;
    PLW_FUNC_NODE    pfuncnode;
    
    LW_SPIN_LOCK_QUICK(&_G_hookcbTick.HOOKCB_slHook, &iregInterLevel);
    _G_hookcbTick.HOOKCB_plineHookOp = _G_hookcbTick.HOOKCB_plineHookHeader;
    while (_G_hookcbTick.HOOKCB_plineHookOp) {
        
        pfuncnode = (PLW_FUNC_NODE)_G_hookcbTick.HOOKCB_plineHookOp;
        _G_hookcbTick.HOOKCB_plineHookOp = 
            _list_line_get_next(_G_hookcbTick.HOOKCB_plineHookOp);
        
        KN_INT_ENABLE(iregInterLevel);
        (pfuncnode->FUNCNODE_hookfuncPtr)(i64Tick);                     /*  不允许改变任务状态          */
        iregInterLevel = KN_INT_DISABLE();
    }
    LW_SPIN_UNLOCK_QUICK(&_G_hookcbTick.HOOKCB_slHook, iregInterLevel);
}
/*********************************************************************************************************
** 函数名称: _SysInitHook
** 功能描述: 线程初始化钩子函数链
** 输　入  : ulId                      线程 Id
**           ptcb                      线程 TCB
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysInitHook (LW_OBJECT_HANDLE  ulId, PLW_CLASS_TCB  ptcb)
{
    __HOOK_TEMPLATE(_G_hookcbInit, (ulId, ptcb));
}
/*********************************************************************************************************
** 函数名称: _SysIdleHook
** 功能描述: 空闲线程钩子函数链
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysIdleHook (VOID)
{
    PLW_FUNC_NODE    pfuncnode;
    
    _G_hookcbIdle.HOOKCB_plineHookOp = _G_hookcbIdle.HOOKCB_plineHookHeader;
    while (_G_hookcbIdle.HOOKCB_plineHookOp) {
    
        pfuncnode = (PLW_FUNC_NODE)_G_hookcbIdle.HOOKCB_plineHookOp;
        _G_hookcbIdle.HOOKCB_plineHookOp = 
            _list_line_get_next(_G_hookcbIdle.HOOKCB_plineHookOp);
        
        (pfuncnode->FUNCNODE_hookfuncPtr)();
    }
}
/*********************************************************************************************************
** 函数名称: _SysInitBeginHook
** 功能描述: 系统初始化开始时钩子
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysInitBeginHook (VOID)
{
    __HOOK_TEMPLATE(_G_hookcbInitBegin, ());
}
/*********************************************************************************************************
** 函数名称: _SysInitEndHook
** 功能描述: 系统初始化结束时钩子
** 输　入  : iError                    操作系统初始化是否出现错误   0 无错误   1 错误
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysInitEndHook (INT  iError)
{
    __HOOK_TEMPLATE(_G_hookcbInitEnd, (iError));
}
/*********************************************************************************************************
** 函数名称: _SysRebootHook
** 功能描述: 系统重启钩子
** 输　入  : iRebootType                系统重新启动类型
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysRebootHook (INT  iRebootType)
{
    __HOOK_TEMPLATE(_G_hookcbReboot, (iRebootType));
}
/*********************************************************************************************************
** 函数名称: _SysWatchDogHook
** 功能描述: 线程看门狗钩子函数链
** 输　入  : ulId                      线程 Id
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysWatchDogHook (LW_OBJECT_HANDLE  ulId)
{
    __HOOK_TEMPLATE(_G_hookcbWatchDog, (ulId));
}
/*********************************************************************************************************
** 函数名称: _SysObjectCreateHook
** 功能描述: 创建内核对象钩子
** 输　入  : ulId                      线程 Id
**           ulOption                  创建选项
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysObjectCreateHook (LW_OBJECT_HANDLE  ulId, ULONG  ulOption)
{
    __HOOK_TEMPLATE(_G_hookcbObjectCreate, (ulId, ulOption));
}
/*********************************************************************************************************
** 函数名称: _SysObjectDeleteHook
** 功能描述: 删除内核对象钩子
** 输　入  : ulId                      线程 Id
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysObjectDeleteHook (LW_OBJECT_HANDLE  ulId)
{
    __HOOK_TEMPLATE(_G_hookcbObjectDelete, (ulId));
}
/*********************************************************************************************************
** 函数名称: _SysFdCreateHook
** 功能描述: 文件描述符创建钩子
** 输　入  : iFd                       文件描述符
**           pid                       进程id
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysFdCreateHook (INT iFd, pid_t  pid)
{
    __HOOK_TEMPLATE(_G_hookcbFdCreate, (iFd, pid));
}
/*********************************************************************************************************
** 函数名称: _SysFdDeleteHook
** 功能描述: 文件描述符删除钩子
** 输　入  : iFd                       文件描述符
**           pid                       进程id
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysFdDeleteHook (INT iFd, pid_t  pid)
{
    __HOOK_TEMPLATE(_G_hookcbFdDelete, (iFd, pid));
}
/*********************************************************************************************************
** 函数名称: _SysCpuIdleEnterHook
** 功能描述: CPU 进入空闲模式
** 输　入  : ulIdEnterFrom             从哪个线程进入 idle
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysCpuIdleEnterHook (LW_OBJECT_HANDLE  ulIdEnterFrom)
{
    __HOOK_TEMPLATE(_G_hookcbCpuIdleEnter, (ulIdEnterFrom));
}
/*********************************************************************************************************
** 函数名称: _SysCpuIdleExitHook
** 功能描述: CPU 退出空闲模式
** 输　入  : ulIdExitTo                退出 idle 线程进入哪个线程
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysCpuIdleExitHook (LW_OBJECT_HANDLE  ulIdExitTo)
{
    __HOOK_TEMPLATE(_G_hookcbCpuIdleExit, (ulIdExitTo));
}
/*********************************************************************************************************
** 函数名称: _SysIntEnterHook
** 功能描述: CPU 进入中断(异常)模式
** 输　入  : ulVector      中断向量
**           ulNesting     当前嵌套层数
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysIntEnterHook (ULONG  ulVector, ULONG  ulNesting)
{
    __HOOK_TEMPLATE(_G_hookcbCpuIntEnter, (ulVector, ulNesting));
}
/*********************************************************************************************************
** 函数名称: _SysIntExitHook
** 功能描述: CPU 退出中断(异常)模式
** 输　入  : ulVector      中断向量
**           ulNesting     当前嵌套层数
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysIntExitHook (ULONG  ulVector, ULONG  ulNesting)
{
    __HOOK_TEMPLATE(_G_hookcbCpuIntExit, (ulVector, ulNesting));
}
/*********************************************************************************************************
** 函数名称: _SysVpCreateHook
** 功能描述: 进程建立钩子服务函数
** 输　入  : pid                       进程 id
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysVpCreateHook (pid_t pid)
{
    __HOOK_TEMPLATE(_G_hookcbVpCreate, (pid));
}
/*********************************************************************************************************
** 函数名称: _SysVpDeleteHook
** 功能描述: 进程删除钩子服务函数
** 输　入  : pid                       进程 id
**           iExitCode                 进程返回值
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SysVpDeleteHook (pid_t pid, INT iExitCode)
{
    __HOOK_TEMPLATE(_G_hookcbVpDelete, (pid, iExitCode));
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
