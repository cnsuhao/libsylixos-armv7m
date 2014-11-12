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
** 文   件   名: KernelReboot.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 03 月 02 日
**
** 描        述: 内核重新启动函数.

** BUG:
2009.05.05  需要禁能 CACHE 以适应 bootloader 的工作.
2009.05.31  必须在 except 线程执行, 而且需要锁住内核.
2009.06.24  API_KernelRebootEx() 启动地址中的局部变量可能会被一些粗心的 BSP CACHE 函数改写, 这里设置全局
            变量保护.
2011.03.08  加入 c++ 运行时卸载函数.
2012.03.26  加入 __LOGMESSAGE_LEVEL 信息打印.
2012.11.09  加入系统重新启动类型定义, 当遇到严重错误时不调用回调并立即重新启动.
2013.12.03  多核系统重启时, 需要将 LW_NCPUS - 1 个 idle 线程优先级提到最高, 抢占其他核的 CPU 然后实际上
            成为单核系统执行复位重启的操作.
2014.07.21  重启时, 自动关闭其他 CPU.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  系统函数声明
*********************************************************************************************************/
#if LW_CFG_SIGNAL_EN > 0
INT  _excJobAdd(VOIDFUNCPTR  pfunc, 
                PVOID        pvArg0,
                PVOID        pvArg1,
                PVOID        pvArg2,
                PVOID        pvArg3,
                PVOID        pvArg4,
                PVOID        pvArg5);
#endif                                                                  /*  LW_CFG_SIGNAL_EN > 0        */
VOID _cppRtUninit(VOID);
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
static addr_t   _K_ulRebootStartAddress;                                /*  重新引导地址                */
/*********************************************************************************************************
** 函数名称: __makeOtherDown
** 功能描述: 关闭其他 CPU. (LW_NCPUS 必须大于 1)
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_SMP_EN > 0

static VOID  __makeOtherDown (VOID)
{
    ULONG   i;
    BOOL    bNeedWait;
    
    for (i = 1; i < LW_NCPUS; i++) {                                    /*  除 0 以外的其他 CPU         */
        API_CpuDown(i);
    }
    
    do {
        bNeedWait = LW_FALSE;
        for (i = 1; i < LW_NCPUS; i++) {
            if (API_CpuIsUp(i)) {                                       /*  确保除 0 核外, 其他 CPU 全关*/
                bNeedWait = LW_TRUE;
            }
        }
        LW_SPINLOCK_DELAY();
    } while (bNeedWait);
}

#endif                                                                  /*  LW_CFG_SMP_EN > 0           */
/*********************************************************************************************************
** 函数名称: API_KernelReboot
** 功能描述: 内核重新启动函数
** 输　入  : iRebootType        重启类型 
                                LW_REBOOT_FORCE
                                LW_REBOOT_WARM
                                LW_REBOOT_COLD
                                LW_REBOOT_SHUTDOWN
                                LW_REBOOT_POWEROFF
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
VOID   API_KernelReboot (INT  iRebootType)
{
    API_KernelRebootEx(iRebootType, 0ull);
}
/*********************************************************************************************************
** 函数名称: API_KernelReboot
** 功能描述: 内核重新启动函数
** 输　入  : iRebootType        重启类型 
                                LW_REBOOT_FORCE
                                LW_REBOOT_WARM
                                LW_REBOOT_COLD
                                LW_REBOOT_SHUTDOWN
                                LW_REBOOT_POWEROFF
**           ulStartAddress     启动地址
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
VOID   API_KernelRebootEx (INT  iRebootType, addr_t  ulStartAddress)
{
    INTREG  iregInterLevel;
    ULONG   ulI;

#if LW_CFG_SIGNAL_EN > 0
    if (LW_CPU_GET_CUR_NESTING() || (API_ThreadIdSelf() != API_KernelGetExc())) {
        _excJobAdd(API_KernelRebootEx, (PVOID)iRebootType, (PVOID)ulStartAddress, 0, 0, 0, 0);
        return;
    }
#endif                                                                  /*  LW_CFG_SIGNAL_EN > 0        */

    _DebugHandle(__LOGMESSAGE_LEVEL, "kernel rebooting...\r\n");
    
    _K_ulRebootStartAddress = ulStartAddress;                           /*  记录局部变量, 防止 XXX      */
    
#if LW_CFG_SMP_EN > 0
    if (LW_NCPUS > 1) {
        __makeOtherDown();                                              /*  将其他 CPU 设置为 idle 模式 */
    }
#endif                                                                  /*  LW_CFG_SMP_EN               */
    
    if (iRebootType != LW_REBOOT_FORCE) {
        __LW_KERNEL_REBOOT_HOOK(iRebootType);                           /*  调用回调函数                */
        _cppRtUninit();                                                 /*  卸载 C++ 运行时             */
    }
    
    for (ulI = 0; ulI < LW_CFG_MAX_INTER_SRC; ulI++) {
        API_InterVectorDisable(ulI);                                    /*  关闭所有中断                */
    }
    
    iregInterLevel = __KERNEL_ENTER_IRQ();                              /*  进入内核同时关闭中断        */

#if LW_CFG_CACHE_EN > 0
    API_CacheDisable(DATA_CACHE);                                       /*  禁能 CACHE                  */
    API_CacheDisable(INSTRUCTION_CACHE);
#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */

#if LW_CFG_VMM_EN > 0
    API_VmmMmuDisable();                                                /*  关闭 MMU                    */
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */

    archReboot(iRebootType, _K_ulRebootStartAddress);                   /*  调用体系架构重启操作        */
    
    _BugHandle(LW_TRUE, LW_TRUE, "kernel reboot error!\r\n")            /*  不会运行到这里              */
    
    __KERNEL_EXIT_IRQ(iregInterLevel);                                  /*  退出内核同时打开中断        */
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
