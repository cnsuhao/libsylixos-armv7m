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
** 文   件   名: ttinyShellSysCmd.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 07 月 27 日
**
** 描        述: 一个超小型的 shell 系统, 系统内部命令定义.

** BUG
2008.10.12  加入更多的 shell 系统命令.
2008.12.23  加入 vmm 显示指令.
2009.02.14  加入 vars 显示命令, 显示系统环境变量.
2009.02.19  加入 sync 命令.
2009.03.11  help 格式字串的显示有色彩提示.
2009.04.09  加入消息队列, 信号量, 事件集, 定长内存分区的信息显示.
2009.05.30  修正了 help 显示的相关 BUG.
2009.06.17  time 加入参数可现实 UTC 时间.
2009.06.30  help -s 命令中加入对格式字串的显示.
2009.07.02  加入 sprio 命令.
2009.07.03  修正了 GCC 产生的警告.
2009.08.08  今天是北京奥运一周年纪念日, 还是怀念那个激动与难忘的日子.
            __tshellSysCmdHelp() 在收到 q 时, 退出前需要清空接收缓冲.
2009.10.27  加入对 printk 打印信息级别的设置.
            加入 buss 命令
2009.11.07  cpuus 加入 -n 参数.
2009.11.19  加入 open close 命令, 最重要的是, control-C 后, 有可能有文件描述符残留(没有 cleanup push), 
            可以使用 close 强制关闭.
2009.11.21  加入 user 命令, 可以对用户进行管理(简单管理).
2009.12.11  加入 devs -a 选项.
2010.01.04  由于使用了新的 TOD 时间, 这里加入 hwclock 命令用于同步硬件时钟.
2010.01.16  加入 shstack 命令.
2010.07.10  命令 __tshellSysCmdTimes() 与 __tshellSysCmdHwclock() 使用 asctime_r() 可重入函数.
2011.02.17  加入 date 命令, 支持 shell 设置时间.
2011.02.22  加入 sigqueue 命令, 发送排队信号.
2011.05.17  加入对 vmm 访问中止统计信息打印命令.
2011.06.11  加入时区同步命令, 从 TZ 环境变量中同步时区信息.
2011.06.23  使用新的色彩转义.
2011.07.03  当 varsave 与 varload 时没有指定参数时, 默认使用 __TTINY_SHELL_VAR_FILE
2012.03.31  暂时取消 user 命令, 未来加入用户工具.
2012.09.05  cpuus 加入对检测时间的设置功能.
2012.10.19  vars 命令加入对变量直接引用的打印.
2012.11.09  加入 shutdown 命令.
2012.12.11  ts 命令可以显示指定进程内的线程.
2012.12.22  加入 fdentrys 命令.
2013.01.07  open 加入对 inode 和文件大小的打印.
2013.06.13  加入 renice 命令.
2013.08.27  加入 monitor 命令.
2013.09.30  加入 lspci 命令.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "limits.h"
#include "unistd.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_SHELL_EN > 0
#include "crypt.h"
#include "pwd.h"
#include "ttinyShell.h"
#include "ttinyShellLib.h"
#include "ttinyShellSysCmd.h"
/*********************************************************************************************************
  变量内部支持
*********************************************************************************************************/
#include "../SylixOS/shell/ttinyVar/ttinyVarLib.h"
#include "../SylixOS/posix/include/px_resource.h"
/*********************************************************************************************************
** 函数名称: __tshellSysCmdArgs
** 功能描述: 系统命令 "args"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdArgs (INT  iArgC, PCHAR  ppcArgV[])
{
    REGISTER INT    i;
    
    for (i = 0; i < iArgC; i++) {
        printf("arg %3d is %s\n", i + 1, ppcArgV[i]);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdEcho
** 功能描述: 系统命令 "echo"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdEcho (INT  iArgC, PCHAR  ppcArgV[])
{
    REGISTER INT   i;
    
    for (i = 1; i < iArgC; i++) {
        printf("%s ", ppcArgV[i]);
    }
    printf("\n");
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdHelp
** 功能描述: 系统命令 "help"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdHelp (INT  iArgC, PCHAR  ppcArgV[])
{
#define __SHELL_LINE_PER_SCREEN     23
    
    REGISTER ULONG          ulGetNum;
    REGISTER INT            i;
             CHAR           cData;
    
    __PTSHELL_KEYWORD       pskwNodeStart = LW_NULL;
    __PTSHELL_KEYWORD       pskwNode[__SHELL_LINE_PER_SCREEN];

    if (iArgC == 1) {                                                   /*  需要显示所有信息            */
        
        do {
            ulGetNum = __tshellKeywordList(pskwNodeStart,
                                           pskwNode,
                                           __SHELL_LINE_PER_SCREEN);
            for (i = 0; i < ulGetNum; i++) {
                printf("%-20s", pskwNode[i]->SK_pcKeyword);             /*  打印关键字队列              */
                if (pskwNode[i]->SK_pcFormatString) {
                    API_TShellCtlCharSend(__LW_VT100_FUNC_COLOR, 
                                          __LW_VT100_COLOR_PURPLE);     /*  设置品红前景                */
                    printf("%s", pskwNode[i]->SK_pcFormatString);       /*  打印格式信息                */
                    API_TShellCtlCharSend(__LW_VT100_FUNC_COLOR, 
                                          __LW_VT100_COLOR_GREEN);      /*  设置绿色前景                */
                }
                printf("\n");
            }
        
            if (ulGetNum == __SHELL_LINE_PER_SCREEN) {
                printf("\npress ENTER to continue, 'Q' to quit.\n");    /*  需要翻屏                    */
                read(0, &cData, 1);
                if (cData == 'q' || cData == 'Q') {                     /*  是否需要退出                */
                    ioctl(STD_IN, FIOFLUSH, 0);                         /*  清空接收缓冲                */
                    break;                                              /*  退出                        */
                }
                pskwNodeStart = pskwNode[__SHELL_LINE_PER_SCREEN - 1];
            }
        
        } while ((ulGetNum == __SHELL_LINE_PER_SCREEN) && 
                 (pskwNodeStart != LW_NULL));

    } else {
        PCHAR   pcKeyword = LW_NULL;
    
        if (iArgC == 2) {
            pcKeyword = ppcArgV[1];
        } else if (iArgC == 3) {
            if (lib_strcmp("-s", ppcArgV[1])) {
                printf("option error.\n");
                return  (-1);
            }
            pcKeyword = ppcArgV[2];
        } else {
            return  (-1);
        }
        
        if (ERROR_NONE == 
            __tshellKeywordFind(pcKeyword, &pskwNode[0])) {
            if (pskwNode[0]->SK_pcHelpString) {
                printf("%s", pskwNode[0]->SK_pcHelpString);             /*  打印帮助信息                */
                printf("%s", pskwNode[0]->SK_pcKeyword);                /*  打印关键字队列              */
                if (pskwNode[0]->SK_pcFormatString) {
                    API_TShellCtlCharSend(__LW_VT100_FUNC_COLOR, 
                                          __LW_VT100_COLOR_PURPLE);     /*  设置品红前景                */
                    printf("%s", pskwNode[0]->SK_pcFormatString);       /*  打印格式信息                */
                    API_TShellCtlCharSend(__LW_VT100_FUNC_COLOR, 
                                          __LW_VT100_COLOR_GREEN);      /*  设置绿色前景                */
                    printf("\n");
                }
            } else {
                return  (-ERROR_TSHELL_EKEYWORD);
            }
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdFree
** 功能描述: 系统命令 "free"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdFree (INT  iArgC, PCHAR  ppcArgV[])
{
    if (iArgC != 2) {
        printf("argument error.\n");
        return  (-1);
    }
    
    if (ERROR_NONE == API_TShellVarDelete(ppcArgV[1])) {
        return  (ERROR_NONE);
    } else {
        return  (-ERROR_TSHELL_EVAR);
    }
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdSem
** 功能描述: 系统命令 "sem"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdSem (INT  iArgC, PCHAR  ppcArgV[])
{
#if ((LW_CFG_SEMB_EN > 0) || (LW_CFG_SEMC_EN > 0) || (LW_CFG_SEMM_EN > 0)) && (LW_CFG_MAX_EVENTS > 0)
    LW_OBJECT_HANDLE        ulId = LW_OBJECT_HANDLE_INVALID;

    if (iArgC != 2) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    sscanf(ppcArgV[1], "%lx", &ulId);
    
    API_SemaphoreShow(ulId);
#endif                                                                  /*  ((LW_CFG_SEMB_EN > 0) ||    */
                                                                        /*   (LW_CFG_SEMC_EN > 0) ||    */
                                                                        /*   (LW_CFG_SEMM_EN > 0)) &&   */
                                                                        /*  (LW_CFG_MAX_EVENTS > 0)     */
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdPart
** 功能描述: 系统命令 "part"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdPart (INT  iArgC, PCHAR  ppcArgV[])
{
#if (LW_CFG_PARTITION_EN > 0) && (LW_CFG_MAX_PARTITIONS > 0)
    LW_OBJECT_HANDLE        ulId = LW_OBJECT_HANDLE_INVALID;

    if (iArgC != 2) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    sscanf(ppcArgV[1], "%lx", &ulId);
    
    API_PartitionShow(ulId);
#endif                                                                  /*  LW_CFG_PARTITION_EN > 0     */
                                                                        /*  (LW_CFG_MAX_PARTITIONS > 0) */
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdEventSet
** 功能描述: 系统命令 "eventset"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdEventSet (INT  iArgC, PCHAR  ppcArgV[])
{
#if LW_CFG_FIO_LIB_EN > 0 && LW_CFG_EVENTSET_EN > 0
    LW_OBJECT_HANDLE        ulId = LW_OBJECT_HANDLE_INVALID;

    if (iArgC != 2) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    sscanf(ppcArgV[1], "%lx", &ulId);
    
    API_EventSetShow(ulId);
#endif                                                                  /*  LW_CFG_FIO_LIB_EN > 0       */
                                                                        /*  LW_CFG_EVENTSET_EN > 0      */
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdMsgq
** 功能描述: 系统命令 "msgq"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdMsgq (INT  iArgC, PCHAR  ppcArgV[])
{
#if LW_CFG_FIO_LIB_EN > 0 && LW_CFG_MSGQUEUE_EN > 0
    LW_OBJECT_HANDLE        ulId = LW_OBJECT_HANDLE_INVALID;

    if (iArgC != 2) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    sscanf(ppcArgV[1], "%lx", &ulId);
    
    API_MsgQueueShow(ulId);
#endif                                                                  /*  LW_CFG_FIO_LIB_EN > 0       */
                                                                        /*  LW_CFG_MSGQUEUE_EN > 0      */
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdTs
** 功能描述: 系统命令 "ts"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdTs (INT  iArgC, PCHAR  ppcArgV[])
{
    pid_t   pid = 0;

    if (iArgC < 2) {
        API_ThreadShow();
        return  (ERROR_NONE);
    }
    
    if (sscanf(ppcArgV[1], "%d", &pid) != 1) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }

    API_ThreadShowEx(pid);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdSs
** 功能描述: 系统命令 "ss"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdSs (INT  iArgC, PCHAR  ppcArgV[])
{
    API_StackShow();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdCpuus
** 功能描述: 系统命令 "cpuus"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdCpuus (INT  iArgC, PCHAR  ppcArgV[])
{
    INT     iTime  = 1;
    INT     iTimes = 1;
    INT     i;
    
    if (iArgC >= 3) {
        for (i = 1; i < iArgC - 1; i += 2) {
            if (lib_strcmp(ppcArgV[i], "-n") == 0) {
                sscanf(ppcArgV[i + 1], "%d", &iTimes);                  /*  指定显示次数                */
            
            } else if (lib_strcmp(ppcArgV[i], "-t") == 0) {
                sscanf(ppcArgV[i + 1], "%d", &iTime);                   /*  检测时间                    */
            }
        }
    }

    API_CPUUsageShow(iTime, iTimes);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdInts
** 功能描述: 系统命令 "ints"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdInts (INT  iArgC, PCHAR  ppcArgV[])
{
    API_InterShow();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdMems
** 功能描述: 系统命令 "mems"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdMems (INT  iArgC, PCHAR  ppcArgV[])
{
    LW_OBJECT_HANDLE   ulId = LW_OBJECT_HANDLE_INVALID;
    
    if (iArgC == 1) {
        
        API_RegionShow(0);
    
    } else if (iArgC == 2) {
        
        if (ppcArgV[1][0] < '0' ||
            ppcArgV[1][0] > '9') {
            printf("option error.\n");
            return  (-1);
        }
        sscanf(ppcArgV[1], "%lx", &ulId);
        
        API_RegionShow(ulId);
    } else {
        printf("option error.\n");
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdKill
** 功能描述: 系统命令 "kill"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdKill (INT  iArgC, PCHAR  ppcArgV[])
{
    LW_OBJECT_HANDLE  ulId    = LW_OBJECT_HANDLE_INVALID;
    INT               iSigNum = SIGTERM;

    if (iArgC == 2) {
        
        if (ppcArgV[1][0] < '0' ||
            ppcArgV[1][0] > '9') {
            printf("option error.\n");
            return  (-1);
        }
        
        sscanf(ppcArgV[1], "%lx", &ulId);
        if (API_ObjectGetClass(ulId) != _OBJECT_THREAD) {
            sscanf(ppcArgV[1], "%ld", &ulId);                           /*  进程 id                     */
        }
        return  (kill(ulId, SIGTERM));
    
    } else if (iArgC == 4) {
        
        if (lib_strcmp(ppcArgV[1], "-n")) {
            printf("option error.\n");
            return  (-1);
        }
        
        sscanf(ppcArgV[2], "%d",  &iSigNum);
        sscanf(ppcArgV[3], "%lx", &ulId);
        if (API_ObjectGetClass(ulId) != _OBJECT_THREAD) {
            sscanf(ppcArgV[3], "%ld", &ulId);                           /*  进程 id                     */
        }
        return  (kill(ulId, iSigNum));
    
    } else {
        
        printf("option error.\n");
        return  (-1);
    }
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdSigqueue
** 功能描述: 系统命令 "sigqueue"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdSigqueue (INT  iArgC, PCHAR  ppcArgV[])
{
    LW_OBJECT_HANDLE  ulId    = LW_OBJECT_HANDLE_INVALID;
    INT               iSigNum = SIGTERM;
    union sigval      sigvalue;
    
    sigvalue.sival_int = 0;
    
    if (iArgC == 2) {
        
        if (ppcArgV[1][0] < '0' ||
            ppcArgV[1][0] > '9') {
            printf("option error.\n");
            return  (-1);
        }
        
        sscanf(ppcArgV[1], "%lx", &ulId);
        if (API_ObjectGetClass(ulId) != _OBJECT_THREAD) {
            sscanf(ppcArgV[1], "%ld", &ulId);                           /*  进程 id                     */
        }
        return  (sigqueue(ulId, SIGTERM, sigvalue));
    
    } else if (iArgC == 4) {
        
        if (lib_strcmp(ppcArgV[1], "-n")) {
            printf("option error.\n");
            return  (-1);
        }
        
        sscanf(ppcArgV[2], "%d",  &iSigNum);
        sscanf(ppcArgV[3], "%lx", &ulId);
        if (API_ObjectGetClass(ulId) != _OBJECT_THREAD) {
            sscanf(ppcArgV[1], "%ld", &ulId);                           /*  进程 id                     */
        }
        return  (sigqueue(ulId, iSigNum, sigvalue));
    
    } else {
        
        printf("option error.\n");
        return  (-1);
    }
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdTimes
** 功能描述: 系统命令 "times"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdTimes (INT  iArgC, PCHAR  ppcArgV[])
{
    BOOL        bIsUTC = LW_FALSE;
    time_t      tim;
    struct tm   tmTime;
    CHAR        cTimeBuffer[32];
    
    lib_time(&tim);
    
    if (iArgC == 2) {
        if (lib_strcmp(ppcArgV[1], "-utc") == 0) {
            bIsUTC = LW_TRUE;
        }
    }
    
    if (bIsUTC) {
        lib_gmtime_r(&tim, &tmTime);
        printf("UTC : ");
    } else {
        lib_localtime_r(&tim, &tmTime);
    }
    
    printf("%s\n", lib_asctime_r(&tmTime, cTimeBuffer));
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdTzsync
** 功能描述: 系统命令 "tzsync"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdTzsync (INT  iArgC, PCHAR  ppcArgV[])
{
    lib_tzset();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdDate
** 功能描述: 系统命令 "date"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdDate (INT  iArgC, PCHAR  ppcArgV[])
{
    struct timespec     tv;
    time_t              tim;
    struct tm           tmTime;
    CHAR                cTimeBuffer[32];
    
    lib_time(&tim);                                                     /*  获得当前系统时间            */
    lib_localtime_r(&tim, &tmTime);

    if (iArgC == 3) {
        if ((lib_strcmp(ppcArgV[1], "-s") == 0) ||
            (lib_strcmp(ppcArgV[1], "--set") == 0)) {
            if (lib_index(ppcArgV[2], ':')) {                           /*  时间设置                    */
                if (sscanf(ppcArgV[2], "%d:%d:%d", &tmTime.tm_hour,
                                                   &tmTime.tm_min,
                                                   &tmTime.tm_sec) > 0) {
                                                                        /*  至少要有一个时间参数        */
                    tv.tv_sec  = lib_timegm(&tmTime) + timezone;        /*  UTC = LOCAL + timezone      */
                    tv.tv_nsec = 0;
                    lib_clock_settime(CLOCK_REALTIME, &tv);
                    
                } else {
                    printf("invalid time '%s'.\n", ppcArgV[2]);
                    return  (PX_ERROR);
                }
            
            } else {                                                    /*  日期设置                    */
                if (lib_strnlen(ppcArgV[2], 10) == 8) {
                    PCHAR       pcDate = ppcArgV[2];
                    INT         i;
                    
                    for (i = 0; i < 8; i++) {
                        if (lib_isdigit(pcDate[i]) == 0) {
                            goto    __invalid_date;
                        }
                    }
                    
                    tmTime.tm_year = ((pcDate[0] - '0') * 1000)
                                   + ((pcDate[1] - '0') * 100)
                                   + ((pcDate[2] - '0') * 10)
                                   +  (pcDate[3] - '0');                /*  读取 year 信息              */
                    if (tmTime.tm_year >  1900) {
                        tmTime.tm_year -= 1900;
                    } else {
                        goto    __invalid_date;
                    }
                    
                    tmTime.tm_mon = ((pcDate[4] - '0') * 10)
                                  +  (pcDate[5] - '0');                 /*  读取 month 信息             */
                    if ((tmTime.tm_mon) > 0 && (tmTime.tm_mon < 13)) {
                        tmTime.tm_mon -= 1;
                    } else {
                        goto    __invalid_date;
                    }
                    
                    tmTime.tm_mday = ((pcDate[6] - '0') * 10)
                                   +  (pcDate[7] - '0');                /*  读取 mday 信息              */
                    if ((tmTime.tm_mday < 1) || (tmTime.tm_mday > 31)) {
                        goto    __invalid_date;
                    }
                    
                    tv.tv_sec  = lib_timegm(&tmTime) + timezone;        /*  UTC = LOCAL + timezone      */
                    tv.tv_nsec = 0;
                    lib_clock_settime(CLOCK_REALTIME, &tv);
                
                } else {
__invalid_date:
                    printf("invalid date '%s'.\n", ppcArgV[2]);
                    return  (PX_ERROR);
                }
            }
        } else {
            printf("option error.\n");
            return  (PX_ERROR);
        }
        
        lib_time(&tim);                                                 /*  获得当前系统时间            */
        lib_localtime_r(&tim, &tmTime);
    }
    
    
    printf("%s\n", lib_asctime_r(&tmTime, cTimeBuffer));
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdDrvs
** 功能描述: 系统命令 "drvs"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdDrvs (INT  iArgC, PCHAR  ppcArgV[])
{
    API_IoDrvShow();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdDrvlics
** 功能描述: 系统命令 "drvlics"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdDrvlics (INT  iArgC, PCHAR  ppcArgV[])
{
    API_IoDrvLicenseShow();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdDevs
** 功能描述: 系统命令 "devs"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdDevs (INT  iArgC, PCHAR  ppcArgV[])
{
    INT     iShowType = 0;

    if (iArgC > 1) {
        if (lib_strncmp(ppcArgV[1], "-a", 3) == 0) {
            iShowType = 1;
        }
    }
    API_IoDevShow(iShowType);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdBuss
** 功能描述: 系统命令 "buss"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdBuss (INT  iArgC, PCHAR  ppcArgV[])
{
    API_BusShow();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdFiles
** 功能描述: 系统命令 "files"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdFiles (INT  iArgC, PCHAR  ppcArgV[])
{
    API_IoFdShow();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdFdentrys
** 功能描述: 系统命令 "fdentrys"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdFdentrys (INT  iArgC, PCHAR  ppcArgV[])
{
    API_IoFdentryShow();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdOpen
** 功能描述: 系统命令 "open"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdOpen (INT  iArgC, PCHAR  ppcArgV[])
{
    INT          iFd;
    INT          iFlag = O_RDONLY;
    INT          iMode = DEFAULT_FILE_PERM;
    struct stat  statBuf;
    
#if LW_CFG_CPU_WORD_LENGHT == 64
#define __PRINT_TYPE    "%llx"
#else
#define __PRINT_TYPE    "%lx"
#endif                                                                  /*  LW_CFG_CPU_WORD_LENGHT      */
    
    if (iArgC < 2) {
        return  (PX_ERROR);
    }
    
    if (iArgC == 3) {
        sscanf(ppcArgV[2], "%d", &iFlag);
    } else if (iArgC > 3) {
        sscanf(ppcArgV[2], "%x", &iFlag);                               /*  16 进制                     */
        sscanf(ppcArgV[3], "%o", &iMode);                               /*  8 进制                      */
    }
    
    iFd = open(ppcArgV[1], iFlag, iMode);
    if (iFd >= 0) {
        fstat(iFd, &statBuf);
        
        printf("open file return : %d dev " __PRINT_TYPE " inode " __PRINT_TYPE " size %llx\n", 
               iFd, statBuf.st_dev, statBuf.st_ino, statBuf.st_size);
    } else {
        printf("open file return : %d error : %s\n", iFd, lib_strerror(errno));
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdClose
** 功能描述: 系统命令 "close"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdClose (INT  iArgC, PCHAR  ppcArgV[])
{
    INT     iFd = PX_ERROR;

    if (iArgC != 2) {
        return  (PX_ERROR);
    } 
    
    sscanf(ppcArgV[1], "%d", &iFd);
    if (iFd >= 0) {
        close(iFd);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdPms
** 功能描述: 系统命令 "pms"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if (LW_CFG_POWERM_EN > 0) && (LW_CFG_MAX_POWERM_NODES > 0)

static INT  __tshellSysCmdPms (INT  iArgC, PCHAR  ppcArgV[])
{
    API_PowerMShow();
    
    return  (ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_POWERM_EN            */
                                                                        /*  LW_CFG_MAX_POWERM_NODES     */
/*********************************************************************************************************
** 函数名称: __tshellSysCmdClear
** 功能描述: 系统命令 "clear"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdClear (INT  iArgC, PCHAR  ppcArgV[])
{
    API_TShellCtlCharSend(1, LW_NULL);                                  /*  发送清屏指令                */
    API_TShellCtlCharSend(__LW_VT100_FUNC_COLOR, 
                          __LW_VT100_COLOR_GREEN);                      /*  设置前景为绿色              */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdExit
** 功能描述: 系统命令 "exit"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdExit (INT  iArgC, PCHAR  ppcArgV[])
{
    exit(0);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdVars
** 功能描述: 系统命令 "vars"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdVars (INT  iArgC, PCHAR  ppcArgV[])
{
    PCHAR   pcTitle = "\n"
                      "       VARIABLE      REF                       VALUE\n"
                      "-------------------- --- --------------------------------------------------\n";
    __PTSHELL_VAR   pskvNode[100];
    __PTSHELL_VAR   pskvNodeStart = LW_NULL;
    ULONG           ulNum;
    INT             i;
                      
    printf("variable show >>\n");
    printf(pcTitle);
    
    /*
     *  在扫描显示的瞬间, 不能创建与删除变量.
     */
    do {
        ulNum = __tshellVarList(pskvNodeStart, pskvNode, 100);
        for (i = 0; i < ulNum; i++) {
            PCHAR  pcRef = "";
            if (pskvNode[i]->SV_ulRefCnt) {
                pcRef = "YES";
            }
            printf("%-20s %-3s %-50s\n", 
                   pskvNode[i]->SV_pcVarName, pcRef,
                   ((pskvNode[i]->SV_pcVarValue) ? (pskvNode[i]->SV_pcVarValue) : ("")));
        }
        pskvNodeStart = pskvNode[ulNum - 1];
    } while (ulNum >= 100);
    
    printf("\n");

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdVarload
** 功能描述: 系统命令 "varload"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdVarload (INT  iArgC, PCHAR  ppcArgV[])
{
    INT     iError;
    PCHAR   pcFile;

    if (iArgC < 2) {
        pcFile = __TTINY_SHELL_VAR_FILE;
    } else {
        pcFile = ppcArgV[1];
    }
    
    iError = __tshellVarLoad(pcFile);
    
    if (iError == ERROR_NONE) {
        printf("envionment variables load from %s success.\n", pcFile);
    } else {
        printf("envionment variables load from %s fail, error : %s\n", pcFile, lib_strerror(errno));
    }
    
    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdVarsave
** 功能描述: 系统命令 "varsave"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdVarsave (INT  iArgC, PCHAR  ppcArgV[])
{
    INT     iError;
    PCHAR   pcFile;
    
    if (iArgC < 2) {
        pcFile = __TTINY_SHELL_VAR_FILE;
    } else {
        pcFile = ppcArgV[1];
    }
    
    iError = __tshellVarSave(pcFile);
    
    if (iError == ERROR_NONE) {
        printf("envionment variables save to %s success.\n", pcFile);
    } else {
        printf("envionment variables save to %s fail, error : %s\n", pcFile, lib_strerror(errno));
    }
    
    return  (iError);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdSync
** 功能描述: 系统命令 "sync"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdSync (INT  iArgC, PCHAR  ppcArgV[])
{
    sync();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdTty
** 功能描述: 系统命令 "tty"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdTty (INT  iArgC, PCHAR  ppcArgV[])
{
    if (isatty(STD_OUT)) {
        printf("%s\n", ttyname(STD_OUT));
    } else {
        printf("std file is not a tty device.\n");
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdZones
** 功能描述: 系统命令 "zones"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0

static INT  __tshellSysCmdZones (INT  iArgC, PCHAR  ppcArgV[])
{
    API_VmmPhysicalShow();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdVirtuals
** 功能描述: 系统命令 "virtuals"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdVirtuals (INT  iArgC, PCHAR  ppcArgV[])
{
    API_VmmVirtualShow();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdAborts
** 功能描述: 系统命令 "aborts"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdAborts (INT  iArgC, PCHAR  ppcArgV[])
{
    API_VmmAbortShow();
    
    return  (ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
** 函数名称: __tshellSysCmdRestart
** 功能描述: 系统命令 "restart"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_THREAD_RESTART_EN > 0

static INT  __tshellSysCmdRestart (INT  iArgC, PCHAR  ppcArgV[])
{
    LW_OBJECT_HANDLE        ulId  = LW_OBJECT_HANDLE_INVALID;
    ULONG                   ulArg = 0ul;

    if (iArgC != 3) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    sscanf(ppcArgV[1], "%lx", &ulId);
    sscanf(ppcArgV[1], "%lu", &ulArg);
    
    if (API_ThreadRestart(ulId, (PVOID)ulArg) == ERROR_NONE) {
        return  (ERROR_NONE);
    } else {
        return  (PX_ERROR);
    }
}

#endif                                                                  /*  LW_CFG_THREAD_RESTART_EN    */
/*********************************************************************************************************
** 函数名称: __tshellSysCmdSprio
** 功能描述: 系统命令 "sprio"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdSprio (INT  iArgC, PCHAR  ppcArgV[])
{
    INT     iPrio = PX_ERROR;
    ULONG   ulId  = LW_OBJECT_HANDLE_INVALID;

    if (iArgC != 3) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    
    sscanf(ppcArgV[1], "%d",  &iPrio);
    sscanf(ppcArgV[2], "%lx", &ulId);
    
    if ((iPrio <  API_KernelGetPriorityMin()) || 
        (iPrio >= API_KernelGetPriorityMax())) {
        printf("priority invalidate.\n");
        return  (PX_ERROR);
    }
    
    if (API_ThreadSetPriority(ulId, (UINT8)iPrio) != ERROR_NONE) {
        printf("can not set thread priority error : %s\n", lib_strerror(errno));
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdRenice
** 功能描述: 系统命令 "renice"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdRenice (INT  iArgC, PCHAR  ppcArgV[])
{
#if LW_CFG_POSIX_EN > 0
    id_t    lId   = 0;
    INT     iNice = 0;
    INT     iPrio;
    INT     iWhich;
    INT     iRet;
    
    struct passwd       passwd;
    struct passwd      *ppasswd = LW_NULL;
    
    CHAR                cUserName[MAX_FILENAME_LENGTH];
    
    if (iArgC < 3) {
__arg_error:
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    
    iNice = lib_atoi(ppcArgV[1]);
    
    if (lib_strcmp(ppcArgV[2], "-p") == 0) {                            /*  PRIO_PROCESS                */
        iWhich = PRIO_PROCESS;
        
        if (iArgC < 4) {
            goto    __arg_error;
        }
        
        lId = lib_atol(ppcArgV[3]);
        
    } else if (lib_strcmp(ppcArgV[2], "-g") == 0) {                     /*  PRIO_PGRP                   */
        iWhich = PRIO_PGRP;
        
        if (iArgC < 4) {
            goto    __arg_error;
        }
        
        lId = lib_atol(ppcArgV[3]);
        
    } else if (lib_strcmp(ppcArgV[2], "-u") == 0) {                     /*  PRIO_USER                   */
        iWhich = PRIO_USER;
        
        if (iArgC < 4) {
            goto    __arg_error;
        }
        
        getpwnam_r(ppcArgV[3], &passwd, cUserName, sizeof(cUserName), &ppasswd);
        if (ppasswd == LW_NULL) {
            goto    __arg_error;
        }
        
        lId = (id_t)ppasswd->pw_uid;
        
    } else {                                                            /*  PRIO_PROCESS                */
        iWhich = PRIO_PROCESS;
        lId = lib_atol(ppcArgV[2]);
    }
    
    iPrio = getpriority(iWhich, lId);
    iRet  = setpriority(iWhich, lId, iPrio + iNice);
    
    if (iRet < ERROR_NONE) {
        printf("set priority error : %s.\n", lib_strerror(errno));
    }
    return  (iRet);
    
#else
    printf("do not support posix interface.\n");
    return  (PX_ERROR);
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdLogFileAdd
** 功能描述: 系统命令 "logfileadd"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_LOG_LIB_EN > 0

static INT  __tshellSysCmdLogFileAdd (INT  iArgC, PCHAR  ppcArgV[])
{
    fd_set      fdsetLog;
    INT         iWidth = 0;
    INT         iFdNew = PX_ERROR;

    if (iArgC != 2) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    
    API_LogFdGet(&iWidth, &fdsetLog);                                   /*  获得先前 log 文件集设置     */
    
    sscanf(ppcArgV[1], "%d", &iFdNew);
    if (iFdNew == PX_ERROR) {
        printf("file error.\n");
        return  (PX_ERROR);
    }
    
    FD_SET(iFdNew, &fdsetLog);
    if (iFdNew >= iWidth) {                                             /*  判断文件集最大文件编号      */
        iWidth = iFdNew + 1;
    }
    
    API_LogFdSet(iWidth, &fdsetLog);                                    /*  设置新的文件集              */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdLogFileClear
** 功能描述: 系统命令 "logfileclear"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdLogFileClear (INT  iArgC, PCHAR  ppcArgV[])
{
    fd_set      fdsetLog;
    INT         iWidth = 0;
    INT         iFdNew = PX_ERROR;

    if (iArgC != 2) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    
    API_LogFdGet(&iWidth, &fdsetLog);                                   /*  获得先前 log 文件集设置     */
    
    sscanf(ppcArgV[1], "%d", &iFdNew);
    if (iFdNew == PX_ERROR) {
        printf("file error.\n");
        return  (PX_ERROR);
    }
    
    FD_CLR(iFdNew, &fdsetLog);
    
    /*
     *  暂不处理 Width.
     */
    API_LogFdSet(iWidth, &fdsetLog);                                    /*  设置新的文件集              */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdLogFiles
** 功能描述: 系统命令 "logfiles"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdLogFiles (INT  iArgC, PCHAR  ppcArgV[])
{
#define __LW_LOG_FD_EACH_LINE_NUM       5

             fd_set     fdsetLog;
             INT        iWidth = 0;
             INT        iFdCounter = 0;
    REGISTER INT        iFdTemp;
    REGISTER ULONG      ulPartMask;
    
    API_LogFdGet(&iWidth, &fdsetLog);                                   /*  获得先前 log 文件集设置     */
    
    printf("log fd(s) include : \n");
    for (iFdTemp = 0; iFdTemp < iWidth; iFdTemp++) {                    /*  检查所有可执行读操作的文件  */
        ulPartMask = 
        fdsetLog.fds_bits[((unsigned)iFdTemp) / NFDBITS];               /*  获得 iFdTemp 所在的掩码组   */
        
        if (ulPartMask == 0) {                                          /*  这个组中与这个文件无关      */
            iFdTemp += NFDBITS - 1;                                     /*  进行下一个掩码组判断        */
        } else if (ulPartMask & (ULONG)(1 << (((unsigned)iFdTemp) % NFDBITS))) {
            iFdCounter++;
            if (iFdCounter > __LW_LOG_FD_EACH_LINE_NUM) {
                printf("%5d\n", iFdTemp);
            } else {
                printf("%5d    ", iFdTemp);
            }
        }
    }
    printf("\n");
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdLogLevel
** 功能描述: 系统命令 "loglevel"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdLogLevel (INT  iArgC, PCHAR  ppcArgV[])
{
    INT     iNewLogLevel = console_loglevel;

    if (iArgC != 2) {
        printf("printk() log message current level is : %d\n", console_loglevel);
        return  (ERROR_NONE);
    }
    
    sscanf(ppcArgV[1], "%d", &iNewLogLevel);
    if (iNewLogLevel != console_loglevel) {
        console_loglevel = iNewLogLevel;                                /*  重新设置 printk 打印等级    */
    }
    
    return  (ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_LOG_LIB_EN > 0       */
/*********************************************************************************************************
** 函数名称: __tshellSysCmdHwclock
** 功能描述: 系统命令 "hwclock"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LW_CFG_RTC_EN > 0

static INT  __tshellSysCmdHwclock (INT  iArgC, PCHAR  ppcArgV[])
{
    time_t      time;
    CHAR        cTimeBuffer[32];

    if (iArgC != 2) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    
    if (lib_strcmp("--show", ppcArgV[1]) == 0) {                        /*  显示硬件时钟                */
        if (API_RtcGet(&time) < 0) {
            printf("can not get RTC info. error : %s\n", lib_strerror(errno));
            return  (PX_ERROR);
        } else {
            printf("%s\n", lib_ctime_r(&time, cTimeBuffer));
        }
    
    } else if (lib_strcmp("--hctosys", ppcArgV[1]) == 0) {              /*  硬件时钟与系统时间同步      */
        if (API_RtcToSys() < 0) {
            printf("can not sync RTC to system clock. error : %s\n", lib_strerror(errno));
            return  (PX_ERROR);
        }
    
    } else if (lib_strcmp("--systohc", ppcArgV[1]) == 0) {              /*  系统时间与硬件时钟同步      */
        if (API_SysToRtc() < 0) {
            printf("can not sync system clock to RTC. error : %s\n", lib_strerror(errno));
            return  (PX_ERROR);
        }
    
    } else {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}

#endif                                                                  /*  LW_CFG_RTC_EN > 0           */
/*********************************************************************************************************
** 函数名称: __tshellSysCmdShStack
** 功能描述: 系统命令 "shstack"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdShStack (INT  iArgC, PCHAR  ppcArgV[])
{
    size_t  stSize = 0;

    if (iArgC < 2) {
        API_TShellSetStackSize(0, &stSize);
        printf("default shell stack : %zd\n", stSize);
        
    } else {
        sscanf(ppcArgV[1], "%zd", &stSize);
        API_TShellSetStackSize(stSize, LW_NULL);
        printf("default shell stack : %zd\n", stSize);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdCrypt
** 功能描述: 系统命令 "crypt"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdCrypt (INT  iArgC, PCHAR  ppcArgV[])
{
    PCHAR   pcRes;
    CHAR    cResBuf[128];

    if (iArgC != 3) {
        printf("argument error.\n");
        return  (PX_ERROR);
    }
    
    pcRes = crypt_safe(ppcArgV[1], ppcArgV[2], cResBuf, sizeof(cResBuf));
    if (pcRes) {
        printf("%s\n", pcRes);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdHostname
** 功能描述: 系统命令 "hostname"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdHostname (INT  iArgC, PCHAR  ppcArgV[])
{
    CHAR    cHostName[HOST_NAME_MAX + 1];

    if (iArgC < 2) {
        gethostname(cHostName, sizeof(cHostName));
        printf("hostname is %s\n", cHostName);
    } else {
        if (sethostname(ppcArgV[1], lib_strlen(ppcArgV[1]))) {
            printf("sethostname error : %s\n", lib_strerror(errno));
            return  (PX_ERROR);
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdLogin
** 功能描述: 系统命令 "login"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdLogin (INT  iArgC, PCHAR  ppcArgV[])
{
    __tshellUserAuthen(STDIN_FILENO);
    
    printf("\n");
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdWho
** 功能描述: 系统命令 "who"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdWho (INT  iArgC, PCHAR  ppcArgV[])
{
    CHAR        cUserName[MAX_FILENAME_LENGTH];

    struct passwd   passwd;
    struct passwd  *ppasswd = LW_NULL;
    
    getpwuid_r(getuid(), &passwd, cUserName, sizeof(cUserName), &ppasswd);
    if (ppasswd) {
        printf("user:%s terminal:%s uid:%d gid:%d euid:%d egid:%d\n",
               ppasswd->pw_name,
               ttyname(STD_OUT),
               getuid(),
               getgid(),
               geteuid(),
               getegid());
    } else {
        printf("user:%s terminal:%s uid:%d gid:%d euid:%d egid:%d\n",
               "unknown",
               ttyname(STD_OUT),
               getuid(),
               getgid(),
               geteuid(),
               getegid());
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdShutdown
** 功能描述: 系统命令 "shutdown"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdShutdown (INT  iArgC, PCHAR  ppcArgV[])
{
    if (iArgC < 2) {
        API_KernelReboot(LW_REBOOT_SHUTDOWN);
        
    } else {
        if (lib_strcmp(ppcArgV[1], "-n")) {
            API_KernelReboot(LW_REBOOT_FORCE);
        
        } else if (lib_strcmp(ppcArgV[1], "-r")) {
            API_KernelReboot(LW_REBOOT_COLD);
        
        } else if (lib_strcmp(ppcArgV[1], "-h")) {
            API_KernelReboot(LW_REBOOT_POWEROFF);
        }
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdMonitor
** 功能描述: 系统命令 "monitor"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdMonitor (INT  iArgC, PCHAR  ppcArgV[])
{
    static PVOID    pvMonitor = LW_NULL;
    static BOOL     bIsNet;
    static CHAR     pcFileOrIp[MAX_FILENAME_LENGTH];
    static UINT16   usPort;
    static UINT64   u64SubEventAllow[MONITOR_EVENT_ID_MAX + 1];         /*  事件滤波器                  */
    
           PCHAR    pcDiv;
    
    if (iArgC == 1) {
        if (pvMonitor == LW_NULL) {
            printf("monitor stopped now.\n");
        } else {
            if (bIsNet) {
                printf("monitor update ip : %s port : %d\n", pcFileOrIp, usPort);
            } else {
                printf("monitor update path : %s\n", pcFileOrIp);
            }
        }
        return  (ERROR_NONE);
    }
    
    if (lib_strcmp(ppcArgV[1], "start") == 0) {                         /*  启动监控器                  */
        INT     i;
        
        if (pvMonitor) {
            printf("monitor has been already started.\n");
            return  (ERROR_NONE);
        }
        if (iArgC < 3) {
            goto    __inval_args;
        }
        
        pcDiv = lib_strchr(ppcArgV[2], ':');
        
        if (pcDiv) {                                                    /*  网络 upload                 */
            lib_memcpy(pcFileOrIp, ppcArgV[2], pcDiv - ppcArgV[2]);
            pcFileOrIp[pcDiv - ppcArgV[2]] = PX_EOS;
            usPort = (UINT16)lib_atoi(&pcDiv[1]);
            
            if (!usPort) {
                goto    __inval_args;
            }
            
            pvMonitor = API_MonitorNetUploadCreate(pcFileOrIp, usPort,
                                                   16 * LW_CFG_KB_SIZE, LW_NULL);
            if (!pvMonitor) {
                printf("can not create monitor net upload path error : %s.\n", lib_strerror(errno));
                return  (PX_ERROR);
            }
            bIsNet = LW_TRUE;
            
        } else {
            lib_strlcpy(pcFileOrIp, ppcArgV[2], MAX_FILENAME_LENGTH);
        
            pvMonitor = API_MonitorFileUploadCreate(pcFileOrIp, O_WRONLY | O_CREAT | O_APPEND, 0666,
                                                    16 * LW_CFG_KB_SIZE, 0, LW_NULL);
            if (!pvMonitor) {
                printf("can not create monitor file upload path error : %s.\n", lib_strerror(errno));
                return  (PX_ERROR);
            }
            bIsNet = LW_FALSE;
        }
        
        for (i = 0; i < MONITOR_EVENT_ID_MAX; i++) {
            API_MonitorUploadSetFilter(pvMonitor, (UINT32)i, u64SubEventAllow[i], 
                                       LW_MONITOR_UPLOAD_SET_EVT_SET);
        }
        
        API_MonitorUploadEnable(pvMonitor);
        
        printf("monitor start.\n");
        return  (ERROR_NONE);
    
    } else if (lib_strcmp(ppcArgV[1], "stop") == 0) {                   /*  停止监控器                  */
        if (!pvMonitor) {
            printf("monitor has been already stopped.\n");
            return  (ERROR_NONE);
        }
        
        if (bIsNet) {
            if (API_MonitorNetUploadDelete(pvMonitor)) {
                printf("can not stop net upload monitor error : %s.\n", lib_strerror(errno));
                return  (PX_ERROR);
            
            } else {
                pvMonitor = LW_NULL;
            }
        
        } else {
            if (API_MonitorFileUploadDelete(pvMonitor)) {
                printf("can not stop file upload monitor error : %s.\n", lib_strerror(errno));
                return  (PX_ERROR);
            
            } else {
                pvMonitor = LW_NULL;
            }
        }
        
        printf("monitor stop.\n");
        return  (ERROR_NONE);
    
    } else if (lib_strcmp(ppcArgV[1], "filter") == 0) {                 /*  设置监控器滤波器            */
        UINT    uiEvent  = MONITOR_EVENT_ID_MAX + 1;
        UINT64  u64Allow = 0;
        
        if (iArgC < 3) {
            INT     i;
            INT     iNum = 0;
            for (i = 0; i < MONITOR_EVENT_ID_MAX; i++) {
                if (u64SubEventAllow[i]) {
                    printf("%d : %016llx ", i, u64SubEventAllow[i]);
                    iNum++;
                    if (iNum > 1) {
                        iNum = 0;
                        printf("\n");
                    }
                }
            }
            printf("\n");
            return  (ERROR_NONE);
        }
        
        if (iArgC < 4) {
            goto    __inval_args;
        }
        
        if (sscanf(ppcArgV[2], "%u", &uiEvent) != 1) {
            goto    __inval_args;
        }
        
        if (sscanf(ppcArgV[3], "%llx", &u64Allow) != 1) {
            goto    __inval_args;
        }
        
        if (uiEvent > MONITOR_EVENT_ID_MAX) {
            goto    __inval_args;
        }
        
        u64SubEventAllow[uiEvent] = u64Allow;
        
        if (pvMonitor) {
            API_MonitorUploadSetFilter(pvMonitor, (UINT32)uiEvent, u64Allow,
                                       LW_MONITOR_UPLOAD_SET_EVT_SET);
        }
        
        return  (ERROR_NONE);
    }
    
__inval_args:
    printf("argument error.\n");
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdLspci
** 功能描述: 系统命令 "lspci"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellSysCmdLspci (INT  iArgC, PCHAR  ppcArgV[])
{
    INT     iFd;
    CHAR    cBuffer[512];
    ssize_t sstNum;
    
    iFd = open("/proc/pci", O_RDONLY);
    if (iFd < 0) {
        printf("can not open /proc/pci!\n");
        return  (PX_ERROR);
    }
    
    do {
        sstNum = read(iFd, cBuffer, sizeof(cBuffer));
        if (sstNum > 0) {
            write(STDOUT_FILENO, cBuffer, (size_t)sstNum);
        }
    } while (sstNum > 0);
    
    close(iFd);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellSysCmdInit
** 功能描述: 初始化系统命令集
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __tshellSysCmdInit (VOID)
{
    API_TShellKeywordAdd("args", __tshellSysCmdArgs);
    API_TShellFormatAdd("args", " [any argument...]");
    API_TShellHelpAdd("args", "show all argument.\n");

    API_TShellKeywordAdd("echo", __tshellSysCmdEcho);
    API_TShellFormatAdd("echo", " [message]");
    API_TShellHelpAdd("echo", "echo the input command.\n"
                              "echo [message]\n");
                              
    API_TShellKeywordAdd("help", __tshellSysCmdHelp);
    API_TShellFormatAdd("help", " [-s keyword]");
    API_TShellHelpAdd("help", "display help information.\n"
                              "help [-s keyword]\n");
                              
    API_TShellKeywordAdd("vars", __tshellSysCmdVars);
    API_TShellHelpAdd("vars", "show current environment.\n");
    
    API_TShellKeywordAdd("env", __tshellSysCmdVars);
    API_TShellHelpAdd("env", "show current environment.\n");
    
    API_TShellKeywordAdd("varload", __tshellSysCmdVarload);
    API_TShellFormatAdd("varload", " [profile]");
    API_TShellHelpAdd("varload", "synchronize envionment variables from profile.\n");
    
    API_TShellKeywordAdd("varsave", __tshellSysCmdVarsave);
    API_TShellFormatAdd("varsave", " [profile]");
    API_TShellHelpAdd("varsave", "synchronize envionment variables to profile.\n");
                              
    API_TShellKeywordAdd("free", __tshellSysCmdFree);
    API_TShellFormatAdd("free", " [variable]");
    API_TShellHelpAdd("free", "delete a variable.\n");
                              
    API_TShellKeywordAdd("msgq", __tshellSysCmdMsgq);
    API_TShellFormatAdd("msgq", " message queue handle");
    API_TShellHelpAdd("msgq", "show message queue information.\n");
    
    API_TShellKeywordAdd("eventset", __tshellSysCmdEventSet);
    API_TShellFormatAdd("eventset", " eventset handle");
    API_TShellHelpAdd("eventset", "show eventset information.\n");
    
    API_TShellKeywordAdd("part", __tshellSysCmdPart);
    API_TShellFormatAdd("part", " partition handle");
    API_TShellHelpAdd("part", "show partition information.\n");
    
    API_TShellKeywordAdd("sem", __tshellSysCmdSem);
    API_TShellFormatAdd("sem", " semaphore handle");
    API_TShellHelpAdd("sem", "show semaphore information.\n");
    
    API_TShellKeywordAdd("ts", __tshellSysCmdTs);
    API_TShellFormatAdd("ts", " [pid]");
    API_TShellHelpAdd("ts", "show thread information.\n"
                            "you can and pid argument to determine which process you want to see.\n");
    
    API_TShellKeywordAdd("ss", __tshellSysCmdSs);
    API_TShellHelpAdd("ss", "show all stack information.\n");
    
    API_TShellKeywordAdd("cpuus", __tshellSysCmdCpuus);
    API_TShellFormatAdd("cpuus", " [-n times] [-t wait_seconds]");
    API_TShellHelpAdd("cpuus", "show cpu usage information, wait_seconds max is 10s.\n");
    
    API_TShellKeywordAdd("ints", __tshellSysCmdInts);
    API_TShellHelpAdd("ints", "show system interrupt vecter information.\n");
    
    API_TShellKeywordAdd("mems", __tshellSysCmdMems);
    API_TShellFormatAdd("mems", " [rid]");
    API_TShellHelpAdd("mems", "show region(heap) information.\n"
                              " if no [rid] is show system heap information.\n"
                              "notice : rid is HEX.\n");
    
    API_TShellKeywordAdd("kill", __tshellSysCmdKill);
    API_TShellFormatAdd("kill", " [-n signum tid/pid] | [tid/pid]");
    API_TShellHelpAdd("kill", "kill a thread or send a signal to a thread.\n"
                              "notice : tid is HEX, pid is DEC.\n");
    
    API_TShellKeywordAdd("sigqueue", __tshellSysCmdSigqueue);
    API_TShellFormatAdd("sigqueue", " [-n signum tid/pid] | [tid/pid]");
    API_TShellHelpAdd("sigqueue", "sigqueue a thread or send a signal to a thread.\n"
                                  "notice : tid is HEX, pid is DEC.\n");
    
    API_TShellKeywordAdd("times", __tshellSysCmdTimes);
    API_TShellFormatAdd("times", " [-utc]");
    API_TShellHelpAdd("times", "show system current time UTC/LOCAL.\n");
    
    API_TShellKeywordAdd("tzsync", __tshellSysCmdTzsync);
    API_TShellHelpAdd("tzsync", "synchronize time zone from environment variables : TZ.\n");
    
    API_TShellKeywordAdd("date", __tshellSysCmdDate);
    API_TShellFormatAdd("date", " [-s {time | date}]");
    API_TShellHelpAdd("date", "set system current time.\n"
                              "eg. date -s 23:28:25\n"
                              "    date -s 20110217\n");
    
    API_TShellKeywordAdd("drvs", __tshellSysCmdDrvs);
    API_TShellHelpAdd("drvs", "show all drivers.\n");
    
    API_TShellKeywordAdd("drvlics", __tshellSysCmdDrvlics);
    API_TShellHelpAdd("drvlics", "show all driver license.\n");
    
    API_TShellKeywordAdd("devs", __tshellSysCmdDevs);
    API_TShellFormatAdd("devs", " [-a]");
    API_TShellHelpAdd("devs", "show all device.\n"
                              "-a   show all information.\n");
    
    API_TShellKeywordAdd("buss", __tshellSysCmdBuss);
    API_TShellHelpAdd("buss", "show all bus info.\n");
    
    API_TShellKeywordAdd("files", __tshellSysCmdFiles);
    API_TShellHelpAdd("files", "show all file.\n");
    
    API_TShellKeywordAdd("fdentrys", __tshellSysCmdFdentrys);
    API_TShellHelpAdd("fdentrys", "show all file entrys.\n");
    
    API_TShellKeywordAdd("open", __tshellSysCmdOpen);
    API_TShellFormatAdd("open", " [filename flag mode]");
    API_TShellHelpAdd("open", "open a file.\n"
                              "filename : file name\n"
                              "flag : O_RDONLY   : 00000000\n"
                              "       O_WRONLY   : 00000001\n"
                              "       O_RDWR     : 00000002\n"
                              "       O_APPEND   : 00000008\n"
                              "       O_SHLOCK   : 00000010\n"
                              "       O_EXLOCK   : 00000020\n"
                              "       O_ASYNC    : 00000040\n"
                              "       O_CREAT    : 00000200\n"
                              "       O_TRUNC    : 00000400\n"
                              "       O_EXCL     : 00000800\n"
                              "       O_SYNC     : 00002000\n"
                              "       O_NONBLOCK : 00004000\n"
                              "       O_NOCTTY   : 00008000\n"
                              "       O_CLOEXEC  : 00080000\n"
                              "mode : create file mode, eg: 0666\n");
    
    API_TShellKeywordAdd("close", __tshellSysCmdClose);
    API_TShellFormatAdd("close", " [fd]");
    API_TShellHelpAdd("close", "close a file.\n");
    
    API_TShellKeywordAdd("sync", __tshellSysCmdSync);
    API_TShellHelpAdd("sync", "flush all system buffer, For example: file system buffer.\n");

    API_TShellKeywordAdd("tty", __tshellSysCmdTty);
    API_TShellHelpAdd("tty", "show tty name.\n");

#if (LW_CFG_POWERM_EN > 0) && (LW_CFG_MAX_POWERM_NODES > 0)
    API_TShellKeywordAdd("pms", __tshellSysCmdPms);
    API_TShellHelpAdd("pms", "show system power management node information.\n");
#endif                                                                  /*  LW_CFG_POWERM_EN            */
                                                                        /*  LW_CFG_MAX_POWERM_NODES     */
    API_TShellKeywordAdd("clear", __tshellSysCmdClear);
    API_TShellHelpAdd("clear", "clear the current screen.\n");
    
    API_TShellKeywordAdd("exit", __tshellSysCmdExit);
    API_TShellHelpAdd("exit", "exit current terminal.\n");
    
#if LW_CFG_VMM_EN > 0
    API_TShellKeywordAdd("zones", __tshellSysCmdZones);
    API_TShellHelpAdd("zones", "show system vmm physical zone information.\n");
    
    API_TShellKeywordAdd("virtuals", __tshellSysCmdVirtuals);
    API_TShellHelpAdd("virtuals", "show system virtuals address space information.\n");
    
    API_TShellKeywordAdd("aborts", __tshellSysCmdAborts);
    API_TShellHelpAdd("aborts", "show system vmm abort statistics information.\n");
#endif                                                                  /*  LW_CFG_VMM_EN > 0           */

#if LW_CFG_THREAD_RESTART_EN > 0
    API_TShellKeywordAdd("restart", __tshellSysCmdRestart);
    API_TShellFormatAdd("restart", " tid argment");
    API_TShellHelpAdd("restart", "restart a thread with specify argment.\n"
                                 "notice : tid is HEX.\n");
#endif                                                                  /*  LW_CFG_THREAD_RESTART_EN    */

    API_TShellKeywordAdd("sprio", __tshellSysCmdSprio);
    API_TShellFormatAdd("sprio", " [priority thread_id]");
    API_TShellHelpAdd("sprio", "set thread priority.\n"
                               "notice : tid is HEX.\n");
                               
    API_TShellKeywordAdd("renice", __tshellSysCmdRenice);
    API_TShellFormatAdd("renice", " priority [[-p] pid ...] [[-g] pgrp ...] [[-u] user ...]");
    API_TShellHelpAdd("renice",  "set process / process group / process user priority.\n");
                               
#if LW_CFG_LOG_LIB_EN > 0
    API_TShellKeywordAdd("logfileadd", __tshellSysCmdLogFileAdd);
    API_TShellFormatAdd("logfileadd", " [file descriptor]");
    API_TShellHelpAdd("logfileadd", "add a file to log file set.\n");
    
    API_TShellKeywordAdd("logfileclear", __tshellSysCmdLogFileClear);
    API_TShellFormatAdd("logfileclear", " [file descriptor]");
    API_TShellHelpAdd("logfileclear", "clear a file from log file set.\n");
    
    API_TShellKeywordAdd("logfiles", __tshellSysCmdLogFiles);
    API_TShellHelpAdd("logfiles", "show all the file(s) in log file set.\n");
    
    API_TShellKeywordAdd("loglevel", __tshellSysCmdLogLevel);
    API_TShellFormatAdd("loglevel", " [level]");
    API_TShellHelpAdd("loglevel", "show or set printk() log level.\n");
#endif                                                                  /*  LW_CFG_LOG_LIB_EN > 0       */
    
#if LW_CFG_RTC_EN > 0
    API_TShellKeywordAdd("hwclock", __tshellSysCmdHwclock);
    API_TShellFormatAdd("hwclock", " [{--show | --hctosys | --systohc}]");
    API_TShellHelpAdd("hwclock", "set/get hardware realtime clock.\n"
                                 "hwclock --show    : show hardware RTC time.\n"
                                 "hwclock --hctosys : sync RTC to system clock.\n"
                                 "hwclock --systohc : sync system clock to RTC.\n");
#endif                                                                  /*  LW_CFG_RTC_EN > 0           */
    API_TShellKeywordAdd("shstack", __tshellSysCmdShStack);
    API_TShellFormatAdd("shstack", " [new stack size]");
    API_TShellHelpAdd("shstack", "show or set sh stack size.\n");
    
    API_TShellKeywordAdd("crypt", __tshellSysCmdCrypt);
    API_TShellFormatAdd("crypt", " [key] [salt]");
    API_TShellHelpAdd("crypt", "crypt() in libcrypt.\n");
    
    API_TShellKeywordAdd("hostname", __tshellSysCmdHostname);
    API_TShellFormatAdd("hostname", " [hostname]");
    API_TShellHelpAdd("hostname", "get or show current machine host name.\n");
    
    API_TShellKeywordAdd("login", __tshellSysCmdLogin);
    API_TShellHelpAdd("login", "change current user.\n");
    
    API_TShellKeywordAdd("who", __tshellSysCmdWho);
    API_TShellHelpAdd("who", "get current user message.\n");
    
    API_TShellKeywordAdd("shutdown", __tshellSysCmdShutdown);
    API_TShellFormatAdd("shutdown", " [shutdown parameter]");
    API_TShellHelpAdd("shutdown", "shutdown or reboot this computer.\n"
                                  "-r    reboot\n"
                                  "-n    reboot and not sync files(do NOT use this)\n"
                                  "-h    shutdown and power off\n"
                                  "no parameter means shutdown only\n");
                                  
    API_TShellKeywordAdd("monitor", __tshellSysCmdMonitor);
    API_TShellFormatAdd("monitor",  " {[start {file | ip:port}] | [stop] | [filter [event allow-mask]]}");
    API_TShellHelpAdd("monitor",    "kernel moniter setting.\n"
                                    "monitor start 192.168.1.1:1234\n"
                                    "monitor start /mnt/nfs/monitor.data\n"
                                    "monitor stop\n"
                                    "monitor filter\n"
                                    "monitor filter 10 1b\n");
                                    
    API_TShellKeywordAdd("lspci", __tshellSysCmdLspci);
    API_TShellHelpAdd("lspci", "show PCI Bus message.\n");
}
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
