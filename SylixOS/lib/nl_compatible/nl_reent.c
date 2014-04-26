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
** 文   件   名: nl_reent.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2011 年 08 月 15 日
**
** 描        述: newlib fio 兼容层. (SylixOS 和 VxWorks 类似, 必须使用自己的 libc 库)
                 很多 gcc 使用 newlib 作为 libc, 其他的库也都依赖于 newlib, 例如 libstdc++ 库, 
                 SylixOS 要想使用这些库, 则必须提供一个 newlib reent 兼容的接口.
                 
2012.11.09  lib_nlreent_init() 修正错误的缓冲类型.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_FIO_LIB_EN > 0)
#include "../libc/stdio/lib_file.h"
/*********************************************************************************************************
  newlib compatible reent
*********************************************************************************************************/
typedef struct __nl_reent {
    /*
     *  ._errno 不使用, 为了二进制兼容, 这里仅是占位符而已.
     *  因为 newlib errno 的定义为 *__errno() 与 SylixOS 相同, 所以如果使用了 errno 就一定会定位到 SylixOS
     *  中的 __errno() 函数.
     */
    int     _errno_notuse;                                              /*  not use!                    */
    
    FILE    *_stdin, *_stdout, *_stderr;                                /*  三个标准文件                */

    void    *_pad_notuse[48];                                           /*  not use!                    */
    
    FILE     _file[3];                                                  /*  三个标准文件                */
} __NL_REENT;
/*********************************************************************************************************
  newlib compatible reent for all thread
*********************************************************************************************************/
static __NL_REENT _G_nlreentTbl[LW_CFG_MAX_THREADS];                    /*  每个任务都拥有一个 reent    */
/*********************************************************************************************************
  newlib compatible reent
*********************************************************************************************************/
struct __nl_reent *__attribute__((weak)) _impure_ptr;                   /*  当前 newlib reent 上下文    */
/*********************************************************************************************************
  stdio particular function
*********************************************************************************************************/
extern int fclose_nfree_fp(FILE *fp);
/*********************************************************************************************************
** 函数名称: __nlreent_swtich_hook
** 功能描述: nl reent 任务调度 hook
** 输　入  : ulOldThread   将被调度器换出的线程
**           ulNewThread   将要运行的线程.
** 输　出  : 0, 1, 2 表示对应的文件句柄, -1 表示没有在 pnlreent->_file[] 中
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __nlreent_swtich_hook (LW_OBJECT_HANDLE  ulOldThread, LW_OBJECT_HANDLE  ulNewThread)
{
    __NL_REENT *pnlreent = &_G_nlreentTbl[_ObjectGetIndex(ulNewThread)];
    
    _impure_ptr = pnlreent;
}
/*********************************************************************************************************
** 函数名称: __nlreent_delete_hook
** 功能描述: nl reent 任务删除 hook
** 输　入  : ulThread      线程
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __nlreent_delete_hook (LW_OBJECT_HANDLE  ulThread)
{
    INT          i;
    __NL_REENT  *pnlreent = &_G_nlreentTbl[_ObjectGetIndex(ulThread)];
    
    /*
     *  注意, 当 stdin stdout 或 stderr 被重定向了, 就不用关闭了, 这里只关闭最原始的三个 std 文件.
     */
    for (i = 0; i < 3; i++) {
        if (pnlreent->_file[i]._flags) {
            fclose_nfree_fp(&pnlreent->_file[i]);
        }
    }
    
    pnlreent->_stdin  = LW_NULL;
    pnlreent->_stdout = LW_NULL;
    pnlreent->_stderr = LW_NULL;
}
/*********************************************************************************************************
** 函数名称: lib_nlreent_init
** 功能描述: 初始化指定线程的 nl reent 结构
** 输　入  : ulThread      线程 ID
** 输　出  : newlib 兼容 reent 结构
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  lib_nlreent_init (LW_OBJECT_HANDLE  ulThread)
{
    static BOOL  bSwpHook = LW_FALSE;
    static BOOL  bDelHook = LW_FALSE;
    
    __NL_REENT  *pnlreent = &_G_nlreentTbl[_ObjectGetIndex(ulThread)];
    
    if (bSwpHook == LW_FALSE) {
        if (API_SystemHookAdd(__nlreent_swtich_hook, LW_OPTION_THREAD_SWAP_HOOK) == ERROR_NONE) {
            bSwpHook = LW_TRUE;
        }
    }
    
    if (bDelHook == LW_FALSE) {
        if (API_SystemHookAdd(__nlreent_delete_hook, LW_OPTION_THREAD_DELETE_HOOK) == ERROR_NONE) {
            bDelHook = LW_TRUE;
        }
    }
    
    pnlreent->_stdin  = &pnlreent->_file[STDIN_FILENO];
    pnlreent->_stdout = &pnlreent->_file[STDOUT_FILENO];
    pnlreent->_stderr = &pnlreent->_file[STDERR_FILENO];
    
    __stdioFileCreate(pnlreent->_stdin);
    __stdioFileCreate(pnlreent->_stdout);
    __stdioFileCreate(pnlreent->_stderr);
    
    /*
     *  stdin init flags
     */
    pnlreent->_stdin->_flags = __SRD;
#if LW_CFG_FIO_STDIN_LINE_EN > 0
    pnlreent->_stdin->_flags |= __SLBF;
#endif                                                                  /* LW_CFG_FIO_STDIN_LINE_EN     */

    /*
     *  stdout init flags
     */
    pnlreent->_stdout->_flags = __SWR;
#if LW_CFG_FIO_STDIN_LINE_EN > 0
    pnlreent->_stdout->_flags |= __SLBF;
#endif                                                                  /* LW_CFG_FIO_STDIN_LINE_EN     */

    /*
     *  stderr init flags
     */
    pnlreent->_stderr->_flags = __SWR | __SNBF;
    
    pnlreent->_stdin->_file  = STDIN_FILENO;
    pnlreent->_stdout->_file = STDOUT_FILENO;
    pnlreent->_stderr->_file = STDERR_FILENO;
}
/*********************************************************************************************************
** 函数名称: lib_nlreent_stdfile
** 功能描述: 获取指定线程的 stdfile 结构 
** 输　入  : ulThread      线程 ID
**           FileNo        文件号, 0, 1, 2
** 输　出  : stdfile 指针地址
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
FILE **lib_nlreent_stdfile (LW_OBJECT_HANDLE  ulThread, INT  FileNo)
{
    __NL_REENT *pnlreent;

    if (!ulThread) {
        return  (LW_NULL);
    }
    
    pnlreent = &_G_nlreentTbl[_ObjectGetIndex(ulThread)];
    
    switch (FileNo) {
    
    case STDIN_FILENO:
        return  (&pnlreent->_stdin);
        
    case STDOUT_FILENO:
        return  (&pnlreent->_stdout);
        
    case STDERR_FILENO:
        return  (&pnlreent->_stderr);
        
    default:
        return  (LW_NULL);
    }
}
#endif                                                                  /*  LW_CFG_DEVICE_EN > 0        */
                                                                        /*  LW_CFG_FIO_LIB_EN > 0       */
/*********************************************************************************************************
  END
*********************************************************************************************************/
