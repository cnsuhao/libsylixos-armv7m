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
** 文   件   名: ThreadUnlock.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 18 日
**
** 描        述: 任务打开调度器.

** BUG
2008.01.20  将全局锁改为局部锁.
2008.03.30  当前任务主动放弃 CPU 时, 即: 当前为非就绪状态, 应该予以调度.
2008.04.06  加入线程上下文的返回值的传递.
2013.07.18  兼容 SMP 系统.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: API_ThreadUnlock
** 功能描述: 任务打开调度器.
** 输　入  : NONE
** 输　出  : 此返回值保留.
** 全局变量: 
** 调用模块: 
** 注  意  : 此 API 仅对当前 CPU 生效, 并不会影响其他 CPU 调度.
                                           API 函数
*********************************************************************************************************/
LW_API
INT  API_ThreadUnlock (VOID)
{
    INTREG          iregInterLevel;
    PLW_CLASS_CPU   pcpuCur;
    PLW_CLASS_TCB   ptcbCur;
    BOOL            bTrySched = LW_FALSE;
   
    if (!LW_SYS_STATUS_IS_RUNNING()) {                                  /*  系统必须已经启动            */
        _ErrorHandle(ERROR_KERNEL_NOT_RUNNING);
        return  (ERROR_NONE);
    }
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);
    
    KN_SMP_MB();
    
    if (__THREAD_LOCK_GET(ptcbCur)) {
        __THREAD_LOCK_DEC(ptcbCur);                                     /*  解锁任务                    */
    }
    
    iregInterLevel = KN_INT_DISABLE();                                  /*  关中断, 禁止 CPU 调度       */
    
    pcpuCur = LW_CPU_GET_CUR();
    if (__COULD_SCHED(pcpuCur, 0)) {
        bTrySched = LW_TRUE;                                            /*  需要尝试调度                */
    }
    
    KN_INT_ENABLE(iregInterLevel);

    if (bTrySched) {
        _ThreadSched(ptcbCur);                                          /*  尝试调度                    */
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
