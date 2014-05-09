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
** 文   件   名: CPUUsageShow.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 03 月 30 日
**
** 描        述: 显示所有的线程的 CPU 占用率信息.

** BUG:
2009.09.18  缩短关中断的时间.
2009.09.18  使用新的算法, 极大的减少中断屏蔽时间.
2009.11.07  加入 -n 选项, 可连续测算多少次.
2009.12.14  加入对内核占用时间的显示.
2012.08.28  使用 API 测算利用率.
2012.12.11  tid 显示 7 位就可以了
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if LW_CFG_FIO_LIB_EN > 0 && LW_CFG_THREAD_CPU_USAGE_CHK_EN > 0
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
static const CHAR   _G_cCPUUsageInfoHdr[] = "\n\
       NAME        TID   PRI CPU  KERN\n\
---------------- ------- --- ---- ----\n";
/*********************************************************************************************************
** 函数名称: API_CPUUsageShow
** 功能描述: 显示所有的线程的的 CPU 占用率信息
** 输　入  : 
**           iWaitSec      每次检测多少秒
**           iTimes        检测多少次不超过 10s
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
                                           
                                       (不得在中断中调用)
*********************************************************************************************************/
LW_API  
VOID    API_CPUUsageShow (INT  iWaitSec, INT  iTimes)
{
    REGISTER INT              i;
             INT              iCounter;
             
             INT              iThreadNum;
             LW_OBJECT_HANDLE ulId[LW_CFG_MAX_THREADS];
             UINT8            ucThreadUsage[LW_CFG_MAX_THREADS];
             UINT8            ucThreadKernel[LW_CFG_MAX_THREADS];
    
             ULONG            iOptionNoAbort;
             ULONG            iOption;
    
    if (LW_CPU_GET_CUR_NESTING()) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return;
    }
    
    ioctl(STD_IN, FIOGETOPTIONS, &iOption);
    iOptionNoAbort = (iOption & ~OPT_ABORT);
    ioctl(STD_IN, FIOSETOPTIONS, iOptionNoAbort);                       /*  不允许 control-C 操作       */
            
    for (iCounter = 0; iCounter < iTimes; iCounter++) {
    
        API_ThreadCPUUsageRefresh();                                    /*  刷新统计变量                */
        printf("CPU usage checking, please wait...\n");
        
        {
            /*
             *  仅在这里延迟时允许 control-C 操作.
             */
            ioctl(STD_IN, FIOSETOPTIONS, iOption);                      /*  回复原来状态                */
            
            iWaitSec = (iWaitSec >  0) ? iWaitSec : 1;
            iWaitSec = (iWaitSec < 10) ? iWaitSec : 10;
            API_TimeSleep((ULONG)iWaitSec * LW_CFG_TICKS_PER_SEC);
            
            ioctl(STD_IN, FIOSETOPTIONS, iOptionNoAbort);               /*  不允许 control-C 操作       */
        }
        
        printf("CPU usage show >>\n");
        printf(_G_cCPUUsageInfoHdr);                                    /*  打印欢迎信息                */
        
        iThreadNum = API_ThreadGetCPUUsageAll(ulId, ucThreadUsage, ucThreadKernel, LW_CFG_MAX_THREADS);
        if (iThreadNum <= 0) {
            continue;
        }
        
        for (i = iThreadNum - 1; i >= 0; i--) {                         /*  反向                        */
            CHAR    cName[LW_CFG_OBJECT_NAME_SIZE];
            UINT8   ucPriority;
            
            if ((API_ThreadGetName(ulId[i], cName) == ERROR_NONE) &&
                (API_ThreadGetPriority(ulId[i], &ucPriority) == ERROR_NONE)) {
                
                printf("%-16s %7lx %3d %3d%% %3d%%\n", 
                       cName,                                           /*  线程名                      */
                       ulId[i],                                         /*  线程代码入口                */
                       ucPriority,                                      /*  优先级                      */
                       ucThreadUsage[i],
                       ucThreadKernel[i]);
            }
        }
    }
    ioctl(STD_IN, FIOSETOPTIONS, iOption);                              /*  回复原来状态                */
    
    printf("\n");
}
#endif                                                                  /*  LW_CFG_FIO_LIB_EN > 0       */
                                                                        /*  LW_CFG_THREAD_CPU_...       */
/*********************************************************************************************************
  END
*********************************************************************************************************/
