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
** 文   件   名: dtrace.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2011 年 11 月 18 日
**
** 描        述: SylixOS 调试跟踪器, GDB server 可以使用此调试跟踪器调试装载的模块或者进程.
*********************************************************************************************************/

#ifndef __DTRACE_H
#define __DTRACE_H

/*********************************************************************************************************
  API
*********************************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif                                                                  /*  __cplusplus                 */

LW_API PVOID            dtrace_create(BOOL (*pfuncTrap)(), PVOID pvArg);
LW_API INT              dtrace_delete(PVOID pvDtrace);

LW_API INT              dtrace_read(PVOID pvDtrace, addr_t ulAddress, PVOID pvBuffer, size_t  stNbytes);
LW_API INT              dtrace_write(PVOID pvDtrace, addr_t ulAddress, 
                                     const PVOID pvBuffer, size_t  stNbytes);
                                     
LW_API INT              dtrace_continue(PVOID pvDtrace);
LW_API INT              dtrace_continue_one(PVOID pvDtrace);

/*********************************************************************************************************
  API (SylixOS internal use only!)
*********************************************************************************************************/

LW_API INT              dtrace_trap(PVOID              pvFrame,
                                    addr_t             ulAddress, 
                                    ULONG              ulType,
                                    LW_OBJECT_HANDLE   ulThread);
                                    
#ifdef __cplusplus
}
#endif                                                                  /*  __cplusplus                 */

#endif                                                                  /*  __DTRACE_H                  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
