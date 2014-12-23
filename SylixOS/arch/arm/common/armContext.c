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
** 文   件   名: armContext.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 12 月 09 日
**
** 描        述: ARM 体系构架上下文处理.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
/*********************************************************************************************************
** 函数名称: archTaskCtxCreate
** 功能描述: 创建任务上下文
** 输　入  : pfuncTask      任务入口
**           pvArg          入口参数
**           pstkTop        初始化堆栈起点
**           ulOpt          任务创建选项
** 输　出  : 初始化堆栈结束点
** 全局变量: 
** 调用模块: 
** 注  意  : 堆栈从高地址向低地址增长.
*********************************************************************************************************/
PLW_STACK  archTaskCtxCreate (PTHREAD_START_ROUTINE  pfuncTask,
                              PVOID                  pvArg,
                              PLW_STACK              pstkTop, 
                              ULONG                  ulOpt)
{
    ARCH_REG_CTX      *pregctx;
    ARCH_FP_CTX       *pfpctx;
    
    if ((addr_t)pstkTop & 0x7) {                                        /*  保证出栈后 CPU SP 8 字节对齐*/
        pstkTop = (PLW_STACK)((addr_t)pstkTop - 4);                     /*  向低地址推进 4 字节         */
    }
    
    pfpctx  = (ARCH_FP_CTX  *)((PCHAR)pstkTop - sizeof(ARCH_FP_CTX));
    pregctx = (ARCH_REG_CTX *)((PCHAR)pstkTop - sizeof(ARCH_FP_CTX) - sizeof(ARCH_REG_CTX));
    
    pfpctx->FP_uiFp = (ARCH_REG_T)LW_NULL;
    pfpctx->FP_uiLr = (ARCH_REG_T)LW_NULL;
    
    pregctx->REG_uiCpsr = (ARCH_ARM_SVC32MODE | 0x40);
    
    pregctx->REG_uiR0  = (ARCH_REG_T)pvArg;
    pregctx->REG_uiR1  = 0x01010101;
    pregctx->REG_uiR2  = 0x02020202;
    pregctx->REG_uiR3  = 0x03030303;
    pregctx->REG_uiR4  = 0x04040404;
    pregctx->REG_uiR5  = 0x05050505;
    pregctx->REG_uiR6  = 0x06060606;
    pregctx->REG_uiR7  = 0x07070707;
    pregctx->REG_uiR8  = 0x08080808;
    pregctx->REG_uiR9  = 0x09090909;
    pregctx->REG_uiR10 = 0x10101010;
    pregctx->REG_uiFp  = pfpctx->FP_uiFp;
    pregctx->REG_uiIp  = 0x12121212;
    pregctx->REG_uiLr  = (ARCH_REG_T)pfuncTask;
    pregctx->REG_uiPc  = (ARCH_REG_T)pfuncTask;
    
    return  ((PLW_STACK)pregctx);
}
/*********************************************************************************************************
** 函数名称: archTaskCtxSetFp
** 功能描述: 设置任务上下文栈帧 (用于 backtrace 回溯, 详情请见 backtrace 相关文件)
** 输　入  : pstkDest  目的 stack frame
**           pstkSrc   源端 stack frame
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  archTaskCtxSetFp (PLW_STACK  pstkDest, PLW_STACK  pstkSrc)
{
    ARCH_REG_CTX      *pregctxDest = (ARCH_REG_CTX *)pstkDest;
    ARCH_REG_CTX      *pregctxSrc  = (ARCH_REG_CTX *)pstkSrc;
    ARCH_FP_CTX       *pfpctx      = (ARCH_FP_CTX *)((PCHAR)pregctxDest + sizeof(ARCH_REG_CTX));
    
    /*
     *  在 ARCH_FP_CTX 区域内, 模拟了一次 
     *  push {fp, lr}
     *  add  fp, sp, #4
     */
    pfpctx->FP_uiFp = pregctxSrc->REG_uiFp;
    pfpctx->FP_uiLr = pregctxSrc->REG_uiLr;
    
    pregctxDest->REG_uiFp = (ARCH_REG_T)&pfpctx->FP_uiLr;
}
/*********************************************************************************************************
** 函数名称: archTaskCtxShow
** 功能描述: 打印任务上下文
** 输　入  : iFd        文件描述符
			 pstkTop    堆栈栈顶
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_DEVICE_EN > 0

VOID  archTaskCtxShow (INT  iFd, PLW_STACK  pstkTop)
{
    CHAR        cCpsr[32 + 1] = "\0";
    LW_STACK    stkCpsr = pstkTop[0];
    
    if (stkCpsr & 0x80000000) {
        cCpsr[0] = 'N';
    } else {
        cCpsr[0] = 'n';
    }
    
    if (stkCpsr & 0x40000000) {
        cCpsr[1] = 'Z';
    } else {
        cCpsr[1] = 'z';
    }
    
    if (stkCpsr & 0x20000000) {
        cCpsr[2] = 'C';
    } else {
        cCpsr[2] = 'c';
    }
    
    if (stkCpsr & 0x10000000) {
        cCpsr[3] = 'V';
    } else {
        cCpsr[3] = 'v';
    }
    
    if (stkCpsr & 0x08000000) {
        cCpsr[4] = 'Q';
    } else {
        cCpsr[4] = 'q';
    }
    
    if (stkCpsr & 0x80) {
        cCpsr[5] = 'I';
    } else {
        cCpsr[5] = 'i';
    }
    
    if (stkCpsr & 0x40) {
        cCpsr[6] = 'F';
    } else {
        cCpsr[6] = 'f';
    }
    
    if (stkCpsr & 0x20) {
        cCpsr[7] = 'T';
    } else {
        cCpsr[7] = 't';
    }
    cCpsr[8] = 0;
    
    stkCpsr &= 0x1F;
    
    switch (stkCpsr) {
    
    case ARCH_ARM_USR32MODE:
        lib_strcpy(&cCpsr[8], "_USER");
        break;
        
    case ARCH_ARM_FIQ32MODE:
        lib_strcpy(&cCpsr[8], "_FIQ");
        break;
        
    case ARCH_ARM_IRQ32MODE:
        lib_strcpy(&cCpsr[8], "_IRQ");
        break;
        
    case ARCH_ARM_SVC32MODE:
        lib_strcpy(&cCpsr[8], "_SVC");
        break;
        
    case ARCH_ARM_ABT32MODE:
        lib_strcpy(&cCpsr[8], "_ABT");
        break;
        
    case ARCH_ARM_UND32MODE:
        lib_strcpy(&cCpsr[8], "_UND");
        break;
        
    case ARCH_ARM_SYS32MODE:
        lib_strcpy(&cCpsr[8], "_SYS");
        break;
        
    default:
        lib_strcpy(&cCpsr[8], "_!!!");
        break;
    }
    
    fdprintf(iFd, "cpsr = %s\n", cCpsr);
    
    fdprintf(iFd, "r0  = 0x%08x  ", pstkTop[1]);
    fdprintf(iFd, "r1  = 0x%08x\n", pstkTop[2]);
    fdprintf(iFd, "r2  = 0x%08x  ", pstkTop[3]);
    fdprintf(iFd, "r3  = 0x%08x\n", pstkTop[4]);
    fdprintf(iFd, "r4  = 0x%08x  ", pstkTop[5]);
    fdprintf(iFd, "r5  = 0x%08x\n", pstkTop[6]);
    fdprintf(iFd, "r6  = 0x%08x  ", pstkTop[7]);
    fdprintf(iFd, "r7  = 0x%08x\n", pstkTop[8]);
    fdprintf(iFd, "r8  = 0x%08x  ", pstkTop[9]);
    fdprintf(iFd, "r9  = 0x%08x\n", pstkTop[10]);
    fdprintf(iFd, "r10 = 0x%08x  ", pstkTop[11]);
    fdprintf(iFd, "fp  = 0x%08x\n", pstkTop[12]);
    fdprintf(iFd, "ip  = 0x%08x  ", pstkTop[13]);
    fdprintf(iFd, "sp  = 0x%08x\n", (ARCH_REG_T)pstkTop);
    fdprintf(iFd, "lr  = 0x%08x  ", pstkTop[14]);
    fdprintf(iFd, "pc  = 0x%08x\n", pstkTop[15]);
}

#endif                                                                  /*  LW_CFG_DEVICE_EN > 0        */
/*********************************************************************************************************
  END
*********************************************************************************************************/
