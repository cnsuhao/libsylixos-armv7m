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
** 文   件   名: KernelParam.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2014 年 08 月 08 日
**
** 描        述: 这是系统内核启动参数设置文件。
*********************************************************************************************************/
#define  __KERNEL_NCPUS_SET
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0
#include "../SylixOS/kernel/vmm/virPage.h"
#endif
/*********************************************************************************************************
** 函数名称: API_KernelStartParam
** 功能描述: 系统内核启动参数
** 输　入  : pcParam       启动参数, 是以空格分开的一个字符串列表，通常具有如下形式:
                           ncpus=1 dlog=no derror=yes ... 
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                       API 函数
*********************************************************************************************************/
LW_API
ULONG  API_KernelStartParam (CPCHAR  pcParam)
{
    CHAR        cParamBuffer[512];                                      /*  参数长度不得超过 512 字节   */
    PCHAR       pcDelim = " ";
    PCHAR       pcLast;
    PCHAR       pcTok;
    
#if LW_CFG_VMM_EN > 0
    PLW_MMU_VIRTUAL_DESC    pvirdesc = __vmmVirtualDesc();
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
    
    if (LW_SYS_STATUS_IS_RUNNING()) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "kernel is already start.\r\n");
        _ErrorHandle(ERROR_KERNEL_RUNNING);
        return  (ERROR_KERNEL_RUNNING);
    }
    
    lib_strlcpy(cParamBuffer, pcParam, 512);

    pcTok = lib_strtok_r(cParamBuffer, pcDelim, &pcLast);
    while (pcTok) {
        if (lib_strncmp(pcTok, "ncpus=", 6) == 0) {                     /*  CPU 数量                    */
            INT     iCpus = lib_atoi(&pcTok[6]);
            if (iCpus > 0 && iCpus < LW_CFG_MAX_PROCESSORS) {
                _K_ulNCpus = (ULONG)iCpus;
            }
            
        } else if (lib_strncmp(pcTok, "kdlog=", 6) == 0) {              /*  是否使能内核 log 打印       */
            if (pcTok[6] == 'n') {
                _K_pfuncKernelDebugLog = LW_NULL;
            } else {
                _K_pfuncKernelDebugLog = bspDebugMsg;
            }
            
        } else if (lib_strncmp(pcTok, "kderror=", 8) == 0) {            /*  是否使能内核错误打印        */
            if (pcTok[8] == 'n') {
                _K_pfuncKernelDebugError = LW_NULL;
            } else {
                _K_pfuncKernelDebugError = bspDebugMsg;
            }
        
        } else if (lib_strncmp(pcTok, "kfpu=", 5) == 0) {               /*  是否使能内核浮点支持        */
            if (pcTok[5] == 'n') {
                _K_bInterFpuEn = LW_FALSE;
            } else {
                _K_bInterFpuEn = LW_TRUE;
            }
        
        } else if (lib_strncmp(pcTok, "heapchk=", 8) == 0) {            /*  是否进行堆内存越界检查      */
            if (pcTok[8] == 'n') {
                _K_bHeapCrossBorderEn = LW_FALSE;
            } else {
                _K_bHeapCrossBorderEn = LW_TRUE;
            }
        }
        
#if LW_CFG_VMM_EN > 0
          else if (lib_strncmp(pcTok, "varea=", 6) == 0) {              /*  虚拟内存起始点              */
            pvirdesc->ulVirtualSwitch = lib_strtoul(&pcTok[6], LW_NULL, 16);
            pvirdesc->ulVirtualStart  = pvirdesc->ulVirtualSwitch 
                                      + LW_CFG_VMM_PAGE_SIZE;
          
        } else if (lib_strncmp(pcTok, "vsize=", 6) == 0) {              /*  虚拟内存大小                */
            pvirdesc->stSize = (size_t)lib_strtoul(&pcTok[6], LW_NULL, 16);
        }
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
        
        pcTok = lib_strtok_r(LW_NULL, pcDelim, &pcLast);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
