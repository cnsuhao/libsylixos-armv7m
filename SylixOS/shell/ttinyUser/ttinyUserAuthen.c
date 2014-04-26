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
** 文   件   名: ttinyUserAuthen.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 05 月 21 日
**
** 描        述: shell 用户登录

** BUG:
2009.07.14  检测密码时遇空行忽略, 重新检测.
2009.07.29  允许输入空密码(匿名登录时).
2009.12.01  在接收完用户名和发送 password: 提示符前需要清除缓冲. 
2012.01.11  获取用户名之前同样需要清除缓冲.
2012.03.26  在所有的操作之前, 需要等待发送缓冲区为空.
2012.03.26  用户名为 __TTINY_SHELL_FORCE_ABORT 直接产生错误用户.
2012.03.31  使用 shadow 提供的密码验证功能.
2012.04.01  需要 initgroups 设置附加组.
2013.01.15  如果 60 秒还没有输入, 则登录失败.
2013.12.12  登陆完成后需要清除一次缓冲区.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "unistd.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_SHELL_EN > 0
#include "shadow.h"
#include "pwd.h"
#include "grp.h"
#include "../ttinyShell/ttinyShellLib.h"
/*********************************************************************************************************
  内部参数
*********************************************************************************************************/
#define __TTINY_SHELL_USER_TO       60                                  /*  密码超时时间                */
/*********************************************************************************************************
** 函数名称: __tshellUserAuthen
** 功能描述: 用户登录认证
** 输　入  : iTtyFd        文件描述符
**           iLevel        用户等级
** 输　出  : ERROR
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
ULONG  __tshellUserAuthen (INT  iTtyFd)
{
    INT             iOldOpt;
    INT             iRetValue;
    struct timeval  tv = {__TTINY_SHELL_USER_TO, 0};
    
    CHAR            cUserName[MAX_FILENAME_LENGTH];
    CHAR            cPassword[MAX_FILENAME_LENGTH];
    
    ioctl(iTtyFd, FIOSYNC, 0);                                          /*  等待发送完成                */
    
    iRetValue = ioctl(iTtyFd, FIOGETOPTIONS, &iOldOpt);                 /*  获取终端模式                */
    if (iRetValue < 0) {
        return  (ERROR_TSHELL_EUSER);
    }
    
    /*
     *  初始化终端模式
     */
    iRetValue = ioctl(iTtyFd, FIOSETOPTIONS, OPT_TERMINAL);             /*  进入终端模式                */
    if (iRetValue < 0) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not set terminal mode.\r\n");
        return  (ERROR_TSHELL_EUSER);
    }
    
    /*
     *  获得用户名
     */
    write(iTtyFd, "login: ", 7);
    
    iRetValue = waitread(iTtyFd, &tv);                                  /*  等待用户输入用户名          */
    if (iRetValue != 1) {
        goto    __login_fail;
    }
    
    iRetValue = (INT)read(iTtyFd, cUserName, MAX_FILENAME_LENGTH);
    if (iRetValue <= 0) {
        goto    __login_fail;
    }
    cUserName[iRetValue - 1] = PX_EOS;                                  /*  在 \n 处结束                */
    
    if (lib_strcmp(cUserName, __TTINY_SHELL_FORCE_ABORT) == 0) {
        return  (ERROR_TSHELL_EUSER);                                   /*  需要 shell 退出             */
    }
    
    /*
     *  转为无回显终端模式
     */
    iRetValue = ioctl(iTtyFd, FIOSETOPTIONS, 
                      (OPT_TERMINAL & ~OPT_ECHO));                      /*  没有回显                    */
    if (iRetValue < 0) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not set terminal mode.\r\n");
        return  (ERROR_TSHELL_EUSER);
    }
    
    /*
     *  获得用户密码
     */
    ioctl(iTtyFd, FIOFLUSH, 0);                                         /*  清除缓冲区                  */
    write(iTtyFd, "password: ", 10);
    
    iRetValue = waitread(iTtyFd, &tv);                                  /*  等待用户输入密码            */
    if (iRetValue != 1) {
        goto    __login_fail;
    }
    
    iRetValue = (INT)read(iTtyFd, cPassword, MAX_FILENAME_LENGTH);
    if (iRetValue <= 0) {
        goto    __login_fail;
    }
    cPassword[iRetValue - 1] = PX_EOS;                                  /*  在 \n 处结束                */
    
    if (lib_strcmp(cPassword, __TTINY_SHELL_FORCE_ABORT) == 0) {
        return  (ERROR_TSHELL_EUSER);                                   /*  需要 shell 退出             */
    }
    
    iRetValue = userlogin(cUserName, cPassword, 1);                     /*  用户登陆                    */
    
    ioctl(iTtyFd, FIOFLUSH, 0);                                         /*  清除缓冲区                  */
    ioctl(iTtyFd, FIOSETOPTIONS, iOldOpt);                              /*  返回先前的模式              */
    
    if (iRetValue == 0) {
        _ErrorHandle(ERROR_NONE);
        return  (ERROR_NONE);
    }
    
__login_fail:
    fdprintf(iTtyFd, "\nlogin fail.\n");                                /*  登陆失败                    */
    
    _ErrorHandle(ERROR_TSHELL_EUSER);
    return  (ERROR_TSHELL_EUSER);
}
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
