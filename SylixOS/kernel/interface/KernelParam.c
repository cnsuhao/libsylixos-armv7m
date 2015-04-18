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
**
** BUG:
2014/09.09  ncpus 取值范围为 [1 ~ LW_CFG_MAX_PROCESSORS].
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
                           ncpus=1     CPU 个数
                           dlog=no     DEBUG LOG 信息打印
                           derror=yes  DEBUG ERROR 信息打印
                           kfpu=no     内核态对浮点支持 (推荐为 no)
                           heapchk=yes 堆栈越界检查
                           varea=*     * 表示虚拟内存起始点, 默认为 0xC000_0000
                           vsize=*     * 表示虚拟内存大小, 默认为 1GB
                           hz=100      系统 tick 频率, 默认为 100 (推荐 100 ~ 10000 中间)
                           hhz=100     高速定时器频率, 默认与 hz 相同 (需 BSP 支持)
                           irate=5     应用定时器分辨率, 默认为 5 个 tick. (推荐 1 ~ 10 中间)
                           hpsec=1     热插拔循环检测间隔时间, 单位: 秒 (推荐 1 ~ 5 秒)
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
            if (iCpus > 0 && iCpus <= LW_CFG_MAX_PROCESSORS) {
                _K_ulNCpus = (ULONG)iCpus;
            }
            
#if LW_CFG_LOGMESSAGE_EN > 0
        } else if (lib_strncmp(pcTok, "kdlog=", 6) == 0) {              /*  是否使能内核 log 打印       */
            if (pcTok[6] == 'n') {
                _K_pfuncKernelDebugLog = LW_NULL;
            } else {
                _K_pfuncKernelDebugLog = bspDebugMsg;
            }
#endif                                                                  /*  LW_CFG_LOGMESSAGE_EN > 0    */

#if LW_CFG_ERRORMESSAGE_EN > 0
        } else if (lib_strncmp(pcTok, "kderror=", 8) == 0) {            /*  是否使能内核错误打印        */
            if (pcTok[8] == 'n') {
                _K_pfuncKernelDebugError = LW_NULL;
            } else {
                _K_pfuncKernelDebugError = bspDebugMsg;
            }
#endif                                                                  /*  LW_CFG_ERRORMESSAGE_EN > 0  */
        
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
        
        } else if (lib_strncmp(pcTok, "hz=", 3) == 0) {                 /*  tick 频率                   */
            ULONG   ulHz = (ULONG)lib_atol(&pcTok[3]);
            if (ulHz >= 100 && ulHz <= 10000) {                         /*  10ms ~ 100us                */
                LW_TICK_HZ = ulHz;
                LW_NSEC_PER_TICK = __TIMEVAL_NSEC_MAX / ulHz;
            }
        
        } else if (lib_strncmp(pcTok, "hhz=", 4) == 0) {                /*  高度定时器频率              */
            ULONG   ulHz = (ULONG)lib_atol(&pcTok[4]);
            if (ulHz >= 100 && ulHz <= 100000) {                        /*  10ms ~ 10us                 */
                LW_HTIMER_HZ = ulHz;
            }
        
        } else if (lib_strncmp(pcTok, "irate=", 6) == 0) {              /*  应用定时器分辨率            */
            ULONG   ulRate = (ULONG)lib_atol(&pcTok[6]);
            if (ulRate >= 1 && ulRate <= 10) {                          /*  1 ~ 10 ticks                */
                LW_ITIMER_RATE = ulRate;
            }
        
        } else if (lib_strncmp(pcTok, "hpsec=", 6) == 0) {              /*  热插拔循环检测周期          */
            ULONG   ulSec = (ULONG)lib_atol(&pcTok[6]);
            if (ulSec >= 1 && ulSec <= 10) {                            /*  1 ~ 10 ticks                */
                LW_HOTPLUG_SEC = ulSec;
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
