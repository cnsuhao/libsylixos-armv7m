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
** 文   件   名: _SmpSpinlock.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 07 月 21 日
**
** 描        述: 多 CPU 系统自旋锁.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_SMP_EN > 0
/*********************************************************************************************************
  自旋锁驱动返回值
*********************************************************************************************************/
#define LW_SPIN_OK      1
#define LW_SPIN_ERROR   0
/*********************************************************************************************************
** 函数名称: _SmpSpinInit
** 功能描述: 自旋锁初始化
** 输　入  : psl           自旋锁
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SmpSpinInit (spinlock_t *psl)
{
    __ARCH_SPIN_INIT(psl);
    KN_SMP_WMB();
}
/*********************************************************************************************************
** 函数名称: _SmpSpinLock
** 功能描述: 自旋锁加锁操作
** 输　入  : psl           自旋锁
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SmpSpinLock (spinlock_t *psl)
{
    INTREG          iregInterLevel;
    PLW_CLASS_CPU   pcpuCur;
    
    iregInterLevel = KN_INT_DISABLE();
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    __ARCH_SPIN_LOCK(psl);                                              /*  驱动保证锁定必须成功        */
    KN_SMP_MB();
    
    KN_INT_ENABLE(iregInterLevel);
}
/*********************************************************************************************************
** 函数名称: _SmpSpinTryLock
** 功能描述: 自旋锁尝试加锁操作
** 输　入  : psl           自旋锁
** 输　出  : LW_TRUE 加锁正常 LW_FALSE 加锁失败
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
BOOL  _SmpSpinTryLock (spinlock_t *psl)
{
    INTREG          iregInterLevel;
    PLW_CLASS_CPU   pcpuCur;
    INT             iRet;
    
    iregInterLevel = KN_INT_DISABLE();
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    iRet = __ARCH_SPIN_TRYLOCK(psl);
    KN_SMP_MB();
    
    if (iRet != LW_SPIN_OK) {
        if (!pcpuCur->CPU_ulInterNesting) {
            __THREAD_LOCK_DEC(pcpuCur->CPU_ptcbTCBCur);                 /*  解锁失败, 解除任务锁定      */
        }
    }
    
    KN_INT_ENABLE(iregInterLevel);
    
    return  ((iRet == LW_SPIN_OK) ? (LW_TRUE) : (LW_FALSE));
}
/*********************************************************************************************************
** 函数名称: _SmpSpinUnlock
** 功能描述: 自旋锁解锁操作
** 输　入  : psl           自旋锁
** 输　出  : 调度器返回值
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  _SmpSpinUnlock (spinlock_t *psl)
{
    INTREG          iregInterLevel;
    PLW_CLASS_CPU   pcpuCur;
    PLW_CLASS_TCB   ptcbCur;
    BOOL            bTrySched = LW_FALSE;
    INT             iRet;
    
    iregInterLevel = KN_INT_DISABLE();
    
    KN_SMP_MB();
    iRet = __ARCH_SPIN_UNLOCK(psl);
    _BugHandle((iRet != LW_SPIN_OK), LW_TRUE, "unlock error!\r\n");
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        ptcbCur = pcpuCur->CPU_ptcbTCBCur;
        __THREAD_LOCK_DEC(ptcbCur);                                     /*  解除任务锁定                */
        if (__SHOULD_SCHED(pcpuCur, 0)) {
            bTrySched = LW_TRUE;                                        /*  需要尝试调度                */
        }
    }
    
    KN_INT_ENABLE(iregInterLevel);
    
    if (bTrySched) {
        return  (_ThreadSched(ptcbCur));
    
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: _SmpSpinLockIgnIrq
** 功能描述: 自旋锁加锁操作, 忽略中断锁定 (必须在中断关闭的状态下被调用)
** 输　入  : psl           自旋锁
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SmpSpinLockIgnIrq (spinlock_t *psl)
{
    PLW_CLASS_CPU   pcpuCur;
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    __ARCH_SPIN_LOCK(psl);                                              /*  驱动保证锁定必须成功        */
    KN_SMP_MB();
}
/*********************************************************************************************************
** 函数名称: _SmpSpinTryLockIgnIrq
** 功能描述: 自旋锁尝试加锁操作, 忽略中断锁定 (必须在中断关闭的状态下被调用)
** 输　入  : psl           自旋锁
** 输　出  : LW_TRUE 加锁正常 LW_FALSE 加锁失败
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
BOOL  _SmpSpinTryLockIgnIrq (spinlock_t *psl)
{
    PLW_CLASS_CPU   pcpuCur;
    INT             iRet;
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    iRet = __ARCH_SPIN_TRYLOCK(psl);
    KN_SMP_MB();
    
    if (iRet != LW_SPIN_OK) {
        if (!pcpuCur->CPU_ulInterNesting) {
            __THREAD_LOCK_DEC(pcpuCur->CPU_ptcbTCBCur);                 /*  解锁失败, 解除任务锁定      */
        }
    }
    
    return  ((iRet == LW_SPIN_OK) ? (LW_TRUE) : (LW_FALSE));
}
/*********************************************************************************************************
** 函数名称: _SmpSpinUnlockIgnIrq
** 功能描述: 自旋锁解锁操作, 忽略中断锁定 (必须在中断关闭的状态下被调用)
** 输　入  : psl           自旋锁
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SmpSpinUnlockIgnIrq (spinlock_t *psl)
{
    PLW_CLASS_CPU   pcpuCur;
    INT             iRet;
    
    KN_SMP_MB();
    iRet = __ARCH_SPIN_UNLOCK(psl);
    _BugHandle((iRet != LW_SPIN_OK), LW_TRUE, "unlock error!\r\n");
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_DEC(pcpuCur->CPU_ptcbTCBCur);                     /*  解除任务锁定                */
    }
}
/*********************************************************************************************************
** 函数名称: _SmpSpinLockIrq
** 功能描述: 自旋锁加锁操作, 连同锁定中断
** 输　入  : psl               自旋锁
**           piregInterLevel   中断锁定信息
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SmpSpinLockIrq (spinlock_t *psl, INTREG  *piregInterLevel)
{
    PLW_CLASS_CPU   pcpuCur;
    
    *piregInterLevel = KN_INT_DISABLE();
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    __ARCH_SPIN_LOCK(psl);                                              /*  驱动保证锁定必须成功        */
    KN_SMP_MB();
}
/*********************************************************************************************************
** 函数名称: _SmpSpinTryLockIrq
** 功能描述: 自旋锁尝试加锁操作, 连同锁定中断
** 输　入  : psl               自旋锁
**           piregInterLevel   中断锁定信息
** 输　出  : LW_TRUE 加锁正常 LW_FALSE 加锁失败
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
BOOL  _SmpSpinTryLockIrq (spinlock_t *psl, INTREG  *piregInterLevel)
{
    PLW_CLASS_CPU   pcpuCur;
    INT             iRet;
    
    *piregInterLevel = KN_INT_DISABLE();
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_INC(pcpuCur->CPU_ptcbTCBCur);                     /*  锁定任务在当前 CPU          */
    }
    
    iRet = __ARCH_SPIN_TRYLOCK(psl);
    KN_SMP_MB();
    
    if (iRet != LW_SPIN_OK) {
        if (!pcpuCur->CPU_ulInterNesting) {
            __THREAD_LOCK_DEC(pcpuCur->CPU_ptcbTCBCur);                 /*  解锁失败, 解除任务锁定      */
        }
        KN_INT_ENABLE(*piregInterLevel);
    }
    
    return  ((iRet == LW_SPIN_OK) ? (LW_TRUE) : (LW_FALSE));
}
/*********************************************************************************************************
** 函数名称: _SmpSpinUnlockIrq
** 功能描述: 自旋锁解锁操作, 连同解锁中断
** 输　入  : psl               自旋锁
**           iregInterLevel    中断锁定信息
** 输　出  : 调度器返回值
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  _SmpSpinUnlockIrq (spinlock_t *psl, INTREG  iregInterLevel)
{
    PLW_CLASS_CPU   pcpuCur;
    PLW_CLASS_TCB   ptcbCur;
    BOOL            bTrySched = LW_FALSE;
    INT             iRet;
    
    KN_SMP_MB();
    iRet = __ARCH_SPIN_UNLOCK(psl);
    _BugHandle((iRet != LW_SPIN_OK), LW_TRUE, "unlock error!\r\n");
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        ptcbCur = pcpuCur->CPU_ptcbTCBCur;
        __THREAD_LOCK_DEC(ptcbCur);                                     /*  解除任务锁定                */
        if (__SHOULD_SCHED(pcpuCur, 0)) {
            bTrySched = LW_TRUE;                                        /*  需要尝试调度                */
        }
    }
    
    KN_INT_ENABLE(iregInterLevel);
    
    if (bTrySched) {
        return  (_ThreadSched(ptcbCur));
    
    } else {
        return  (ERROR_NONE);
    }
}
/*********************************************************************************************************
** 函数名称: _SmpSpinUnlockIrqQuick
** 功能描述: 自旋锁解锁操作, 连同解锁中断, 不进行尝试调度
** 输　入  : psl               自旋锁
**           iregInterLevel    中断锁定信息
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SmpSpinUnlockIrqQuick (spinlock_t *psl, INTREG  iregInterLevel)
{
    PLW_CLASS_CPU   pcpuCur;
    INT             iRet;

    KN_SMP_MB();
    iRet = __ARCH_SPIN_UNLOCK(psl);
    _BugHandle((iRet != LW_SPIN_OK), LW_TRUE, "unlock error!\r\n");
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_DEC(pcpuCur->CPU_ptcbTCBCur);                     /*  解除任务锁定                */
    }
    
    KN_INT_ENABLE(iregInterLevel);
}
/*********************************************************************************************************
** 函数名称: _SmpSpinUnlockSched
** 功能描述: SMP 调度器切换完成后专用释放函数 (关中断状态下被调用)
** 输　入  : psl           自旋锁
**           ptcbOwner     锁的持有者
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _SmpSpinUnlockSched (spinlock_t *psl, PLW_CLASS_TCB ptcbOwner)
{
    PLW_CLASS_CPU   pcpuCur;
    INT             iRet;
    
    KN_SMP_MB();
    iRet = __ARCH_SPIN_UNLOCK(psl);
    _BugHandle((iRet != LW_SPIN_OK), LW_TRUE, "unlock error!\r\n");
    
    pcpuCur = LW_CPU_GET_CUR();
    if (!pcpuCur->CPU_ulInterNesting) {
        __THREAD_LOCK_DEC(ptcbOwner);                                   /*  解除任务锁定                */
    }
}

#endif                                                                  /*  LW_CFG_SMP_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
