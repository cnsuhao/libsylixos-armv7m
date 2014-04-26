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
** 文   件   名: _ErrorLib.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 13 日
**
** 描        述: 这是系统错误相关.

** BUG:
2011.03.04  使用 __errno 作为函数名.
2012.03.20  减少对 _K_ptcbTCBCur 的引用, 尽量采用局部变量, 减少对当前 CPU ID 获取的次数.
2012.07.21  加入错误处理与显示内容函数, 不再使用宏.
2014.04.08  由于 _DebugHandle() 已经为函数, 丧失了获得调用者位置信息的能力, 这里不再打印错误位置.
2013.07.18  使用新的获取 TCB 的方法, 确保 SMP 系统安全.
2013.12.13  将 _DebugHandle() 改名为 _DebugMessage(), _DebugHandle() 转由宏实现.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  打印信息
*********************************************************************************************************/
#define __ERROR_THREAD_SHOW()   do {    \
            if (LW_CPU_GET_CUR_NESTING()) { \
                _K_pfuncKernelDebugError("in interrupt context.\r\n");  \
            } else {    \
                REGISTER PLW_CLASS_TCB   ptcb;  \
                LW_TCB_GET_CUR_SAFE(ptcb);  \
                if (ptcb && ptcb->TCB_cThreadName[0] != PX_EOS) { \
                    _K_pfuncKernelDebugError("in thread \"");   \
                    _K_pfuncKernelDebugError(ptcb->TCB_cThreadName);    \
                    _K_pfuncKernelDebugError("\" context.\r\n");    \
                }   \
            }   \
        } while (0)
/*********************************************************************************************************
** 函数名称: __errno
** 功能描述: posix 获得当前 errno
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
** 注  意  : 由于 longwing 由于历史原因采用 ulong 保存错误编号, 而 posix 使用 errno_t 类型, 而绝大多数系统
             将 errno_t 定义为 int 型, 假设使用 GCC 3.x 以上版本带有 -fstrict-aliasing 参数.这里的代码可能
             会产生一个警告: strict aliasing, 目前将此警告忽略处理.
*********************************************************************************************************/
errno_t *__errno (VOID)
{
    INTREG          iregInterLevel;
    PLW_CLASS_CPU   pcpuCur;
    PLW_CLASS_TCB   ptcbCur;
    errno_t        *perrno;
    
    iregInterLevel = KN_INT_DISABLE();                                  /*  关闭中断, 防止调度到其他 CPU*/
    
    pcpuCur = LW_CPU_GET_CUR();
    
    if (pcpuCur->CPU_ulInterNesting) {
        perrno = (errno_t *)(&pcpuCur->CPU_ulInterError[pcpuCur->CPU_ulInterNesting]);
    } else {
        ptcbCur = pcpuCur->CPU_ptcbTCBCur;
        if (ptcbCur) {
            perrno = (errno_t *)(&ptcbCur->TCB_ulLastError);
        } else {
            perrno = (errno_t *)(&_K_ulNotRunError);
        }
    }
    
    KN_INT_ENABLE(iregInterLevel);
    
    return  (perrno);
}
/*********************************************************************************************************
** 函数名称: _ErrorHandle
** 功能描述: 记录当前错误号
** 输　入  : ulErrorCode       当前错误号
** 输　出  : 
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
VOID  _ErrorHandle (ULONG  ulErrorCode)
{
    INTREG          iregInterLevel;
    PLW_CLASS_CPU   pcpuCur;
    PLW_CLASS_TCB   ptcbCur;
    
#if LW_CFG_ERRORNO_AUTO_CLEAR == 0
    if (ulErrorCode == 0) {
        return;
    }
#endif

    iregInterLevel = KN_INT_DISABLE();                                  /*  关闭中断, 防止调度到其他 CPU*/
    
    pcpuCur = LW_CPU_GET_CUR();
    
    if (pcpuCur->CPU_ulInterNesting) {
        pcpuCur->CPU_ulInterError[pcpuCur->CPU_ulInterNesting] = ulErrorCode;
    } else {
        ptcbCur = pcpuCur->CPU_ptcbTCBCur;
        if (ptcbCur) {
            ptcbCur->TCB_ulLastError = ulErrorCode;
        } else {
            _K_ulNotRunError = ulErrorCode;
        }
    }
    
    KN_INT_ENABLE(iregInterLevel);
}
/*********************************************************************************************************
** 函数名称: _DebugMessage
** 功能描述: 内核打印调试信息
** 输　入  : iLevel      等级
**           pcPosition  位置
**           pcString    打印内容
** 输　出  : 
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
VOID  _DebugMessage (INT  iLevel, CPCHAR  pcPosition, CPCHAR  pcString)
{
#if LW_CFG_ERRORMESSAGE_EN > 0
    if (_K_pfuncKernelDebugError && (iLevel & __ERRORMESSAGE_LEVEL)) {
        _K_pfuncKernelDebugError(pcPosition);
        _K_pfuncKernelDebugError("() error: ");
        _K_pfuncKernelDebugError(pcString);
        __ERROR_THREAD_SHOW();
    }
#endif

#if LW_CFG_LOGMESSAGE_EN > 0
    if (_K_pfuncKernelDebugLog && (iLevel & __LOGMESSAGE_LEVEL)) {
        _K_pfuncKernelDebugLog(pcString);
    }
#endif
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
