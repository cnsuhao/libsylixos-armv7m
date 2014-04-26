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
** 文   件   名: HookList.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 04 月 20 日
**
** 描        述: 系统钩子函数函数, 

** BUG
2007.08.22  API_SystemHookAdd    出错误时，没有释放掉内存。
2007.08.22  API_SystemHookDelete 在操作关键性链表没有关闭中断。
2007.09.21  加入 _DebugHandle() 功能。
2007.11.13  使用链表库对链表操作进行完全封装.
2007.11.18  整理注释.
2008.03.02  加入系统重新启动回调.
2008.03.10  加入安全处理机制.
2008.05.18  使用 __KERNEL_ENTER() 代替 ThreadLock(); 
2009.12.09  修改注释.
2010.08.03  每个回调控制块使用独立的 spinlock.
2012.09.22  加入诸多电源管理的 HOOK.
2012.09.23  初始化时并不设置系统回调, 而是当用户第一次调用 hook add 操作时再安装.
2012.12.08  加入资源回收的功能.
2013.03.16  加入进程回调.
2013.05.02  这里已经加入资源管理, 进程允许安装回调.
*********************************************************************************************************/
/*********************************************************************************************************
注意：
      用户最好不要使用内核提供的 hook 功能，内核的 hook 功能是为系统级 hook 服务的，系统的 hook 有动态链表
      的功能，一个系统 hook 功能可以添加多个函数
      
      API_SystemHookDelete() 调用的时机非常重要，不能在 hook 扫描链扫描时调用，可能会发生扫描链断裂的情况
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  进程相关
*********************************************************************************************************/
#if LW_CFG_MODULELOADER_EN > 0
#include "../SylixOS/loader/include/loader_vppatch.h"
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
/*********************************************************************************************************
** 函数名称: API_SystemHookAdd
** 功能描述: 添加一个系统 hook 功能函数
** 输　入  : 
**           hookfuncPtr                   HOOK 功能函数
**           ulOpt                         HOOK 类型
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_SystemHookAdd (LW_HOOK_FUNC  hookfuncPtr, ULONG  ulOpt)
{
             INTREG           iregInterLevel;
             PLW_FUNC_NODE    pfuncnode;
    REGISTER PLW_LIST_LINE    pline;
    
    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (ERROR_KERNEL_IN_ISR);
    }
             
#if LW_CFG_ARG_CHK_EN > 0
    if (!hookfuncPtr) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "hookfuncPtr invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HOOK_NULL);
        return  (ERROR_KERNEL_HOOK_NULL);
    }
#endif
    
    pfuncnode = (PLW_FUNC_NODE)__SHEAP_ALLOC(sizeof(LW_FUNC_NODE));     /*  申请控制块内存              */
    if (!pfuncnode) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);                          /*  缺少内存                    */
        return  (ERROR_SYSTEM_LOW_MEMORY);
    }
    
    pline = &pfuncnode->FUNCNODE_lineManage;                            /*  管理线链表指针              */
    pfuncnode->FUNCNODE_hookfuncPtr = hookfuncPtr;
    
    _ErrorHandle(ERROR_NONE);
    
    switch (ulOpt) {
    
    case LW_OPTION_THREAD_CREATE_HOOK:                                  /*  线程建立钩子                */
        LW_SPIN_LOCK_QUICK(&_G_hookcbCreate.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbCreate.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_ThreadCreate = _SysCreateHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbCreate.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbCreate.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_THREAD_DELETE_HOOK:                                  /*  线程删除钩子                */
        LW_SPIN_LOCK_QUICK(&_G_hookcbDelete.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbDelete.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_ThreadDelete = _SysDeleteHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbDelete.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbDelete.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_THREAD_SWAP_HOOK:                                    /*  线程切换钩子                */
        LW_SPIN_LOCK_QUICK(&_G_hookcbSwap.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbSwap.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_ThreadSwap = _SysSwapHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbSwap.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbSwap.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_THREAD_TICK_HOOK:                                    /*  系统时钟中断钩子            */
        LW_SPIN_LOCK_QUICK(&_G_hookcbTick.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbTick.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_ThreadTick = _SysTickHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbTick.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbTick.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_THREAD_INIT_HOOK:                                    /*  线程初始化钩子              */
        LW_SPIN_LOCK_QUICK(&_G_hookcbInit.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbInit.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_ThreadInit = _SysInitHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbInit.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbInit.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_THREAD_IDLE_HOOK:                                    /*  空闲线程钩子                */
        if (API_KernelIsRunning()) {
            __SHEAP_FREE(pfuncnode);                                    /*  释放内存                    */
            _DebugHandle(__ERRORMESSAGE_LEVEL, "can not add idle hook in running status.\r\n");
            _ErrorHandle(ERROR_KERNEL_RUNNING);
            return  (ERROR_KERNEL_RUNNING);
        }
        if (_G_hookcbIdle.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_ThreadIdle = _SysIdleHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbIdle.HOOKCB_plineHookHeader);
        break;
        
    case LW_OPTION_KERNEL_INITBEGIN:                                    /*  内核初始化开始钩子          */
        LW_SPIN_LOCK_QUICK(&_G_hookcbInitBegin.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbInitBegin.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_KernelInitBegin = _SysInitBeginHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbInitBegin.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbInitBegin.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_KERNEL_INITEND:                                      /*  内核初始化结束钩子          */
        LW_SPIN_LOCK_QUICK(&_G_hookcbInitEnd.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbInitEnd.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_KernelInitEnd = _SysInitEndHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbInitEnd.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbInitEnd.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_KERNEL_REBOOT:                                       /*  内核重新启动                */
        LW_SPIN_LOCK_QUICK(&_G_hookcbReboot.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbReboot.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_KernelReboot = _SysRebootHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbReboot.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbReboot.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_WATCHDOG_TIMER:                                      /*  看门狗定时器钩子            */
        LW_SPIN_LOCK_QUICK(&_G_hookcbWatchDog.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbWatchDog.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_WatchDogTimer = _SysWatchDogHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbWatchDog.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbWatchDog.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_OBJECT_CREATE_HOOK:                                  /*  创建内核对象钩子            */
        LW_SPIN_LOCK_QUICK(&_G_hookcbObjectCreate.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbObjectCreate.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_ObjectCreate = _SysObjectCreateHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbObjectCreate.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbObjectCreate.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_OBJECT_DELETE_HOOK:                                  /*  删除内核对象钩子            */
        LW_SPIN_LOCK_QUICK(&_G_hookcbObjectDelete.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbObjectDelete.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_ObjectDelete = _SysObjectDeleteHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbObjectDelete.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbObjectDelete.HOOKCB_slHook, iregInterLevel);
        break;
    
    case LW_OPTION_FD_CREATE_HOOK:                                      /*  文件描述符创建钩子          */
        LW_SPIN_LOCK_QUICK(&_G_hookcbFdCreate.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbFdCreate.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_FdCreate = _SysFdCreateHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbFdCreate.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbFdCreate.HOOKCB_slHook, iregInterLevel);
        break;
    
    case LW_OPTION_FD_DELETE_HOOK:                                      /*  文件描述符删除钩子          */
        LW_SPIN_LOCK_QUICK(&_G_hookcbFdDelete.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbFdDelete.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_FdDelete = _SysFdDeleteHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbFdDelete.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbFdDelete.HOOKCB_slHook, iregInterLevel);
        break;
    
    case LW_OPTION_CPU_IDLE_ENTER:                                      /*  CPU 进入空闲模式            */
        LW_SPIN_LOCK_QUICK(&_G_hookcbCpuIdleEnter.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbCpuIdleEnter.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_CpuIdleEnter = _SysCpuIdleEnterHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbCpuIdleEnter.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbCpuIdleEnter.HOOKCB_slHook, iregInterLevel);
        break;
    
    case LW_OPTION_CPU_IDLE_EXIT:                                       /*  CPU 退出空闲模式            */
        LW_SPIN_LOCK_QUICK(&_G_hookcbCpuIdleExit.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbCpuIdleExit.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_CpuIdleExit = _SysCpuIdleExitHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbCpuIdleExit.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbCpuIdleExit.HOOKCB_slHook, iregInterLevel);
        break;
    
    case LW_OPTION_CPU_INT_ENTER:                                       /*  CPU 进入中断(异常)模式      */
        LW_SPIN_LOCK_QUICK(&_G_hookcbCpuIntEnter.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbCpuIntEnter.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_CpuIntEnter = _SysIntEnterHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbCpuIntEnter.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbCpuIntEnter.HOOKCB_slHook, iregInterLevel);
        break;
    
    case LW_OPTION_CPU_INT_EXIT:                                        /*  CPU 退出中断(异常)模式      */
        LW_SPIN_LOCK_QUICK(&_G_hookcbCpuIntExit.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbCpuIntExit.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_CpuIntExit = _SysIntExitHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbCpuIntExit.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbCpuIntExit.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_VPROC_CREATE_HOOK:                                   /*  进程建立钩子                */
        LW_SPIN_LOCK_QUICK(&_G_hookcbVpCreate.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbVpCreate.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_VpCreate = _SysVpCreateHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbVpCreate.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbVpCreate.HOOKCB_slHook, iregInterLevel);
        break;
        
    case LW_OPTION_VPROC_DELETE_HOOK:                                   /*  进程删除钩子                */
        LW_SPIN_LOCK_QUICK(&_G_hookcbVpDelete.HOOKCB_slHook, &iregInterLevel);
        if (_G_hookcbVpDelete.HOOKCB_plineHookHeader == LW_NULL) {
            _K_hookKernel.HOOK_VpDelete = _SysVpDeleteHook;
        }
        _List_Line_Add_Ahead(pline, &_G_hookcbVpDelete.HOOKCB_plineHookHeader);
        LW_SPIN_UNLOCK_QUICK(&_G_hookcbVpDelete.HOOKCB_slHook, iregInterLevel);
        break;
    
    default:
        __SHEAP_FREE(pfuncnode);                                        /*  释放内存                    */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "option invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_OPT_NULL);
        return  (ERROR_KERNEL_OPT_NULL);
    }
    
#if LW_CFG_MODULELOADER_EN > 0
    if (__PROC_GET_PID_CUR() && vprocFindProc((PVOID)hookfuncPtr)) {
        __resAddRawHook(&pfuncnode->FUNCNODE_resraw, (VOIDFUNCPTR)API_SystemHookDelete, 
                        (PVOID)pfuncnode->FUNCNODE_hookfuncPtr, (PVOID)ulOpt, 0, 0, 0, 0);
    } else {
        pfuncnode->FUNCNODE_resraw.RESRAW_bIsInstall = LW_FALSE;        /*  不需要回收操作             */
    }
#else
    __resAddRawHook(&pfuncnode->FUNCNODE_resraw, (VOIDFUNCPTR)API_SystemHookDelete, 
                    (PVOID)pfuncnode->FUNCNODE_hookfuncPtr, (PVOID)ulOpt, 0, 0, 0, 0);
#endif
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_SystemHookDelete
** 功能描述: 删除一个系统 hook 功能函数
** 输　入  : 
**           hookfuncPtr                   HOOK 功能函数
**           ulOpt                         HOOK 类型
** 输　出  : 
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_SystemHookDelete (LW_HOOK_FUNC  hookfuncPtr, ULONG  ulOpt)
{
             INTREG                 iregInterLevel;
             PLW_HOOK_CB            phookcb;
             
             PLW_FUNC_NODE          pfuncnode;
    REGISTER PLW_LIST_LINE          plinePtr;
    
    if (LW_CPU_GET_CUR_NESTING()) {                                     /*  不能在中断中调用            */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return  (ERROR_KERNEL_IN_ISR);
    }
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!hookfuncPtr) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "hookfuncPtr invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HOOK_NULL);
        return  (ERROR_KERNEL_HOOK_NULL);
    }
#endif

    _ErrorHandle(ERROR_NONE);
    
    switch (ulOpt) {
    
    case LW_OPTION_THREAD_CREATE_HOOK:                                  /*  线程建立钩子                */
        phookcb = &_G_hookcbCreate;
        break;
        
    case LW_OPTION_THREAD_DELETE_HOOK:                                  /*  线程删除钩子                */
        phookcb = &_G_hookcbDelete;
        break;
        
    case LW_OPTION_THREAD_SWAP_HOOK:                                    /*  线程切换钩子                */
        phookcb = &_G_hookcbSwap;
        break;
        
    case LW_OPTION_THREAD_TICK_HOOK:                                    /*  系统时钟中断钩子            */
        phookcb = &_G_hookcbTick;
        break;
        
    case LW_OPTION_THREAD_INIT_HOOK:                                    /*  线程初始化钩子              */
        phookcb = &_G_hookcbInit;
        break;
        
    case LW_OPTION_THREAD_IDLE_HOOK:                                    /*  空闲线程钩子                */
        phookcb = &_G_hookcbIdle;
        break;
        
    case LW_OPTION_KERNEL_INITBEGIN:                                    /*  内核初始化开始钩子          */
        phookcb = &_G_hookcbInitBegin;
        break;
        
    case LW_OPTION_KERNEL_INITEND:                                      /*  内核初始化结束钩子          */
        phookcb = &_G_hookcbInitEnd;
        break;
        
    case LW_OPTION_KERNEL_REBOOT:                                       /*  内核重新启动                */
        phookcb = &_G_hookcbReboot;
        break;
        
    case LW_OPTION_WATCHDOG_TIMER:                                      /*  看门狗定时器钩子            */
        phookcb = &_G_hookcbWatchDog;
        break;
        
    case LW_OPTION_OBJECT_CREATE_HOOK:                                  /*  创建内核对象钩子            */
        phookcb = &_G_hookcbObjectCreate;
        break;
    
    case LW_OPTION_OBJECT_DELETE_HOOK:                                  /*  删除内核对象钩子            */
        phookcb = &_G_hookcbObjectDelete;
        break;
    
    case LW_OPTION_FD_CREATE_HOOK:                                      /*  文件描述符创建钩子          */
        phookcb = &_G_hookcbFdCreate;
        break;
    
    case LW_OPTION_FD_DELETE_HOOK:                                      /*  文件描述符删除钩子          */
        phookcb = &_G_hookcbFdDelete;
        break;
        
    case LW_OPTION_CPU_IDLE_ENTER:                                      /*  CPU 进入空闲模式            */
        phookcb = &_G_hookcbCpuIdleEnter;
        break;
    
    case LW_OPTION_CPU_IDLE_EXIT:                                       /*  CPU 退出空闲模式            */
        phookcb = &_G_hookcbCpuIdleExit;
        break;
    
    case LW_OPTION_CPU_INT_ENTER:                                       /*  CPU 进入中断(异常)模式      */
        phookcb = &_G_hookcbCpuIntEnter;
        break;
    
    case LW_OPTION_CPU_INT_EXIT:                                        /*  CPU 退出中断(异常)模式      */
        phookcb = &_G_hookcbCpuIntExit;
        break;
        
    case LW_OPTION_VPROC_CREATE_HOOK:                                   /*  进程建立钩子                */
        phookcb = &_G_hookcbVpCreate;
        break;
        
    case LW_OPTION_VPROC_DELETE_HOOK:                                   /*  进程删除钩子                */
        phookcb = &_G_hookcbVpDelete;
        break;
    
    default:
        _DebugHandle(__ERRORMESSAGE_LEVEL, "option invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_OPT_NULL);
        return  (ERROR_KERNEL_OPT_NULL);
    }
    
    LW_SPIN_LOCK(&phookcb->HOOKCB_slHook);
    __KERNEL_ENTER();
    for (plinePtr  = phookcb->HOOKCB_plineHookHeader;
         plinePtr != LW_NULL;
         plinePtr  = _list_line_get_next(plinePtr)) {                   /*  开始查询                    */
         
        pfuncnode = (PLW_FUNC_NODE)plinePtr;
         
        if (pfuncnode->FUNCNODE_hookfuncPtr == hookfuncPtr) {
            
            iregInterLevel = KN_INT_DISABLE();                          /*  关闭中断                    */
            if (plinePtr == phookcb->HOOKCB_plineHookOp) {              /*  是否为当前操作的指针        */
                phookcb->HOOKCB_plineHookOp = _list_line_get_next(plinePtr);
            }
            _List_Line_Del(plinePtr, &phookcb->HOOKCB_plineHookHeader);
            KN_INT_ENABLE(iregInterLevel);                              /*  打开中断                    */
             
            LW_SPIN_UNLOCK(&phookcb->HOOKCB_slHook);
            __KERNEL_EXIT();
             
            __resDelRawHook(&pfuncnode->FUNCNODE_resraw);
            
            __SHEAP_FREE(pfuncnode);
            return  (ERROR_NONE);
        }
    }
    LW_SPIN_UNLOCK(&phookcb->HOOKCB_slHook);
    __KERNEL_EXIT();

    return  (ERROR_SYSTEM_HOOK_NULL);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
