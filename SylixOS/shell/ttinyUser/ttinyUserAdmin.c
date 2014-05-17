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
** 文   件   名: ttinyUserAdmin.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2014 年 05 月 09 日
**
** 描        述: shell 用户名管理.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "unistd.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_SHELL_EN > 0
#include "pwd.h"
#include "grp.h"
#include "limits.h"
#include "crypt.h"
#include "../ttinyShell/ttinyShellLib.h"
/*********************************************************************************************************
  文件单行缓冲
*********************************************************************************************************/
extern int  scanpw(FILE *fp, struct passwd *pwd, char *buffer, size_t bufsize);
extern int  scangr(FILE *fp, struct group *grp, char *buffer, size_t bufsize);
/*********************************************************************************************************
** 函数名称: __tshellUserShow
** 功能描述: 显示所有用户信息
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __tshellUserShow (VOID)
{
    static PCHAR    pcUserInfo = "\n"\
    "     USER      ENABLE  UID   GID\n"
    "-------------- ------ ----- -----\n";

    struct passwd   pwd;
    FILE           *pfile;
    CHAR            cBuffer[MAX_FILENAME_LENGTH];
    
    pfile = fopen(_PATH_PASSWD, "r");
    if (pfile == LW_NULL) {
        fprintf(stderr, "can not open %s %s.\n", _PATH_PASSWD, lib_strerror(errno));
        return;
    }
    
    printf(pcUserInfo);
    
    for(;;) {
        PCHAR   pcEnable;
    
        if (!scanpw(pfile, &pwd, cBuffer, sizeof(cBuffer))) {
            fclose(pfile);
            return;
        }
        
        if (lib_strcmp(pwd.pw_passwd, "x") == 0) {
            pcEnable = "yes";
        } else if (lib_strcmp(pwd.pw_passwd, "!!") == 0) {
            pcEnable = "nopass";
        } else {
            pcEnable = "no";
        }
        
        printf("%-14s %-6s %5d %5d\n", pwd.pw_name, pcEnable, pwd.pw_uid, pwd.pw_gid);
    }
    
    fclose(pfile);
}
/*********************************************************************************************************
** 函数名称: __tshellGroupShow
** 功能描述: 显示所有组信息
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __tshellGroupShow (VOID)
{
    static PCHAR    pcGrpInfo = "\n"\
    "     GROUP      GID               USERs\n"
    "-------------- ----- --------------------------------\n";

    INT             i;
    struct group    grp;
    FILE           *pfile;
    CHAR            cBuffer[MAX_FILENAME_LENGTH];
    
    pfile = fopen(_PATH_GROUP, "r");
    if (pfile == LW_NULL) {
        fprintf(stderr, "can not open %s %s.\n", _PATH_GROUP, lib_strerror(errno));
        return;
    }
    
    printf(pcGrpInfo);
    
    for(;;) {
        if (!scangr(pfile, &grp, cBuffer, sizeof(cBuffer))) {
            fclose(pfile);
            return;
        }
        
        printf("%-14s %5d ", grp.gr_name, grp.gr_gid);
        
        for (i = 0; grp.gr_mem[i] != LW_NULL; i++) {
            printf("%s, ", grp.gr_mem[i]);
        }
        
        printf("\n");
    }
    
    fclose(pfile);
}
/*********************************************************************************************************
** 函数名称: __tshellUserGenpass
** 功能描述: 为一个用户生成密码
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __tshellUserGenpass (VOID)
{
    INT             i;
    PCHAR           pcGet;
    CHAR            cPass[PASS_MAX];
    CHAR            cConfirm[PASS_MAX];
    CHAR            cSalt[14] = "$1$........$";
    CHAR            cBuffer[PASS_MAX];
    
    printf("\n---GENPASS---\n");
    
__re_input_pass:
    pcGet = getpass_r("input password:", cPass, PASS_MAX);
    if (pcGet == LW_NULL) {
        fprintf(stderr, "password input error.\n");
        goto    __re_input_pass;
    
    } else if (*pcGet == PX_EOS) {
        fprintf(stderr, "you must set a password.\n");
        goto    __re_input_pass;
    }
    
    
    pcGet = getpass_r("input confirm :", cConfirm, PASS_MAX);
    if (pcGet == LW_NULL) {
        fprintf(stderr, "confirm input error.\n");
        goto    __re_input_pass;
    }
    
    if (lib_strcmp(cPass, cConfirm)) {
        fprintf(stderr, "password confirm error.\n");
        goto    __re_input_pass;
    }
    
    for (i = 3; i < 12; i++) {
        INT iType = (lib_rand() % 3);
        
        switch (iType) {
        
        case 0:
            cSalt[i] = (CHAR)((lib_rand() % 10) + '0');
            break;
            
        case 1:
            cSalt[i] = (CHAR)((lib_rand() % 26) + 'A');
            break;
            
        case 2:
            cSalt[i] = (CHAR)((lib_rand() % 26) + 'a');
            break;
        }
    }
    
    crypt_safe(cPass, cSalt, cBuffer, sizeof(cBuffer));
    
    printf("%s\n", cBuffer);
}
/*********************************************************************************************************
** 函数名称: __tshellUserCmdUser
** 功能描述: 系统命令 "user"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellUserCmdUser (INT  iArgC, PCHAR  ppcArgV[])
{
    uid_t   uidOld;
    ULONG   ulError;
    
    uidOld = getuid();
    
    ulError = __tshellUserAuthen(STDIN_FILENO);
    if (ulError) {
        return  (-ERROR_TSHELL_EUSER);
    }
    
    if (geteuid() != 0) {
        fprintf(stderr, "you must use \"root\" user.\n");
        return  (-ERROR_TSHELL_EUSER);
    }

    if (iArgC < 2) {
        __tshellUserShow();
        setuid(uidOld);
        return  (ERROR_NONE);
    }
    
    if (lib_strcmp(ppcArgV[1], "genpass") == 0) {
        __tshellUserGenpass();
        setuid(uidOld);
        return  (ERROR_NONE);
    
    } else {
        setuid(uidOld);
        printf("argments error!\n");
        return  (-ERROR_TSHELL_EPARAM);
    }
}
/*********************************************************************************************************
** 函数名称: __tshellUserCmdGroup
** 功能描述: 系统命令 "group"
** 输　入  : iArgC         参数个数
**           ppcArgV       参数表
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __tshellUserCmdGroup (INT  iArgC, PCHAR  ppcArgV[])
{
    uid_t   uidOld;
    ULONG   ulError;
    
    uidOld = getuid();
    
    ulError = __tshellUserAuthen(STDIN_FILENO);
    if (ulError) {
        return  (-ERROR_TSHELL_EUSER);
    }
    
    if (geteuid() != 0) {
        fprintf(stderr, "you must use \"root\" user.\n");
        return  (-ERROR_TSHELL_EUSER);
    }

    __tshellGroupShow();
    setuid(uidOld);
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __tshellUserCmdInit
** 功能描述: 初始化 tar 命令集
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __tshellUserCmdInit (VOID)
{
    API_TShellKeywordAdd("user", __tshellUserCmdUser);
    API_TShellFormatAdd("user", " [genpass]");
    API_TShellHelpAdd("user", "show user infomation, generate a password.\n"
                               "eg. user\n"
                               "    user genpass\n");
    
    API_TShellKeywordAdd("group", __tshellUserCmdGroup);
    API_TShellHelpAdd("group", "show group infomation.\n");
}
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
