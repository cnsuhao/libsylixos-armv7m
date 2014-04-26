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
** 文   件   名: logLib.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 06 月 12 日
**
** 描        述: 系统日志管理系统, 输出函数可以在中断函数或者信号句柄中运行.

** BUG:
2009.04.04  升级日志系统, 支持 logMsg 和 printk 操作, 同时拒绝使用危险的全局缓冲.
2009.04.07  printk 使用数组队列实现缓冲, 这样可以允许 printk 在中断中的处理.
2009.11.03  API_LogPrintk() 在入队之前, 进行一次等级判断.
2012.03.12  使用自动 attr 
*********************************************************************************************************/
#define  __SYLIXOS_STDARG
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  MACRO
*********************************************************************************************************/
#define  __MAX_MSG_LEN         1024                                     /*  系统打印的最长信息长度      */
#define  __MAX_MSG_NUM         LW_CFG_MAX_LOGMSGS                       /*  消息队列消息数量            */
#define  __MAX_ARG             10                                       /*  最多参数数量                */
/*********************************************************************************************************
  printk priority
*********************************************************************************************************/
#define DEFAULT_MESSAGE_LOGLEVEL    4                                   /*  KERN_WARNING                */
#define MINIMUM_CONSOLE_LOGLEVEL    0                                   /*  让用户使用的最小级别        */
#define DEFAULT_CONSOLE_LOGLEVEL    7                                   /*  anything MORE serious than  */
                                                                        /*  KERN_DEBUG                  */
int     console_printk[4] = {
       DEFAULT_CONSOLE_LOGLEVEL,                                        /*  终端级别                    */
       DEFAULT_MESSAGE_LOGLEVEL,                                        /*  默认级别                    */
       MINIMUM_CONSOLE_LOGLEVEL,                                        /*  让用户使用的最小级别        */
       DEFAULT_CONSOLE_LOGLEVEL,                                        /*  默认终端级别                */
};
/*********************************************************************************************************
  CONTRL BLOCK
*********************************************************************************************************/
#if LW_CFG_LOG_LIB_EN > 0

typedef struct {
    CPCHAR                     LOGMSG_pcFormat;                         /*  格式化字串                  */
    
    /*
     *  printk
     */
    PCHAR                      LOGMSG_pcPrintk;                         /*  直接需要打印的字串 printk   */
    int                        LOGMSG_iLevel;                           /*  printk level                */
    
    /*
     *  log messsage
     */
    PVOID                      LOGMSG_pvArg[__MAX_ARG];                 /*  回调参数 logMsg             */
    LW_OBJECT_HANDLE           LOGMSG_ulThreadId;                       /*  发送线程句柄                */
    BOOL                       LOGMSG_bIsNeedHeader;                    /*  是否需要打印头部            */
} LW_LOG_MSG;
typedef LW_LOG_MSG            *PLW_LOG_MSG;
/*********************************************************************************************************
  INTERNAL CLOBAL
*********************************************************************************************************/
LW_OBJECT_HANDLE               _G_hLogMsgHandle;                        /*  LOG 消息队列句柄            */
static INT                     _G_iLogMsgsLost = 0;                     /*  丢失的LOG 消息数量          */
static fd_set                  _G_fdsetLogFd;                           /*  LOG 文件集                  */
static INT                     _G_iMaxWidth;                            /*  最大的文件号 + 1            */
/*********************************************************************************************************
  printk buffer
*********************************************************************************************************/
static CHAR                    _G_cLogPrintkBuffer[__MAX_MSG_NUM][__MAX_MSG_LEN];
static LW_OBJECT_HANDLE        _G_cLogPartition;                        /*  内存缓冲分区                */

#define __LOG_PRINTK_GET_BUFFER()           API_PartitionGet(_G_cLogPartition);
#define __LOG_PRINTK_FREE_BUFFER(pvMem)     API_PartitionPut(_G_cLogPartition, pvMem);
/*********************************************************************************************************
  服务线程
*********************************************************************************************************/
VOID    _LogThread(VOID);                                               /*  LOG 处理程序                */
/*********************************************************************************************************
  INTERNAL FUNC
*********************************************************************************************************/
VOID    __logPrintk(int          iLevel, 
                    PCHAR        pcPrintk);
VOID    __logPrintf(CPCHAR       pcFormat,
                    PVOID        pvArg0,
                    PVOID        pvArg1,
                    PVOID        pvArg2,
                    PVOID        pvArg3,
                    PVOID        pvArg4,
                    PVOID        pvArg5,
                    PVOID        pvArg6,
                    PVOID        pvArg7,
                    PVOID        pvArg8,
                    PVOID        pvArg9);
/*********************************************************************************************************
** 函数名称: API_LogFdSet
** 功能描述: 设置 LOG 需要关心的文件
** 输　入  : iWidth                        最大的文件号 + 1  类似 select() 第一个参数
**           pfdsetLog                     新的文件集
** 输　出  : PX_ERROR or ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_LogFdSet (INT  iWidth, fd_set  *pfdsetLog)
{
    if (!pfdsetLog || !iWidth) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pfdsetLog invalidate.\r\n");
        _ErrorHandle(ERROR_LOG_FDSET_NULL);
        return  (PX_ERROR);
    }
    
    __KERNEL_MODE_PROC(
        _G_fdsetLogFd = *pfdsetLog;
        _G_iMaxWidth  = iWidth;
    );
    
    _ErrorHandle(ERROR_NONE);
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_LogFdGet
** 功能描述: 获得 LOG 需要关心的文件
** 输　入  : piWidth                       最大的文件号 + 1  类似 select() 第一个参数
**           pfdsetLog                     新的文件集
** 输　出  : PX_ERROR or ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_LogFdGet (INT  *piWidth, fd_set  *pfdsetLog)
{
    if (!pfdsetLog || !piWidth) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pfdsetLog invalidate.\r\n");
        _ErrorHandle(ERROR_LOG_FDSET_NULL);
        return  (PX_ERROR);
    }
    
    __KERNEL_MODE_PROC(
        *pfdsetLog = _G_fdsetLogFd;
        *piWidth   = _G_iMaxWidth;
    );
    
    _ErrorHandle(ERROR_NONE);
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_LogPrintk
** 功能描述: 记录格式化日志信息
** 输　入  : pcFormat                   格式化字串
**           ...                        变长字串
** 输　出  : 打印长度
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_LogPrintk (CPCHAR   pcFormat, ...)
{
             va_list       varlist;
             LW_LOG_MSG    logmsg;
    REGISTER INT           iRet;
    REGISTER PCHAR         pcBuffer;
    REGISTER ULONG         ulError;
    
    pcBuffer = (PCHAR)__LOG_PRINTK_GET_BUFFER();
    if (pcBuffer == LW_NULL) {
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (PX_ERROR);
    }
    
    logmsg.LOGMSG_iLevel = PX_ERROR;
    if (lib_strnlen(pcFormat, 3) >= 3) {
        if ((pcFormat[0] == '<') && (pcFormat[2] == '>')) {
            if ((pcFormat[1] <= '9') && (pcFormat[1] >= '0')) {
                logmsg.LOGMSG_iLevel = pcFormat[1] - '0';
            }
        }
    }
    
    if (logmsg.LOGMSG_iLevel == PX_ERROR) {
        logmsg.LOGMSG_iLevel =  default_message_loglevel;               /*  使用默认优先级              */
        if (logmsg.LOGMSG_iLevel > console_loglevel) {
            __LOG_PRINTK_FREE_BUFFER(pcBuffer);
            return  (ERROR_NONE);                                       /*  等级太低, 无法打印          */
        }
        va_start(varlist, pcFormat);
        iRet = vsnprintf(pcBuffer, __MAX_MSG_LEN, pcFormat, varlist);
        va_end(varlist);
    
    } else {
        if (logmsg.LOGMSG_iLevel > console_loglevel) {
            __LOG_PRINTK_FREE_BUFFER(pcBuffer);
            return  (ERROR_NONE);                                       /*  等级太低, 无法打印          */
        }
        va_start(varlist, pcFormat);
        iRet = vsnprintf(pcBuffer, __MAX_MSG_LEN, &pcFormat[3], varlist);
        va_end(varlist);
    }
    
    logmsg.LOGMSG_pcPrintk = pcBuffer;
    logmsg.LOGMSG_pcFormat = pcFormat;
    
    logmsg.LOGMSG_bIsNeedHeader = LW_FALSE;                             /*  不需要打印头部              */
    logmsg.LOGMSG_ulThreadId    = LW_OBJECT_HANDLE_INVALID;
    
    ulError = API_MsgQueueSend(_G_hLogMsgHandle, &logmsg, sizeof(LW_LOG_MSG));
    if (ulError) {
        __LOG_PRINTK_FREE_BUFFER(pcBuffer);
        _G_iLogMsgsLost++;
        _DebugHandle(__ERRORMESSAGE_LEVEL, "log message lost.\r\n");
        _ErrorHandle(ERROR_LOG_LOST);
        return  (PX_ERROR);
    }
    
    _ErrorHandle(ERROR_NONE);
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: API_LogMsg
** 功能描述: 记录格式化日志信息
** 输　入  : pcFormat                   格式化字串
**           pvArg0                     函数参数
**           pvArg1                     函数参数
**           pvArg2                     函数参数
**           pvArg3                     函数参数
**           pvArg4                     函数参数
**           pvArg5                     函数参数
**           pvArg6                     函数参数
**           pvArg7                     函数参数
**           pvArg8                     函数参数
**           pvArg9                     函数参数
**           bIsNeedHeader              是否需要添加 LOG 头信息
** 输　出  : PX_ERROR or ERROR_NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_LogMsg (CPCHAR       pcFormat,
                 PVOID        pvArg0,
                 PVOID        pvArg1,
                 PVOID        pvArg2,
                 PVOID        pvArg3,
                 PVOID        pvArg4,
                 PVOID        pvArg5,
                 PVOID        pvArg6,
                 PVOID        pvArg7,
                 PVOID        pvArg8,
                 PVOID        pvArg9,
                 BOOL         bIsNeedHeader)
{
             LW_LOG_MSG    logmsg;
    REGISTER ULONG         ulError;
    
    if (!pcFormat) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "log message format.\r\n");
        _ErrorHandle(ERROR_LOG_FMT);
        return  (PX_ERROR);
    }
    
    logmsg.LOGMSG_pcPrintk   = LW_NULL;
    logmsg.LOGMSG_pcFormat   = pcFormat;
    logmsg.LOGMSG_pvArg[0]   = pvArg0;
    logmsg.LOGMSG_pvArg[1]   = pvArg1;
    logmsg.LOGMSG_pvArg[2]   = pvArg2;
    logmsg.LOGMSG_pvArg[3]   = pvArg3;
    logmsg.LOGMSG_pvArg[4]   = pvArg4;
    logmsg.LOGMSG_pvArg[5]   = pvArg5;
    logmsg.LOGMSG_pvArg[6]   = pvArg6;
    logmsg.LOGMSG_pvArg[7]   = pvArg7;
    logmsg.LOGMSG_pvArg[8]   = pvArg8;
    logmsg.LOGMSG_pvArg[9]   = pvArg9;
    
    logmsg.LOGMSG_bIsNeedHeader = bIsNeedHeader;
    if (LW_CPU_GET_CUR_NESTING()) {
        logmsg.LOGMSG_ulThreadId = LW_OBJECT_HANDLE_INVALID;
    } else {
        logmsg.LOGMSG_ulThreadId = API_ThreadIdSelf();
    }
    
    ulError = API_MsgQueueSend(_G_hLogMsgHandle, &logmsg, sizeof(LW_LOG_MSG));
    if (ulError) {
        _G_iLogMsgsLost++;
        _DebugHandle(__ERRORMESSAGE_LEVEL, "log message lost.\r\n");
        _ErrorHandle(ERROR_LOG_LOST);
        return  (PX_ERROR);
    }
    
    _ErrorHandle(ERROR_NONE);
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: _logInit
** 功能描述: 初始化 LOG 处理 机制
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
INT  _logInit (VOID)
{
    LW_CLASS_THREADATTR       threadattr;

    FD_ZERO(&_G_fdsetLogFd);                                            /*  清空文件集                  */
    
    _G_hLogMsgHandle = API_MsgQueueCreate("log_msg", 
                                          __MAX_MSG_NUM, 
                                          sizeof(LW_LOG_MSG),
                                          LW_OPTION_WAIT_FIFO | LW_OPTION_OBJECT_GLOBAL, 
                                          LW_NULL);                     /*  建立 Msg Queue              */
    if (!_G_hLogMsgHandle) {
        return  (PX_ERROR);
    }
    
    _G_cLogPartition = API_PartitionCreate("printk_pool", 
                                           (PVOID)_G_cLogPrintkBuffer,
                                           __MAX_MSG_NUM, 
                                           __MAX_MSG_LEN, 
                                           LW_OPTION_OBJECT_GLOBAL,
                                           LW_NULL);                    /*  建立 printk 缓冲            */
    if (!_G_cLogPartition) {
        return  (PX_ERROR);
    }
    
    API_ThreadAttrBuild(&threadattr, 
                        LW_CFG_THREAD_LOG_STK_SIZE, 
                        LW_PRIO_T_LOG,
                        (LW_OPTION_THREAD_STK_CHK | LW_OPTION_THREAD_SAFE | LW_OPTION_OBJECT_GLOBAL), 
                        LW_NULL);
    
    _S_ulThreadLogId = API_ThreadCreate("t_log",
                                        (PTHREAD_START_ROUTINE)_LogThread,
                                        &threadattr,
                                        LW_NULL);                       /*  建立LOG 处理线程            */
    if (!_S_ulThreadLogId) {
        API_PartitionDelete(&_G_cLogPartition);
        API_MsgQueueDelete(&_G_hLogMsgHandle);
        return  (PX_ERROR);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: _LogThread
** 功能描述: 处理LOG 消息的线程
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  _LogThread (VOID)
{
    static   INT           iOldMsgsLost = 0;
             INT           iNewMsgsLost;
             LW_LOG_MSG    logmsg;
    REGISTER ULONG         ulError;
             size_t        stMsgLen;
             
             CHAR          cThreadName[LW_CFG_OBJECT_NAME_SIZE];
    
    for (;;) {
        ulError = API_MsgQueueReceive(_G_hLogMsgHandle, 
                                      (PVOID)&logmsg,
                                      sizeof(LW_LOG_MSG),
                                      &stMsgLen,
                                      LW_OPTION_WAIT_INFINITE);         /*  等待LOG 消息                */
                                      
        if (ulError) {
            /*
             *  除非初始化不成功, 否则不会到这里
             */
        } else {
            if (logmsg.LOGMSG_ulThreadId) {                             /*  多任务模式发送              */
                if (logmsg.LOGMSG_bIsNeedHeader) {                      /*  需要打印头部                */
                    ulError = API_ThreadGetName(logmsg.LOGMSG_ulThreadId,
                                                cThreadName);
                    if (ulError) {
                        __logPrintf("thread id 0x%08x log : ", 
                                    (PVOID)logmsg.LOGMSG_ulThreadId, 
                                    0, 0, 0, 0, 0, 0, 0, 0, 0);
                    } else {
                        __logPrintf("thread \"%s\" log : ", 
                                    (PVOID)cThreadName, 
                                    0, 0, 0, 0, 0, 0, 0, 0, 0);
                    }
                }
            } else {                                                    /*  中断中发送                  */
                if (logmsg.LOGMSG_bIsNeedHeader) {                      /*  需要打印头部                */
                    __logPrintf("interrupt log : ", 
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                }
            }
            
            if (logmsg.LOGMSG_pcPrintk) {
                __logPrintk(logmsg.LOGMSG_iLevel,
                            logmsg.LOGMSG_pcPrintk);                    /*  打印信息                    */
            } else {
                __logPrintf(logmsg.LOGMSG_pcFormat,
                            logmsg.LOGMSG_pvArg[0],
                            logmsg.LOGMSG_pvArg[1],
                            logmsg.LOGMSG_pvArg[2],
                            logmsg.LOGMSG_pvArg[3],
                            logmsg.LOGMSG_pvArg[4],
                            logmsg.LOGMSG_pvArg[5],
                            logmsg.LOGMSG_pvArg[6],
                            logmsg.LOGMSG_pvArg[7],
                            logmsg.LOGMSG_pvArg[8],
                            logmsg.LOGMSG_pvArg[9]);                    /*  打印信息                    */
            }
        }
        
        iNewMsgsLost = _G_iLogMsgsLost;
        if (iNewMsgsLost != iOldMsgsLost) {
            /*
             *  DO STH.
             */
            iOldMsgsLost = iNewMsgsLost;
        }
    }
}
/*********************************************************************************************************
** 函数名称: __logPrintk
** 功能描述: 向指定的文件集打印 LOG 信息
** 输　入  : iLevel                     打印等级
**           pcPrintk                   格式化信息
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID   __logPrintk (int  iLevel, PCHAR  pcPrintk)
{
    REGISTER size_t     stLen = lib_strlen(pcPrintk);
    REGISTER INT        iFdTemp;                                        /*  临时文件描述符              */
    REGISTER ULONG      ulPartMask;
    
    if (iLevel > console_loglevel) {
        goto    __printk_over;                                          /*  超过打印等级                */
    }
    
    for (iFdTemp = 0; iFdTemp < _G_iMaxWidth; iFdTemp++) {              /*  检查所有可执行读操作的文件  */
        ulPartMask = 
        _G_fdsetLogFd.fds_bits[((unsigned)iFdTemp) / NFDBITS];          /*  获得 iFdTemp 所在的掩码组   */
        
        if (ulPartMask == 0) {                                          /*  这个组中与这个文件无关      */
            iFdTemp += NFDBITS - 1;                                     /*  进行下一个掩码组判断        */
        
        } else if (ulPartMask & (ULONG)(1 << (((unsigned)iFdTemp) % NFDBITS))) {
            write(iFdTemp, pcPrintk, stLen);                            /*  打印                        */
        }
    }
    
__printk_over:
    __LOG_PRINTK_FREE_BUFFER(pcPrintk);                                 /*  释放消息空间                */
}
/*********************************************************************************************************
** 函数名称: __logPrintf
** 功能描述: 向指定的文件集打印 LOG 信息
** 输　入  : pcFormat                   格式化字串
**           pvArg0                     函数参数
**           pvArg1                     函数参数
**           pvArg2                     函数参数
**           pvArg3                     函数参数
**           pvArg4                     函数参数
**           pvArg5                     函数参数
**           pvArg6                     函数参数
**           pvArg7                     函数参数
**           pvArg8                     函数参数
**           pvArg9                     函数参数
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID   __logPrintf (CPCHAR       pcFormat,
                    PVOID        pvArg0,
                    PVOID        pvArg1,
                    PVOID        pvArg2,
                    PVOID        pvArg3,
                    PVOID        pvArg4,
                    PVOID        pvArg5,
                    PVOID        pvArg6,
                    PVOID        pvArg7,
                    PVOID        pvArg8,
                    PVOID        pvArg9)
{
    REGISTER size_t     stLen;
    REGISTER INT        iFdTemp;                                        /*  临时文件描述符              */
    REGISTER ULONG      ulPartMask;
             CHAR       cPrintBuffer[__MAX_MSG_LEN];                    /*  输出缓冲                    */
    
    stLen = bnprintf(cPrintBuffer, __MAX_MSG_LEN, 0, pcFormat,
                     pvArg0, pvArg1, pvArg2, pvArg3, pvArg4, 
                     pvArg5, pvArg6, pvArg7, pvArg8, pvArg9);           /*  格式化字串                  */
    
    for (iFdTemp = 0; iFdTemp < _G_iMaxWidth; iFdTemp++) {              /*  检查所有可执行读操作的文件  */
        ulPartMask = 
        _G_fdsetLogFd.fds_bits[((unsigned)iFdTemp) / NFDBITS];          /*  获得 iFdTemp 所在的掩码组   */
        
        if (ulPartMask == 0) {                                          /*  这个组中与这个文件无关      */
            iFdTemp += NFDBITS - 1;                                     /*  进行下一个掩码组判断        */
        
        } else if (ulPartMask & (ULONG)(1 << (((unsigned)iFdTemp) % NFDBITS))) {
        
            write(iFdTemp, cPrintBuffer, stLen);                        /*  打印                        */
        }
    }
}
#endif                                                                  /*  LW_CFG_LOG_LIB_EN > 0 &&    */
/*********************************************************************************************************
  END
*********************************************************************************************************/
