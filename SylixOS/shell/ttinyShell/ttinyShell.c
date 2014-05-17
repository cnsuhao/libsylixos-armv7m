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
** 文   件   名: ttinyShell.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 07 月 27 日
**
** 描        述: 一个超小型的 shell 系统, 使用 tty/pty 做接口, 主要用于调试与简单交互.

** BUG
2008.08.12  在 linux shell 中, 传入的第一个参数为, 命令完整关键字.
2008.10.19  加入转义字符串发送函数.
2009.04.04  今天是清明节, 纪念为国捐躯的英雄们.
            在任务删除时, 需要清除 getopt 相关内存.
2009.05.27  加入 shell 对 ctrl+C 的支持.
2009.05.30  API_TShellCtlCharSend() 需要立即输出.
2009.07.13  加入创建 shell 的扩展方式.
2009.11.23  加入 heap 命令初始化.
2010.01.16  shell 堆栈大小可以设置.
2010.07.28  加入 modem 命令.
2011.06.03  初始化 shell 系统时, 从 /etc/profile 同步环境变量.
2011.07.08  取消 shell 初始化时, load 环境变量. 确保 etc 目录挂载后, 从 bsp 中 load.
2012.12.07  shell 为系统线程.
2012.12.24  shell 支持从进程创建, 创建函数自动将进程内文件描述符 dup 到内核中, 然后再启动 shell.
            API_TShellExec() 如果是在进程中调用, 则必须以同步背景方式运行.
            API_TShellExecBg() 如果在背景中运行则必须将指定的文件 dup 到内核中再运行背景 shell.
2013.01.21  加入用户 CACHE 功能.
2013.07.18  使用新的获取 TCB 的方法, 确保 SMP 系统安全.
2013.08.26  shell 系统命令添加 format 和 help 第二个参数为 const char *.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "../SylixOS/shell/include/ttiny_shell.h"
/*********************************************************************************************************
  应用级 API
*********************************************************************************************************/
#include "../SylixOS/api/Lw_Api_Kernel.h"
#include "../SylixOS/api/Lw_Api_System.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_SHELL_EN > 0
#include "ttinyShell.h"
#include "ttinyShellLib.h"
#include "ttinyShellReadline.h"
#include "ttinyShellSysCmd.h"
#include "ttinyShellSysVar.h"
#include "../SylixOS/shell/ttinyVar/ttinyVarLib.h"
#include "../SylixOS/shell/fsLib/ttinyShellFsCmd.h"
#include "../SylixOS/shell/tarLib/ttinyShellTarCmd.h"
#include "../SylixOS/shell/modemLib/ttinyShellModemCmd.h"
#include "../SylixOS/shell/heapLib/ttinyShellHeapCmd.h"
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
LW_OBJECT_HANDLE        _G_hTShellLock      = LW_OBJECT_HANDLE_INVALID;
static BOOL             _G_bIsInstallSysCmd = LW_FALSE;
/*********************************************************************************************************
  shell 执行线程堆栈大小 (默认与 shell 相同)
*********************************************************************************************************/
size_t                  _G_stShellStackSize = LW_CFG_SHELL_THREAD_STK_SIZE;
/*********************************************************************************************************
  TTY 控制转义字符
*********************************************************************************************************/
static  const PCHAR     _G_pcTShellTransCharTbl[] =
{
    "\a",                                                               /*  警报                        */
    "\x1B" "c",                                                         /*  清屏                        */
    "\x1B" "[",                                                         /*  开始转义序列                */
    "\x1B",
    "",
    "",
    "",
    "\x1B" "]0;"                                                        /*  将图标名和窗口标题设为文本  */
};
#define  __TTNIYSHELL_MAX_TRANSFUNC     7
/*********************************************************************************************************
** 函数名称: API_TShellCtlCharSend
** 功能描述: 发送指定的转义字符序列
** 输　入  : ulFunc        选择的转义功能
**           pcArg         参数序列
** 输　出  : ERROR_NONE 表示没有错误, -1 表示错误,
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_TShellCtlCharSend (ULONG  ulFunc, PCHAR  pcArg)
{
    CHAR            cSendBuffer[128];
    PLW_CLASS_TCB   ptcbCur;
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);
    
    if (!(__TTINY_SHELL_GET_OPT(ptcbCur) & LW_OPTION_TSHELL_VT100)) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    if (ulFunc > __TTNIYSHELL_MAX_TRANSFUNC) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    lib_strlcpy(cSendBuffer, _G_pcTShellTransCharTbl[ulFunc], 128);
    if (pcArg) {
        lib_strlcat(cSendBuffer, pcArg, 128);
    }
    printf("%s", cSendBuffer);
    fflush(stdout);                                                     /*  立即输出                    */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_TShellSetStackSize
** 功能描述: 设定新的 shell 堆栈大小 (设置后仅对后来创建的 shell 和 背景执行 shell 起效)
** 输　入  : stNewSize     新的堆栈大小 (0 表示不设置)
**           pstOldSize    先前堆栈大小 (返回)
** 输　出  : ERROR_NONE 表示没有错误, -1 表示错误,
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_TShellSetStackSize (size_t  stNewSize, size_t  *pstOldSize)
{
    if (pstOldSize) {
        *pstOldSize = _G_stShellStackSize;
    }
    
    if (stNewSize) {
        _G_stShellStackSize = stNewSize;
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_TShellInit
** 功能描述: 安装 tshell 程序, 相当于初始化 tshell 平台
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
VOID  API_TShellInit (VOID)
{
    if (_G_hTShellLock == 0) {
        _G_hTShellLock = API_SemaphoreMCreate("tshell_lock", LW_PRIO_DEF_CEILING, 
                         __TTINY_SHELL_LOCK_OPT, LW_NULL);              /*  创建锁                      */
        if (!_G_hTShellLock) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, 
                         "tshell lock can not create.\r\n");
            _DebugHandle(__LOGMESSAGE_LEVEL,
                         "ttniy shell system is not initialize.\r\n");
            return;
        }
        API_SystemHookAdd(__tShellOptDeleteHook, 
                          LW_OPTION_THREAD_DELETE_HOOK);                /*  安装回调函数                */
        API_SystemHookAdd(__tshellReadlineClean, 
                          LW_OPTION_THREAD_DELETE_HOOK);                /*  readline 历史删除           */
    }
    
    if (_G_bIsInstallSysCmd == LW_FALSE) {
        _G_bIsInstallSysCmd =  LW_TRUE;
        __tshellSysVarInit();                                           /*  初始化系统环境变量          */
        __tshellUserCmdInit();                                          /*  初始化用户管理命令          */
        __tshellSysCmdInit();                                           /*  初始化系统命令              */
#if LW_CFG_SHELL_HEAP_TRACE_EN > 0
        __tshellHeapCmdInit();                                          /*  初始化内存堆命令            */
#endif                                                                  /*  LW_CFG_SHELL_HEAP_TRACE_EN  */
        __tshellFsCmdInit();                                            /*  初始化文件系统命令          */
        __tshellModemCmdInit();                                         /*  初始化 modem 命令           */
        __tshellTarCmdInit();                                           /*  初始化 tar 命令             */
    }
}
/*********************************************************************************************************
** 函数名称: API_TShellCreateEx
** 功能描述: 创建一个 ttiny shell 系统, SylixOS 支持多个终端设备同时运行.
**           tshell 可以使用标准 tty 设备, 或者 pty 虚拟终端设备.
** 输　入  : iTtyFd                终端设备的文件描述符
**           ulOption              启动参数
**           pfuncRunCallback      初始化完毕后, 启动回调
** 输　出  : shell 线程的句柄.
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
LW_OBJECT_HANDLE  API_TShellCreateEx (INT  iTtyFd, ULONG  ulOption, FUNCPTR  pfuncRunCallback)
{
    LW_CLASS_THREADATTR     threadattrTShell;
    LW_OBJECT_HANDLE        hTShellHandle;
    INT                     iKernelFile;
    PLW_CLASS_TCB           ptcbShell;
    
    _DebugHandle(__LOGMESSAGE_LEVEL, "ttniy shell system initialize...\r\n");
    
    if (!isatty(iTtyFd)) {                                              /*  检测是否为终端设备          */
        _DebugHandle(__ERRORMESSAGE_LEVEL,
                     "is not a tty or pty device.\r\n");
        _DebugHandle(__LOGMESSAGE_LEVEL, 
                     "ttniy shell system is not initialize.\r\n");
        return  (0);
    }
    
    API_TShellInit();                                                   /*  初始化 shell                */
    
    if (__PROC_GET_PID_CUR() != 0) {                                    /*  在进程中启动                */
        iKernelFile = dup2kernel(iTtyFd);                               /*  将此文件描述符 dup 到 kernel*/
        if (iKernelFile < 0) {
            return  (0);
        }
        ulOption |= LW_OPTION_TSHELL_CLOSE_FD;                          /*  执行完毕后需要关闭文件      */
    } else {
        iKernelFile = iTtyFd;
    }
    
    API_ThreadAttrBuild(&threadattrTShell,
                        _G_stShellStackSize,                            /*  shell 堆栈大小              */
                        LW_PRIO_T_SHELL,
                        LW_CFG_SHELL_THREAD_OPTION | LW_OPTION_OBJECT_GLOBAL,
                        (PVOID)iKernelFile);                            /*  构建属性块                  */
    
    hTShellHandle = API_ThreadInit("t_tshell", __tshellThread,
                                    &threadattrTShell, LW_NULL);        /*  创建 tshell 线程            */
    if (!hTShellHandle) {
        if (__PROC_GET_PID_CUR() != 0) {                                /*  在进程中启动                */
            __KERNEL_SPACE_ENTER();
            close(iKernelFile);                                         /*  内核中关闭 dup 到内核的文件 */
            __KERNEL_SPACE_EXIT();
        }
        _DebugHandle(__ERRORMESSAGE_LEVEL, 
                     "tshell thread can not create.\r\n");
        _DebugHandle(__LOGMESSAGE_LEVEL, 
                     "ttniy shell system is not initialize.\r\n");
        return  (0);
    }
    
    ptcbShell = __GET_TCB_FROM_INDEX(_ObjectGetIndex(hTShellHandle));
    __TTINY_SHELL_SET_OPT(ptcbShell, ulOption);                         /*  初始化选项                  */
    __TTINY_SHELL_SET_CALLBACK(ptcbShell, pfuncRunCallback);            /*  初始化启动回调              */
    
    API_ThreadStart(hTShellHandle);                                     /*  启动 shell 线程             */
    
    return  (hTShellHandle);
}
/*********************************************************************************************************
** 函数名称: API_TShellCreate
** 功能描述: 创建一个 ttiny shell 系统, SylixOS 支持多个终端设备同时运行.
**           tshell 可以使用标准 tty 设备, 或者 pty 虚拟终端设备.
** 输　入  : iTtyFd    终端设备的文件描述符
**           ulOption  启动参数
** 输　出  : shell 线程的句柄.
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
LW_OBJECT_HANDLE  API_TShellCreate (INT  iTtyFd, ULONG  ulOption)
{
    return  (API_TShellCreateEx(iTtyFd, ulOption, LW_NULL));
}
/*********************************************************************************************************
** 函数名称: API_TShellGetUserName
** 功能描述: 通过 shell 用户缓冲获取一个用户名.
** 输　入  : uid           用户 id
**           pcName        用户名
**           stSize        缓冲区大小
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_TShellGetUserName (uid_t  uid, PCHAR  pcName, size_t  stSize)
{
    if (!pcName || !stSize) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    return  (__tshellGetUserName(uid, pcName, stSize));
}
/*********************************************************************************************************
** 函数名称: API_TShellGetGrpName
** 功能描述: 通过 shell 用户缓冲获取一个组名.
** 输　入  : gid           组 id
**           pcName        组名
**           stSize        缓冲区大小
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_TShellGetGrpName (gid_t  gid, PCHAR  pcName, size_t  stSize)
{
    if (!pcName || !stSize) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    return  (__tshellGetGrpName(gid, pcName, stSize));
}
/*********************************************************************************************************
** 函数名称: API_TShellFlushCache
** 功能描述: 删除所有 shell 缓冲的用户与组信息 (当有用户或者组变动时, 需要释放 CACHE)
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
VOID  API_TShellFlushCache (VOID)
{
    __tshellFlushCache();
}
/*********************************************************************************************************
** 函数名称: API_TShellKeywordAdd
** 功能描述: 向 ttiny shell 系统添加一个关键字.
** 输　入  : pcKeyword     关键字
**           pfuncCommand  执行的 shell 函数
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_TShellKeywordAdd (CPCHAR  pcKeyword, PCOMMAND_START_ROUTINE  pfuncCommand)
{
    return  (API_TShellKeywordAddEx(pcKeyword, pfuncCommand, LW_OPTION_DEFAULT));
}
/*********************************************************************************************************
** 函数名称: API_TShellKeywordAddEx
** 功能描述: 向 ttiny shell 系统添加一个关键字.
** 输　入  : pcKeyword     关键字
**           pfuncCommand  执行的 shell 函数
**           ulOption      选项
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_TShellKeywordAddEx (CPCHAR  pcKeyword, PCOMMAND_START_ROUTINE  pfuncCommand, ULONG  ulOption)
{
    REGISTER size_t    stStrLen;

    if (__PROC_GET_PID_CUR() != 0) {                                    /*  进程中不能注册命令          */
        _ErrorHandle(ENOTSUP);
        return  (ENOTSUP);
    }
    
    if (!pcKeyword) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pcKeyword invalidate.\r\n");
        _ErrorHandle(ERROR_TSHELL_EPARAM);
        return  (ERROR_TSHELL_EPARAM);
    }
    
    stStrLen = lib_strnlen(pcKeyword, LW_CFG_SHELL_MAX_KEYWORDLEN);
    
    if (stStrLen >= (LW_CFG_SHELL_MAX_KEYWORDLEN + 1)) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pcKeyword is too long.\r\n");
        _ErrorHandle(ERROR_TSHELL_EPARAM);
        return  (ERROR_TSHELL_EPARAM);
    }

    return  (__tshellKeywordAdd(pcKeyword, stStrLen, pfuncCommand, ulOption));
}
/*********************************************************************************************************
** 函数名称: API_TShellHelpAdd
** 功能描述: 为一个关键字添加帮助信息
** 输　入  : pcKeyword     关键字
**           pcHelp        帮助信息字符串
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_TShellHelpAdd (CPCHAR  pcKeyword, CPCHAR  pcHelp)
{
             __PTSHELL_KEYWORD   pskwNode = LW_NULL;
    REGISTER size_t              stStrLen;
    
    if (__PROC_GET_PID_CUR() != 0) {                                    /*  进程中不允许操作            */
        _ErrorHandle(ENOTSUP);
        return  (ENOTSUP);
    }
    
    if (!pcKeyword) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pcKeyword invalidate.\r\n");
        _ErrorHandle(ERROR_TSHELL_EPARAM);
        return  (ERROR_TSHELL_EPARAM);
    }
    if (!pcHelp) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pcHelp invalidate.\r\n");
        _ErrorHandle(ERROR_TSHELL_EPARAM);
        return  (ERROR_TSHELL_EPARAM);
    }
    
    stStrLen = lib_strnlen(pcKeyword, LW_CFG_SHELL_MAX_KEYWORDLEN);
    
    if (stStrLen >= (LW_CFG_SHELL_MAX_KEYWORDLEN + 1)) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pcKeyword is too long.\r\n");
        _ErrorHandle(ERROR_TSHELL_EPARAM);
        return  (ERROR_TSHELL_EPARAM);
    }
    
    if (ERROR_NONE == __tshellKeywordFind(pcKeyword, &pskwNode)) {      /*  查找关键字                  */
        
        __TTINY_SHELL_LOCK();                                           /*  互斥访问                    */
        if (pskwNode->SK_pcHelpString) {
            __TTINY_SHELL_UNLOCK();                                     /*  释放资源                    */
            
            _DebugHandle(__ERRORMESSAGE_LEVEL, "help message overlap.\r\n");
            _ErrorHandle(ERROR_TSHELL_OVERLAP);
            return  (ERROR_TSHELL_OVERLAP);
        }
        
        stStrLen = lib_strlen(pcHelp);                                  /*  为帮助字串开辟空间          */
        pskwNode->SK_pcHelpString = (PCHAR)__SHEAP_ALLOC(stStrLen + 1);
        if (!pskwNode->SK_pcHelpString) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
            _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
            return  (ERROR_SYSTEM_LOW_MEMORY);
        }
        lib_strcpy(pskwNode->SK_pcHelpString, pcHelp);
        __TTINY_SHELL_UNLOCK();                                         /*  释放资源                    */
        
        _ErrorHandle(ERROR_NONE);
        return  (ERROR_NONE);
    
    } else {
        
        _ErrorHandle(ERROR_TSHELL_EKEYWORD);
        return  (ERROR_TSHELL_EKEYWORD);                                /*  关键字错误                  */
    }
}
/*********************************************************************************************************
** 函数名称: API_TShellFormatAdd
** 功能描述: 为一个关键字添加格式字串信息
** 输　入  : pcKeyword     关键字
**           pcFormat      格式字串
** 输　出  : 错误代码.
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_TShellFormatAdd (CPCHAR  pcKeyword, CPCHAR  pcFormat)
{
             __PTSHELL_KEYWORD   pskwNode = LW_NULL;
    REGISTER size_t              stStrLen;
    
    if (__PROC_GET_PID_CUR() != 0) {                                    /*  进程中不允许操作            */
        _ErrorHandle(ENOTSUP);
        return  (ENOTSUP);
    }
    
    if (!pcKeyword) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pcKeyword invalidate.\r\n");
        _ErrorHandle(ERROR_TSHELL_EPARAM);
        return  (ERROR_TSHELL_EPARAM);
    }
    if (!pcFormat) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pcHelp invalidate.\r\n");
        _ErrorHandle(ERROR_TSHELL_EPARAM);
        return  (ERROR_TSHELL_EPARAM);
    }
    
    stStrLen = lib_strnlen(pcKeyword, LW_CFG_SHELL_MAX_KEYWORDLEN);
    
    if (stStrLen >= (LW_CFG_SHELL_MAX_KEYWORDLEN + 1)) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "pcKeyword is too long.\r\n");
        _ErrorHandle(ERROR_TSHELL_EPARAM);
        return  (ERROR_TSHELL_EPARAM);
    }
    
    if (ERROR_NONE == __tshellKeywordFind(pcKeyword, &pskwNode)) {      /*  查找关键字                  */
        
        __TTINY_SHELL_LOCK();                                           /*  互斥访问                    */
        if (pskwNode->SK_pcFormatString) {
            __TTINY_SHELL_UNLOCK();                                     /*  释放资源                    */
            
            _DebugHandle(__ERRORMESSAGE_LEVEL, "format string overlap.\r\n");
            _ErrorHandle(ERROR_TSHELL_OVERLAP);
            return  (ERROR_TSHELL_OVERLAP);
        }
        
        stStrLen = lib_strlen(pcFormat);                                /*  为帮助字串开辟空间          */
        pskwNode->SK_pcFormatString = (PCHAR)__SHEAP_ALLOC(stStrLen + 1);
        if (!pskwNode->SK_pcFormatString) {
            _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
            _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
            return  (ERROR_SYSTEM_LOW_MEMORY);
        }
        lib_strcpy(pskwNode->SK_pcFormatString, pcFormat);
        __TTINY_SHELL_UNLOCK();                                         /*  释放资源                    */
        
        _ErrorHandle(ERROR_NONE);
        return  (ERROR_NONE);
    
    } else {
        
        _ErrorHandle(ERROR_TSHELL_EKEYWORD);
        return  (ERROR_TSHELL_EKEYWORD);                                /*  关键字错误                  */
    }
}
/*********************************************************************************************************
** 函数名称: API_TShellExec
** 功能描述: ttiny shell 系统, 执行一条 shell 命令
** 输　入  : pcCommand    命令字符串
** 输　出  : 命令返回值(当发生错误时, 返回值为负值)
** 全局变量: 
** 调用模块: 
** 注  意  : 当 shell 检测出命令字符串错误时, 将会返回负值, 此值取相反数后即为错误编号.
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_TShellExec (CPCHAR  pcCommandExec)
{
    if (__PROC_GET_PID_CUR() != 0) {                                    /*  进程中执行                  */
        return  (API_TShellExecBg(pcCommandExec, LW_NULL, LW_NULL, LW_TRUE, LW_NULL));
    } else {
        return  (__tshellExec(pcCommandExec, LW_NULL));
    }
}
/*********************************************************************************************************
** 函数名称: API_TShellExecBg
** 功能描述: ttiny shell 系统, 背景执行一条 shell 命令 (不过成功与否都会关闭需要关闭的文件)
** 输　入  : pcCommand    命令字符串
**           iFd[3]       标准文件
**           bClosed[3]   执行结束后是否关闭对应标准文件
**           bIsJoin      是否等待命令执行结束
**           pulSh        背景线程句柄, (仅当 bIsJoin = LW_FALSE 时返回)
** 输　出  : 命令返回值(当发生错误时, 返回值为负值)
** 全局变量: 
** 调用模块: 
** 注  意  : 当 shell 检测出命令字符串错误时, 将会返回负值, 此值取相反数后即为错误编号.
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_TShellExecBg (CPCHAR  pcCommandExec, INT  iFd[3], BOOL  bClosed[3], 
                       BOOL  bIsJoin, LW_OBJECT_HANDLE *pulSh)
{
    INT  iRet;
    INT  iError;
    INT  iFdArray[3];
    BOOL bClosedArray[3];
    
    if (__PROC_GET_PID_CUR() != 0) {                                    /*  进程中创建                  */
        if (iFd == LW_NULL) {
            iFdArray[0] = dup2kernel(STD_IN);
            iFdArray[1] = dup2kernel(STD_OUT);
            iFdArray[2] = dup2kernel(STD_ERR);
        } else {
            iFdArray[0] = dup2kernel(iFd[0]);
            iFdArray[1] = dup2kernel(iFd[1]);
            iFdArray[2] = dup2kernel(iFd[2]);
        }
        /*
         *  由于相关文件已经 dup 到内核中, 所以这里可以关闭进程中的相关的文件
         */
        if (bClosed && iFd) {
            if (bClosed[0]) {
                close(iFd[0]);
            }
            if (bClosed[1]) {
                close(iFd[1]);
            }
            if (bClosed[2]) {
                close(iFd[2]);
            }
        }
        
        /*
         *  由于已经 dup 到内核中, 这里必须在运行结束后关闭这些文件
         */
        bClosedArray[0] = LW_TRUE;                                      /*  执行完毕后需要关闭这些       */
        bClosedArray[1] = LW_TRUE;
        bClosedArray[2] = LW_TRUE;
        
        iFd     = iFdArray;
        bClosed = bClosedArray;                                         /*  文件是 dup 出来的这里必须全关*/
    
    } else {                                                            /*  内核中调用                   */
        if (iFd == LW_NULL) {
            LW_OBJECT_HANDLE  ulMe = API_ThreadIdSelf();
            iFd = iFdArray;
            iFdArray[0] = API_IoTaskStdGet(ulMe, STD_IN);
            iFdArray[1] = API_IoTaskStdGet(ulMe, STD_OUT);
            iFdArray[2] = API_IoTaskStdGet(ulMe, STD_ERR);
        }
        if (bClosed == LW_NULL) {
            bClosed = bClosedArray;
            bClosedArray[0] = LW_FALSE;
            bClosedArray[1] = LW_FALSE;
            bClosedArray[2] = LW_FALSE;
        }
    }
    
    iError = __tshellBgCreateEx(iFd, bClosed, pcCommandExec, 
                                lib_strlen(pcCommandExec), bIsJoin, 0, pulSh, &iRet);
    if (iError < 0) {                                                   /*  背景创建失败                */
        /*
         *  运行失败, 则关闭内核中需要关闭的文件
         */
        __KERNEL_SPACE_ENTER();
        if (bClosed[0]) {
            close(iFd[0]);
        }
        if (bClosed[1]) {
            close(iFd[1]);
        }
        if (bClosed[2]) {
            close(iFd[2]);
        }
        __KERNEL_SPACE_EXIT();
    }                                                                   /*  如果运行成功, 则会自动关闭  */
    
    return  (iRet);
}
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
