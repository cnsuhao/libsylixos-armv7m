/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                       SylixOS(TM)
**
**                               Copyright  All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: _UpSpinlock.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 07 月 21 日
**
** 描        述: 单 CPU 系统自旋锁.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_SMP_EN == 0
/*********************************************************************************************************
** 函数名称: _UpSpinLock
** 功能描述: 自旋锁加锁操作
** 输　入  : psl           自旋锁
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _UpSpinLock (spinlock_t *psl)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
}
/*********************************************************************************************************
** 函数名称: _UpSpinTryLock
** 功能描述: 自旋锁尝试加锁操作
** 输　入  : psl           自旋锁
** 输　出  : LW_TRUE 加锁正常 LW_FALSE 加锁失败
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
BOOL  _UpSpinTryLock (spinlock_t *psl)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    return  (LW_TRUE);
}
/*********************************************************************************************************
** 函数名称: _UpSpinUnlock
** 功能描述: 自旋锁解锁操作
** 输　入  : psl           自旋锁
** 输　出  : 调度器返回值
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  _UpSpinUnlock (spinlock_t *psl)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_DEC(pcpuCur->CPU_ptcbTCBCur);                     /*  解除任务锁定                */
        if (__SHOULD_SCHED(pcpuCur, 0)) {
            return  (_ThreadSched(pcpuCur->CPU_ptcbTCBCur));            /*  尝试一次调度                */
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: _UpSpinLockIgnIrq
** 功能描述: 自旋锁加锁操作, 忽略中断锁定 (必须在中断关闭的状态下被调用)
** 输　入  : psl           自旋锁
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _UpSpinLockIgnIrq (spinlock_t *psl)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
}
/*********************************************************************************************************
** 函数名称: _UpSpinTryLockIgnIrq
** 功能描述: 自旋锁尝试加锁操作, 忽略中断锁定 (必须在中断关闭的状态下被调用)
** 输　入  : psl           自旋锁
** 输　出  : LW_TRUE 加锁正常 LW_FALSE 加锁失败
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
BOOL  _UpSpinTryLockIgnIrq (spinlock_t *psl)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    return  (LW_TRUE);
}
/*********************************************************************************************************
** 函数名称: _UpSpinUnlockIgnIrq
** 功能描述: 自旋锁解锁操作, 忽略中断锁定 (必须在中断关闭的状态下被调用)
** 输　入  : psl           自旋锁
** 输　出  : NONE (不进行调度尝试)
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _UpSpinUnlockIgnIrq (spinlock_t *psl)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_DEC(pcpuCur->CPU_ptcbTCBCur);                     /*  解锁任务在当前 CPU          */
    }
}
/*********************************************************************************************************
** 函数名称: _UpSpinLockIrq
** 功能描述: 自旋锁加锁操作, 连同锁定中断
** 输　入  : psl               自旋锁
**           piregInterLevel   中断锁定信息
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _UpSpinLockIrq (spinlock_t *psl, INTREG  *piregInterLevel)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    *piregInterLevel = KN_INT_DISABLE();
}
/*********************************************************************************************************
** 函数名称: _UpSpinTryLockIrq
** 功能描述: 自旋锁尝试加锁操作, 连同锁定中断
** 输　入  : psl               自旋锁
**           piregInterLevel   中断锁定信息
** 输　出  : LW_TRUE 加锁正常 LW_FALSE 加锁失败
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
BOOL  _UpSpinTryLockIrq (spinlock_t *psl, INTREG  *piregInterLevel)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    *piregInterLevel = KN_INT_DISABLE();
    
    return  (LW_TRUE);
}
/*********************************************************************************************************
** 函数名称: _UpSpinUnlockIrq
** 功能描述: 自旋锁解锁操作, 连同解锁中断
** 输　入  : psl               自旋锁
**           iregInterLevel    中断锁定信息
** 输　出  : 调度器返回值
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  _UpSpinUnlockIrq (spinlock_t *psl, INTREG  iregInterLevel)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    KN_INT_ENABLE(iregInterLevel);
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_DEC(pcpuCur->CPU_ptcbTCBCur);                     /*  解除任务锁定                */
        if (__SHOULD_SCHED(pcpuCur, 0)) {
            return  (_ThreadSched(pcpuCur->CPU_ptcbTCBCur));            /*  尝试一次调度                */
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: _UpSpinUnlockIrqQuick
** 功能描述: 自旋锁解锁操作, 连同解锁中断, 不进行尝试调度
** 输　入  : psl               自旋锁
**           iregInterLevel    中断锁定信息
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _UpSpinUnlockIrqQuick (spinlock_t *psl, INTREG  iregInterLevel)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    KN_INT_ENABLE(iregInterLevel);
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_DEC(pcpuCur->CPU_ptcbTCBCur);                     /*  解锁任务在当前 CPU          */
    }
}
/*********************************************************************************************************
** 函数名称: _UpSpinUnlockSched
** 功能描述: 单处理器调度器切换完成后专用释放函数 (关中断状态下被调用)
** 输　入  : psl           自旋锁
**           ptcbOwner     锁的持有者
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _UpSpinUnlockSched (spinlock_t *psl, PLW_CLASS_TCB ptcbOwner)
{
    PLW_CLASS_CPU   pcpuCur = LW_CPU_GET_CUR();
    
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_DEC(ptcbOwner);                                   /*  解锁任务在当前 CPU          */
    }
}

#endif                                                                  /*  LW_CFG_SMP_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/

