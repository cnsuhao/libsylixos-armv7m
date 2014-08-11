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
** 文   件   名: sysHookList.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 04 月 01 日
**
** 描        述: 系统钩子函数链头文件
*********************************************************************************************************/
/*********************************************************************************************************
注意：
      用户最好不要使用内核提供的 hook 功能，内核的 hook 功能是为系统级 hook 服务的，系统的 hook 有动态链表
      的功能，一个系统 hook 功能可以添加多个函数
      
      API_SystemHookDelete() 调用的时机非常重要，不能在 hook 扫描链扫描时调用，可能会发生扫描链断裂的情况
*********************************************************************************************************/

#ifndef  __SYSHOOKLIST_H
#define  __SYSHOOKLIST_H

/*********************************************************************************************************
  FUNCTION NODE
*********************************************************************************************************/

typedef struct {
    LW_LIST_LINE              FUNCNODE_lineManage;                      /*  管理链表                    */
    LW_HOOK_FUNC              FUNCNODE_hookfuncPtr;                     /*  钩子函数指针                */
    LW_RESOURCE_RAW           FUNCNODE_resraw;                          /*  资源管理节点                */
} LW_FUNC_NODE;
typedef LW_FUNC_NODE         *PLW_FUNC_NODE;

/*********************************************************************************************************
  SYSTEM HOOK SERVICE ROUTINE
*********************************************************************************************************/

VOID  _SysCreateHook(LW_OBJECT_HANDLE  ulId, ULONG  ulOption);
VOID  _SysDeleteHook(LW_OBJECT_HANDLE  ulId, PVOID  pvReturnVal, PLW_CLASS_TCB  ptcb);
VOID  _SysSwapHook(LW_OBJECT_HANDLE   hOldThread, LW_OBJECT_HANDLE   hNewThread);
VOID  _SysTickHook(INT64   i64Tick);
VOID  _SysInitHook(LW_OBJECT_HANDLE  ulId, PLW_CLASS_TCB  ptcb);
VOID  _SysIdleHook(VOID);
VOID  _SysInitBeginHook(VOID);
VOID  _SysInitEndHook(INT  iError);
VOID  _SysRebootHook(INT  iRebootType);
VOID  _SysWatchDogHook(LW_OBJECT_HANDLE  ulId);

VOID  _SysObjectCreateHook(LW_OBJECT_HANDLE  ulId, ULONG  ulOption);
VOID  _SysObjectDeleteHook(LW_OBJECT_HANDLE  ulId);
VOID  _SysFdCreateHook(INT iFd, pid_t  pid);
VOID  _SysFdDeleteHook(INT iFd, pid_t  pid);

VOID  _SysCpuIdleEnterHook(LW_OBJECT_HANDLE  ulIdEnterFrom);
VOID  _SysCpuIdleExitHook(LW_OBJECT_HANDLE  ulIdExitTo);
VOID  _SysIntEnterHook(ULONG  ulVector, ULONG  ulNesting);
VOID  _SysIntExitHook(ULONG  ulVector, ULONG  ulNesting);

VOID  _SysStkOverflowHook(pid_t  pid, LW_OBJECT_HANDLE  ulId);
VOID  _SysFatalErrorHook(pid_t  pid, LW_OBJECT_HANDLE  ulId, struct siginfo *psiginfo);

VOID  _SysVpCreateHook(pid_t pid);
VOID  _SysVpDeleteHook(pid_t pid, INT iExitCode);

/*********************************************************************************************************
  GLOBAL VAR
*********************************************************************************************************/

#ifdef   __SYSHOOKLIST_MAIN_FILE
#define  __SYSHOOK_EXT
#else
#define  __SYSHOOK_EXT           extern
#endif

/*********************************************************************************************************
  SYSTEM HOOK NODE
*********************************************************************************************************/

typedef struct {
    PLW_LIST_LINE           HOOKCB_plineHookHeader;                     /*  钩子函数链                  */
    PLW_LIST_LINE           HOOKCB_plineHookOp;                         /*  当前操作函数链              */
    LW_SPINLOCK_DEFINE     (HOOKCB_slHook);                             /*  自旋锁                      */
} LW_HOOK_CB;
typedef LW_HOOK_CB         *PLW_HOOK_CB;

/*********************************************************************************************************
  HOOK CONTROL BLOCK
*********************************************************************************************************/

__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbCreate;                       /*  线程建立钩子                */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbDelete;                       /*  线程删除钩子                */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbSwap;                         /*  线程切换钩子                */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbTick;                         /*  时钟中断钩子                */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbInit;                         /*  线程初始化钩子              */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbIdle;                         /*  空闲线程钩子                */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbInitBegin;                    /*  系统初始化开始时钩子        */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbInitEnd;                      /*  系统初始化结束时钩子        */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbReboot;                       /*  系统重新启动钩子            */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbWatchDog;                     /*  线程看门狗钩子              */

__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbObjectCreate;                 /*  创建内核对象钩子            */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbObjectDelete;                 /*  删除内核对象钩子            */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbFdCreate;                     /*  文件描述符创建钩子          */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbFdDelete;                     /*  文件描述符删除钩子          */

__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbCpuIdleEnter;                 /*  CPU 进入空闲模式            */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbCpuIdleExit;                  /*  CPU 退出空闲模式            */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbCpuIntEnter;                  /*  CPU 进入中断(异常)模式      */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbCpuIntExit;                   /*  CPU 退出中断(异常)模式      */

__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbStkOverflow;                  /*  堆栈溢出                    */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbFatalError;                   /*  致命错误                    */

__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbVpCreate;                     /*  进程建立钩子                */
__SYSHOOK_EXT LW_HOOK_CB         _G_hookcbVpDelete;                     /*  进程删除钩子                */

#endif                                                                  /*  __SYSHOOKLIST_H             */
/*********************************************************************************************************
  END
*********************************************************************************************************/

