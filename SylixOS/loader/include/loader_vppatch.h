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
** 文   件   名: loader_vppatch.h
**
** 创   建   人: Han.hui (韩辉)
**
** 文件创建日期: 2010 年 12 月 08 日
**
** 描        述: 进程库
*********************************************************************************************************/

#ifndef __LOADER_VPPATCH_H
#define __LOADER_VPPATCH_H

#include "SylixOS.h"
#include "loader_lib.h"

/*********************************************************************************************************
  vprocess 控制块操作
*********************************************************************************************************/

#define __LW_VP_GET_TCB_PROC(ptcb)      ((LW_LD_VPROC *)(ptcb->TCB_pvVProcessContext))
#define __LW_VP_GET_CUR_PROC()          ((LW_LD_VPROC *)(API_ThreadTcbSelf()->TCB_pvVProcessContext))
#define __LW_VP_SET_CUR_PROC(pvproc)    (API_ThreadTcbSelf()->TCB_pvVProcessContext = (PVOID)(pvproc))

/*********************************************************************************************************
  vprocess pid
*********************************************************************************************************/

static LW_INLINE pid_t __lw_vp_get_tcb_pid (PLW_CLASS_TCB ptcb)
{
    LW_LD_VPROC  *pvproc = __LW_VP_GET_TCB_PROC(ptcb);
    
    if (pvproc) {
        return  (pvproc->VP_pid);
    } else {
        return  (0);
    }
}

/*********************************************************************************************************
  vprocess 内部操作
*********************************************************************************************************/

VOID                vprocThreadExitHook(PVOID  pvVProc, LW_OBJECT_HANDLE  ulId);
INT                 vprocSetGroup(pid_t  pid, pid_t  pgid);
pid_t               vprocGetGroup(pid_t  pid);
pid_t               vprocGetFather(pid_t  pid);
INT                 vprocDetach(pid_t  pid);
LW_LD_VPROC        *vprocCreate(CPCHAR  pcFile);
INT                 vprocDestroy(LW_LD_VPROC *pvproc);                  /*  没有调用 vprocRun 才可调用  */
LW_LD_VPROC        *vprocGet(pid_t  pid);
pid_t               vprocGetPidByTcb(PLW_CLASS_TCB ptcb);
pid_t               vprocGetPidByTcbdesc(PLW_CLASS_TCB_DESC  ptcbdesc);
LW_OBJECT_HANDLE    vprocMainThread(pid_t pid);
INT                 vprocNotifyParent(LW_LD_VPROC *pvproc, INT  iSigCode);
VOID                vprocReclaim(LW_LD_VPROC *pvproc, BOOL  bFreeVproc);
VOID                vprocExit(LW_LD_VPROC *pvproc, LW_OBJECT_HANDLE  ulId, INT  iCode);
VOID                vprocExitNotDestroy(LW_LD_VPROC *pvproc);
INT                 vprocRun(LW_LD_VPROC      *pvproc, 
                             LW_LD_VPROC_STOP *pvpstop,
                             CPCHAR            pcFile, 
                             CPCHAR            pcEntry, 
                             INT              *piRet,
                             INT               iArgC, 
                             CPCHAR            ppcArgV[],
                             CPCHAR            ppcEnv[]);
VOID                vprocTickHook(PLW_CLASS_TCB  ptcb, PLW_CLASS_CPU  pcpu);
PLW_IO_ENV          vprocIoEnvGet(PLW_CLASS_TCB  ptcb);
FUNCPTR             vprocGetMain(VOID);
pid_t               vprocFindProc(PVOID  pvAddr);
INT                 vprocGetPath(pid_t  pid, PCHAR  pcPath, size_t stMaxLen);
VOID                vprocThreadAdd(PVOID   pvVProc, PLW_CLASS_TCB  ptcb);
VOID                vprocThreadDel(PVOID   pvVProc, PLW_CLASS_TCB  ptcb);
VOID                vprocThreadTraversal(PVOID          pvVProc, 
                                         VOIDFUNCPTR    pfunc, 
                                         PVOID          pvArg0,
                                         PVOID          pvArg1,
                                         PVOID          pvArg2,
                                         PVOID          pvArg3,
                                         PVOID          pvArg4,
                                         PVOID          pvArg5);
                                         
#if LW_CFG_GDB_EN > 0
ssize_t             vprocGetModsInfo(pid_t  pid, PCHAR  pcBuff, size_t stMaxLen);
#endif                                                                  /*  LW_CFG_GDB_EN > 0           */

/*********************************************************************************************************
  vprocess 文件描述符操作
*********************************************************************************************************/

VOID                vprocIoFileInit(LW_LD_VPROC *pvproc);               /*  vprocCreate  内被调用       */
VOID                vprocIoFileDeinit(LW_LD_VPROC *pvproc);             /*  vprocDestroy 内被调用       */
PLW_FD_ENTRY        vprocIoFileGet(INT  iFd, BOOL  bIsIgnAbn);
PLW_FD_ENTRY        vprocIoFileGetEx(LW_LD_VPROC *pvproc, INT  iFd, BOOL  bIsIgnAbn);
PLW_FD_DESC         vprocIoFileDescGet(INT  iFd, BOOL  bIsIgnAbn);
INT                 vprocIoFileDup(PLW_FD_ENTRY pfdentry, INT  iMinFd);
INT                 vprocIoFileDup2(PLW_FD_ENTRY pfdentry, INT  iNewFd);
INT                 vprocIoFileRefInc(INT  iFd);
INT                 vprocIoFileRefDec(INT  iFd);
INT                 vprocIoFileRefGet(INT  iFd);

/*********************************************************************************************************
  文件描述符传递操作
*********************************************************************************************************/

INT                 vprocIoFileDupFrom(pid_t  pidSrc, INT  iFd);
INT                 vprocIoFileRefIncByPid(pid_t  pid, INT  iFd);
INT                 vprocIoFileRefDecByPid(pid_t  pid, INT  iFd);

/*********************************************************************************************************
  资源管理器调用以下函数
*********************************************************************************************************/

VOID                vprocIoReclaim(pid_t  pid, BOOL  bIsExec);

#endif                                                                  /*  __LOADER_SYMBOL_H           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
