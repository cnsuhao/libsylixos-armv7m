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
** 文   件   名: loader.c
**
** 创   建   人: Jiang.Taijin (蒋太金)
**
** 文件创建日期: 2010 年 04 月 17 日
**
** 描        述: 模块管理

** BUG:
2010.10.24  修改注释.
2011.02.20  加入 unload 接口.
            加入全局符号与本地符号的处理. (韩辉)
2011.03.02  加入指定扇区加载符号表的能力. (韩辉)
            将所有 module 连接成链表.
2011.03.23  加入对 c++ 的支持.
2011.03.26  API_ModuleRunEx() 最后一个参数应为 const 类型.
2011.06.09  加入 __ldPathIsFile() 判断可执行文件类型与权限.
2011.07.20  对动态库的搜索路径为 : PATH 与 LD_LIBRARY_PATH.
2011.07.29  今天是令人兴奋的一天, 韩辉, 徐贵州, 蒋太金, 焦进星, 梁毅, 经过几个小时激烈的辩论, 终于找到了
            解决 SylixOS 对进程(虚拟)的支持模型!
2011.12.08  如果模块初始化失败, 则装载失败. (韩辉)
2012.04.12  对内核模块的操作, 非 root 用户没有权限. (韩辉)
2012.12.10  moduleLoadSub 装载子库的时候需要提供 entry. 因为进程的入口在补丁中. (韩辉)
2012.12.17  moduleGetLibPath 保存绝对路径. (韩辉)
            修正 moduleLoadSub 只有一个库时的依赖关系判断错误. (韩辉)
            API_ModuleSym() 时需要加锁. (韩辉)
2013.01.18  装载依赖的链接库时, 不能继承当前库的符号是否可见属性, 因为如果 dlopen 打开一个库使用 LOCAL 
            方式, 而这个库又依赖于其他的动态链接库, 这时, 如果以来的库也使用 LOCAL 则 dlopen 的库无法完成
            符号重定向, 因为它找不到它依赖的库里面的符号, 所以, 只要是自动装载的动态库, 必须为 GLOBAL.
            测试 isql -> libunixodbc.so -> libsqlite3odbc.so(LOCAL) -> libsqlite3.so 时更正. (韩辉)
2013.05.21  module 更新符号表的保存与查找算法, 这里更新相关接口.
2013.06.07  加入 API_ModuleShareRefresh 操作, 用来清除当前缓存的动态库或者应用的共享信息.
2014.07.26  cache text update 操作放在每个模块加载之后.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_MODULELOADER_EN > 0
#include "../include/loader_lib.h"
#include "../include/loader_symbol.h"
#include "../include/loader_error.h"
#include "../elf/elf_loader.h"
/*********************************************************************************************************
  模块进程模型
  
  SylixOS 使用一个虚拟地址空间映射关系, 所以这里面的进程只能称为一个没有独立寻址空间的进程框架, 主要是为了
  方便程序开发使用, 它具有独立的文件描述符, 独立的内核对象, 这些资源将在进程退出时被操作系统或者父亲回收.
  
  编译进程时, 需要加入 SylixOS 提供的相关补丁, 这些补丁的源码和说明可以在 doc/vpmpdm 目录下找到. 
  
  每一个进程拥有一个自己私有的内存堆, 这个内存对通过缺页中断分配, 默认大小为环境变量 SO_MEM_PAGES 决定, 在
  进程退出时, 整个空间将会被释放. 
    
  注意: dlopen() 装载时, RTLD_GLOBAL 如果有效, 则此库的符号将可以在本进程内被动态引用. 记住, 与 linux 系统
        类似, 动态库只能依附在进程内, RTLD_GLOBAL 仅表明此库的符号在本进程内可以被动态使用.
        
        如装入 ko 类型内核模块则导出的符号表对整个内核可见.
*********************************************************************************************************/
/*********************************************************************************************************
  C++ 模块 atexit() 模块析构函数
*********************************************************************************************************/
extern void __cxa_module_finalize(void *pvBase, size_t stLen, BOOL bCall);
/*********************************************************************************************************
  装载器判断文件是否可被装载 (进判断文件权限和类型, 不判断文件格式)
*********************************************************************************************************/
extern BOOL __ldPathIsFile(CPCHAR  pcName);
/*********************************************************************************************************
** 函数名称: moduleCreate
** 功能描述: 创建并初始化模块结构.
** 输　入  : pcPath        文件路径
**           bExportSym    是否导出符号
**           bIsGlobal     符号是否为全局
**           pcInit        模块初始化函数名
**           pcExit        模块退出函数名
**           pcEntry       模块入口函数名
**           pcSection     仅从指定的 section 中导出符号表
**           pvVProc       指向的进程主模块
** 输　出  : 创建好的模块指针，如果失败，输出NULL。
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static LW_LD_EXEC_MODULE  *moduleCreate (CPCHAR  pcPath,
                                         BOOL    bExportSym,
                                         BOOL    bIsGlobal,
                                         CPCHAR  pcInit,
                                         CPCHAR  pcExit,
                                         CPCHAR  pcEntry,
                                         CPCHAR  pcSection,
                                         PVOID   pvVProc)
{
    LW_LD_EXEC_MODULE *pmodule      = LW_NULL;
    size_t             stModuleSize = 0;                                /*  模块大小                    */
    size_t             stInitOff    = 0;                                /*  Init函数名称的位置          */
    size_t             stExitOff    = 0;                                /*  Exit函数名称的位置          */
    size_t             stEntryOff   = 0;                                /*  Entry函数名称的位置         */
    size_t             stPathOff    = 0;                                /*  文件路径在结构体中的位置    */
    size_t             stSectionOff = 0;

    if (lib_strlen(pcPath) >= PATH_MAX) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "module path too long!\r\n");
        return  (LW_NULL);
    }

    stModuleSize = sizeof(LW_LD_EXEC_MODULE);

    if (pcInit) {
        stInitOff = stModuleSize;                                       /*  计算偏移，累加结构内存大小  */
        stModuleSize += lib_strlen(pcInit) + 1;
    }

    if (pcExit) {
        stExitOff = stModuleSize;
        stModuleSize += lib_strlen(pcExit) + 1;
    }

    if (pcEntry) {
        stEntryOff = stModuleSize;
        stModuleSize += lib_strlen(pcEntry) + 1;
    }

    if (pcPath) {
        stPathOff = stModuleSize;
        stModuleSize += lib_strlen(pcPath) + 1;
    }

    if (pcSection) {
        stSectionOff = stModuleSize;
        stModuleSize += lib_strlen(pcSection) + 1;
    }

    pmodule = (LW_LD_EXEC_MODULE *)LW_LD_SAFEMALLOC(stModuleSize);
    if (LW_NULL == pmodule) {
        return LW_NULL;
    }
    lib_bzero(pmodule, stModuleSize);

    pmodule->EMOD_ulMagic      = __LW_LD_EXEC_MODULE_MAGIC;
    pmodule->EMOD_ulModType    = LW_LD_MOD_TYPE_KO;
    pmodule->EMOD_ulStatus     = LW_LD_STATUS_UNLOAD;
    pmodule->EMOD_ulRefs       = 0;
    pmodule->EMOD_pvUsedArr    = LW_NULL;
    pmodule->EMOD_ulUsedCnt    = 0;
    pmodule->EMOD_bExportSym   = bExportSym;
    pmodule->EMOD_bIsGlobal    = bIsGlobal;
    pmodule->EMOD_pvFormatInfo = LW_NULL;
    pmodule->EMOD_pvproc       = (LW_LD_VPROC *)pvVProc;

    if (pcInit) {
        pmodule->EMOD_pcInit = (PCHAR)pmodule + stInitOff;
        lib_strcpy(pmodule->EMOD_pcInit, pcInit);                       /*  拷贝字符串                  */
    }

    if (pcExit) {
        pmodule->EMOD_pcExit = (PCHAR)pmodule + stExitOff;
        lib_strcpy(pmodule->EMOD_pcExit, pcExit);
    }

    if (pcEntry) {
        pmodule->EMOD_pcEntry = (PCHAR)pmodule + stEntryOff;
        lib_strcpy(pmodule->EMOD_pcEntry, pcEntry);
    }

    if (pcPath) {
        pmodule->EMOD_pcModulePath = (PCHAR)pmodule + stPathOff;
        lib_strcpy(pmodule->EMOD_pcModulePath, pcPath);
    }

    if (pcSection) {
        pmodule->EMOD_pcSymSection = (PCHAR)pmodule + stSectionOff;
        lib_strcpy(pmodule->EMOD_pcSymSection, pcSection);
    }

    return  (pmodule);
}
/*********************************************************************************************************
** 函数名称: moduleDestory
** 功能描述: 销毁模块.
** 输　入  : pmodule       模块指针
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT moduleDestory (LW_LD_EXEC_MODULE *pmodule)
{
    INT     i;

    if (pmodule->EMOD_pfuncDestroy) {
        pmodule->EMOD_pfuncDestroy();                                   /*  执行模块销毁函数            */
    }

    if (pmodule->EMOD_psymbolHash) {                                    /*  符号表销毁                  */
        __moduleDeleteAllSymbol(pmodule);
        LW_LD_SAFEFREE(pmodule->EMOD_psymbolHash);
    }

    if(pmodule->EMOD_psegmentArry) {                                    /*  扩展段销毁                  */
        for(i = 0; i < pmodule->EMOD_ulSegCount; i++) {
            LW_LD_VMSAFEFREE(pmodule->EMOD_psegmentArry[i].ESEG_ulAddr);
        }
        LW_LD_SAFEFREE(pmodule->EMOD_psegmentArry);
    }
    
    if (pmodule->EMOD_ulModType == LW_LD_MOD_TYPE_SO) {
        LW_LD_VMSAFEFREE_AREA(pmodule->EMOD_pvBaseAddr);                /*  共享库卸载                  */
    } else {
        LW_LD_VMSAFEFREE(pmodule->EMOD_pvBaseAddr);                     /*  内核模块卸载                */
    }

    LW_LD_SAFEFREE(pmodule->EMOD_ppfuncInitArray);                      /*  销毁 C++ 全局对象构造与析构 */
    LW_LD_SAFEFREE(pmodule->EMOD_ppfuncFiniArray);

    LW_LD_SAFEFREE(pmodule->EMOD_pvUsedArr);

    LW_LD_SAFEFREE(pmodule->EMOD_pvFormatInfo);

    LW_LD_SAFEFREE(pmodule);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: moduleDelAndDestory
** 功能描述: 从全局模块链表或者进程模块链表删除模块，之后销毁模块.
** 输　入  : pmodule       模块指针
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT moduleDelAndDestory (LW_LD_EXEC_MODULE *pmodule)
{
    LW_LIST_RING      *pringTemp = LW_NULL;
    LW_LD_EXEC_MODULE *pmodTemp  = LW_NULL;
    LW_LD_VPROC       *pvproc;

    pvproc = pmodule->EMOD_pvproc;

    LW_VP_LOCK(pmodule->EMOD_pvproc);

    pringTemp = pvproc->VP_ringModules;
    if (!pringTemp) {
        LW_VP_UNLOCK(pmodule->EMOD_pvproc);
        return  (ERROR_NONE);
    }

    do {
        pmodTemp = _LIST_ENTRY(pringTemp, LW_LD_EXEC_MODULE, EMOD_ringModules);

        while (pvproc->VP_ringModules && pmodTemp->EMOD_ulRefs == 0) {
            pringTemp = _list_ring_get_next(pringTemp);
            _List_Ring_Del(&pmodTemp->EMOD_ringModules, &pvproc->VP_ringModules);
            
            moduleDestory(pmodTemp);
            pmodTemp = _LIST_ENTRY(pringTemp, LW_LD_EXEC_MODULE, EMOD_ringModules);
        }

        pringTemp = _list_ring_get_next(pringTemp);
    } while (pvproc->VP_ringModules && pringTemp != pvproc->VP_ringModules);

    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: moduleGetLibPath
** 功能描述: 获得库文件的路径
** 输　入  : pcParam       用户路径参数
**           pcPathBuffer  查找到的文件路径缓冲
**           stMaxLen      缓冲区大小
**           pcEnv         环境变量名
** 输　出  : 0
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT moduleGetLibPath (CPCHAR  pcFileName, PCHAR  pcPathBuffer, size_t  stMaxLen, CPCHAR  pcEnv)
{
    CHAR    cBuffer[MAX_FILENAME_LENGTH];

    PCHAR   pcStart;
    PCHAR   pcDiv;

    if (stMaxLen < 2) {                                                 /*  缓冲区大小错误              */
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    if (__ldPathIsFile(pcFileName)) {                                   /*  在当前目录下                */
        _PathGetFull(pcPathBuffer, stMaxLen, pcFileName);               /*  保存绝对路径                */
        return  (ERROR_NONE);
    
    } else {                                                            /*  不是一个路径                */
        if (lib_getenv_r(pcEnv, cBuffer, MAX_FILENAME_LENGTH)
            != ERROR_NONE) {                                            /*  环境变量值为空              */
            _ErrorHandle(ENOENT);                                       /*  无法找到文件                */
            return  (PX_ERROR);
        }
        
        pcPathBuffer[stMaxLen - 1] = PX_EOS;

        pcDiv = cBuffer;                                                /*  从第一个参数开始找          */
        do {
            pcStart = pcDiv;
            pcDiv   = lib_strchr(pcStart, ':');                         /*  查找下一个参数分割点        */
            if (pcDiv) {
                *pcDiv = PX_EOS;
                pcDiv++;
            }

            snprintf(pcPathBuffer, stMaxLen, "%s/%s", pcStart, pcFileName);
                                                                        /*  合并为完整的目录            */
            if (__ldPathIsFile(pcPathBuffer)) {                         /*  此文件可以被访问            */
                return  (ERROR_NONE);
            }
        } while (pcDiv);
    }

    _ErrorHandle(ENOENT);                                               /*  无法找到文件                */
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: moduleLoadSub
** 功能描述: 装载 sub 库 (仅对 .so 有效)
** 输　入  : pmodule      当前模块
**           pcLibName    依赖模块名称
**           bCreate      是否在没找到时创建，如果为 LW_FALSE，则只进行查找，不添加到列表
** 输　出  : 模块句柄
** 全局变量:
** 调用模块:
*********************************************************************************************************/
LW_LD_EXEC_MODULE *moduleLoadSub (LW_LD_EXEC_MODULE *pmodule, CPCHAR pcLibName, BOOL bCreate)
{
    LW_LD_EXEC_MODULE *pmoduleNeed = LW_NULL;
    CHAR               cLibPath[MAX_FILENAME_LENGTH];
    LW_LIST_RING      *pringTemp;
    CHAR              *pcFound;
    CHAR              *pcEntry;

    if (LW_NULL == pcLibName) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "invalid parameter\r\n");
        _ErrorHandle(ERROR_LOADER_PARAM_NULL);
        return  (LW_NULL);
    }

    if (LW_NULL == pmodule) {
        return  (LW_NULL);
    }
    /*
     *  查找依赖链表中已经加载的库
     */
    LW_VP_LOCK(pmodule->EMOD_pvproc);
    pringTemp = &pmodule->EMOD_ringModules;
    do {
        pmoduleNeed = _LIST_ENTRY(pringTemp, LW_LD_EXEC_MODULE, EMOD_ringModules);
        pcFound = lib_strstr(pmoduleNeed->EMOD_pcModulePath, pcLibName);
        if (NULL == pcFound) {
            pringTemp = _list_ring_get_next(pringTemp);
            continue;
        }

        if (pcFound + lib_strlen(pcLibName) == pmoduleNeed->EMOD_pcModulePath
                                            +  lib_strlen(pmoduleNeed->EMOD_pcModulePath)) {
            LW_VP_UNLOCK(pmodule->EMOD_pvproc);
            return  (pmoduleNeed);                                      /*  如果已加载该模块，直接返回  */
        }

        pringTemp = _list_ring_get_next(pringTemp);
    } while (pringTemp != &pmodule->EMOD_ringModules);
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    if (!bCreate) {                                                     /*  如果没有找到, 不需要创建    */
        return  (LW_NULL);
    }
    
    /*
     *  获取动态链接库位置
     */
    if (ERROR_NONE != moduleGetLibPath(pcLibName, cLibPath, MAX_FILENAME_LENGTH, "LD_LIBRARY_PATH")) {
        if (ERROR_NONE != moduleGetLibPath(pcLibName, cLibPath, MAX_FILENAME_LENGTH, "PATH")) {
            fprintf(stderr, "can not find dependent library: %s\n", pcLibName);
            _ErrorHandle(ERROR_LOADER_NO_MODULE);
            return  (LW_NULL);
        }
    }

    if (pmodule->EMOD_bIsSymbolEntry) {
        pcEntry = LW_NULL;                                              /*  已找到了进程入口, 不必找了  */
    } else {
        pcEntry = pmodule->EMOD_pcEntry;                                /*  需要在此库中寻找进程入口    */
    }

    pmoduleNeed = moduleCreate(cLibPath,
                               LW_TRUE,
                               LW_TRUE,                                 /*  自动装载的库, 符号必须为全局*/
                               pmodule->EMOD_pcInit,
                               pmodule->EMOD_pcExit,
                               pcEntry, 
                               LW_NULL,
                               pmodule->EMOD_pvproc);
                                                                        /*  创建模块                    */
    if (LW_NULL == pmoduleNeed) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "create module error.\r\n");
        _ErrorHandle(ERROR_LOADER_CREATE);
        return  (LW_NULL);
    }

    pringTemp = &pmodule->EMOD_ringModules;
    LW_VP_LOCK(pmodule->EMOD_pvproc);
    _List_Ring_Add_Front(&pmoduleNeed->EMOD_ringModules, &pringTemp);
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    return  (pmoduleNeed);
}
/*********************************************************************************************************
** 函数名称: initArrayCall
** 功能描述: 调用初始化函数列表. (一般为 C++ 全局对象构造函数列表)
** 输　入  : pmodule       模块指针
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT initArrayCall (LW_LD_EXEC_MODULE *pmodule)
{
    INT             i;
    VOIDFUNCPTR     pfuncInit    = LW_NULL;
    LW_LD_EXEC_MODULE *pmodTemp  = LW_NULL;
    LW_LIST_RING      *pringTemp = _list_ring_get_prev(&pmodule->EMOD_ringModules);

    /*
     *  这里的执行顺序非常重要, 首先, 依存库之间必须要先调用最底层的库的初始化函数组, 然后一级一级向上层库
     *  调用, 库内部的初始化函数组按顺序执行就可以了.
     */
    LW_VP_LOCK(pmodule->EMOD_pvproc);
    do {
        pmodTemp = _LIST_ENTRY(pringTemp, LW_LD_EXEC_MODULE, EMOD_ringModules);

        if (pmodTemp->EMOD_ulStatus == LW_LD_STATUS_LOADED) {
            pmodTemp->EMOD_ulStatus = LW_LD_STATUS_INITED;              /*  处理构造函数调用opendl      */

            for (i = 0; i < pmodTemp->EMOD_ulInitArrCnt; i++) {         /*  正顺序调用初始化函数        */
                pfuncInit = pmodTemp->EMOD_ppfuncInitArray[i];
                if (pfuncInit != LW_NULL && pfuncInit != (VOIDFUNCPTR)(~0)) {
                    pfuncInit();
                }
            }

            if (pmodTemp->EMOD_pfuncInit) {
                if (pmodTemp->EMOD_pfuncInit() < 0) {
                    LW_VP_UNLOCK(pmodule->EMOD_pvproc);
                    return  (PX_ERROR);
                }
            }
        }

        pringTemp = _list_ring_get_prev(pringTemp);
    } while (pringTemp != _list_ring_get_prev(&pmodule->EMOD_ringModules));
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: finiArrayCall
** 功能描述: 调用结束函数列表. (一般为 C++ 全局对象析构函数列表)
** 输　入  : pmodule       模块指针
**           bRunFini      是否运行析构函数表.
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT finiArrayCall (LW_LD_EXEC_MODULE *pmodule, BOOL  bRunFini)
{
    INT             i;
    VOIDFUNCPTR     pfuncFini    = LW_NULL;
    LW_LD_EXEC_MODULE *pmodTemp  = pmodule;
    LW_LIST_RING      *pringTemp = &pmodule->EMOD_ringModules;

    /*
     *  这里的执行顺序非常重要, 首先, 依存库之间必须要先调用最上层的库的卸载函数组, 然后一级一级向下层库
     *  调用, 库内部的初始化函数组按逆序执行!
     */
    LW_VP_LOCK(pmodule->EMOD_pvproc);
    do {
        pmodTemp = _LIST_ENTRY(pringTemp, LW_LD_EXEC_MODULE, EMOD_ringModules);

        if (pmodTemp->EMOD_ulRefs == 0 && pmodTemp->EMOD_ulStatus == LW_LD_STATUS_INITED) {
            pmodTemp->EMOD_ulStatus = LW_LD_STATUS_FINIED;

            if (pmodTemp->EMOD_pfuncExit) {
                pmodTemp->EMOD_pfuncExit();
            }
            
            if (bRunFini) {                                             /*  逆序调用结束函数           */
                for (i = (INT)pmodTemp->EMOD_ulFiniArrCnt - 1; i >= 0; i--) {
                    pfuncFini = pmodTemp->EMOD_ppfuncFiniArray[i];
                    if (pfuncFini != LW_NULL && pfuncFini != (VOIDFUNCPTR)(~0)) {
                        pfuncFini();
                    }
                }
            }

            __cxa_module_finalize(pmodTemp->EMOD_pvBaseAddr,
                                  pmodTemp->EMOD_stLen,
                                  bRunFini);                            /*  释放当前模块的 cxx_atexit   */
        }

        pringTemp = _list_ring_get_next(pringTemp);
    } while (pringTemp != &pmodule->EMOD_ringModules);
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: moduleCleanup
** 功能描述: 表示所有模块引用计数器为0，用于情况进程模块链表
** 输　入  : pmodule       模块指针
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT moduleCleanup (LW_LD_EXEC_MODULE *pmodule)
{
    LW_LD_EXEC_MODULE *pmodTemp  = pmodule;
    LW_LIST_RING      *pringTemp = &pmodule->EMOD_ringModules;

    LW_VP_LOCK(pmodule->EMOD_pvproc);
    do {
        pmodTemp = _LIST_ENTRY(pringTemp, LW_LD_EXEC_MODULE, EMOD_ringModules);

        pmodTemp->EMOD_ulRefs = 0;

        pringTemp = _list_ring_get_next(pringTemp);
    } while (pringTemp != &pmodule->EMOD_ringModules);
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: moduleDelRef
** 功能描述: 递减模块引用计数，如果为0，则递减其子模块引用计数，如此递归
** 输　入  : pmodule       模块指针
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT moduleDelRef (LW_LD_EXEC_MODULE *pmodule)
{
    LW_LD_EXEC_MODULE  *pmodTemp    = pmodule;
    LW_LD_EXEC_MODULE **pmodUsedArr = LW_NULL;
    LW_LIST_RING       *pringTemp;
    BOOL                bUpdated    = LW_TRUE;
    INT                 i;

    LW_VP_LOCK(pmodule->EMOD_pvproc);
    if (pmodule->EMOD_ulRefs > 0) {
        pmodule->EMOD_ulRefs--;
    }

    while (bUpdated) {
        bUpdated  = LW_FALSE;
        pringTemp = &pmodule->EMOD_ringModules;
        do {
            pmodTemp = _LIST_ENTRY(pringTemp, LW_LD_EXEC_MODULE, EMOD_ringModules);

            if (pmodTemp->EMOD_ulRefs == 0 && pmodTemp->EMOD_pvUsedArr) {
                pmodUsedArr = (LW_LD_EXEC_MODULE **)pmodTemp->EMOD_pvUsedArr;
                for (i = 0; i < pmodTemp->EMOD_ulUsedCnt; i++) {
                    if (pmodUsedArr[i] && pmodUsedArr[i]->EMOD_ulRefs > 0) {
                        pmodUsedArr[i]->EMOD_ulRefs--;
                        bUpdated = LW_TRUE;
                    }
                }

                LW_LD_SAFEFREE(pmodTemp->EMOD_pvUsedArr);
            }

            pringTemp = _list_ring_get_next(pringTemp);
        } while (pringTemp != &pmodule->EMOD_ringModules);
    }
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: moduleRebootHook
** 功能描述: 系统重新启动时, 将调用所有内核模块的 module_exit 函数
** 输　入  : iRebootType   重启类型
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID  moduleRebootHook (INT  iRebootType)
{
    LW_LD_VPROC        *pvproc = &_G_vprocKernel;
    LW_LD_EXEC_MODULE  *pmodule;
    LW_LIST_RING       *pringTemp;
    BOOL                bStart;
    BOOL                bRunFini = (iRebootType == LW_REBOOT_FORCE) ? LW_FALSE : LW_TRUE;

    LW_VP_LOCK(pvproc);
    for (pringTemp  = pvproc->VP_ringModules, bStart = LW_TRUE;
         pringTemp && (pringTemp != pvproc->VP_ringModules || bStart);
         pringTemp  = _list_ring_get_next(pringTemp), bStart = LW_FALSE) {
         
        pmodule = _LIST_ENTRY(pringTemp, LW_LD_EXEC_MODULE, EMOD_ringModules);
        
        LW_VP_UNLOCK(pvproc);
        
        moduleCleanup(pmodule);
        
        finiArrayCall(pmodule, bRunFini);
        
        LW_VP_LOCK(pvproc);
    }
    LW_VP_UNLOCK(pvproc);
}
/*********************************************************************************************************
** 函数名称: API_ModuleStatus
** 功能描述: 查看elf文件信息.
** 输　入  : pcFile        文件路径
**           iFd           信息输出文件句柄
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
INT  API_ModuleStatus (CPCHAR  pcFile, INT  iFd)
{
    return  __elfStatus(pcFile, iFd);
}
/*********************************************************************************************************
** 函数名称: API_ModuleLoad
** 功能描述: 以模块方式加载elf文件. 
             (iMode == GLOBAL && pvVProc == NULL 此模块在整个内核都有效)
** 输　入  : pcFile        文件路径
**           iMode         装载模式 (全局还是局部)
**           pcInit        初始化函数名，如果为LW_NULL，表示不需要条用初始化函数
**           pcExit        模块退出时运行的函数, 如果为LW_NULL，表示不需要退出函数
**           pvVProc       指向的进程主模块
** 输　出  : 模块句柄
** 全局变量:
** 调用模块:
** 注  意  : 函数不装在 pcInit 到符号表.
                                           API 函数
*********************************************************************************************************/
LW_API
PVOID  API_ModuleLoad (CPCHAR  pcFile,
                       INT     iMode,
                       CPCHAR  pcInit,
                       CPCHAR  pcExit,
                       PVOID   pvVProc)
{
    return API_ModuleLoadEx(pcFile, iMode, pcInit, pcExit, LW_NULL, LW_NULL, pvVProc);
}
/*********************************************************************************************************
** 函数名称: API_ModuleLoad
** 功能描述: 以模块方式加载elf文件. 仅从指定的 sect 中加载符号表, 主要用于装在系统模块 
             (iMode == GLOBAL && pvVProc == NULL 此模块在整个内核都有效)
** 输　入  : pcFile        文件路径
**           iMode         装载模式 (全局还是局部)
**           pcInit        初始化函数名，如果为LW_NULL，表示不需要条用初始化函数
**           pcExit        模块退出时运行的函数, 如果为LW_NULL，表示不需要退出函数
**           pcEntry       模块入口函数
**           pcSection     指定的 section
**           pvVProc       指向的进程控制块
** 输　出  : 模块句柄
** 全局变量:
** 调用模块:
** 注  意  : 函数不装在 pcInit 到符号表.
                                           API 函数
*********************************************************************************************************/
LW_API
PVOID  API_ModuleLoadEx (CPCHAR  pcFile,
                         INT     iMode,
                         CPCHAR  pcInit,
                         CPCHAR  pcExit,
                         CPCHAR  pcEntry,
                         CPCHAR  pcSection,
                         PVOID   pvVProc)
{
    CHAR               cLibPath[MAX_FILENAME_LENGTH];
    LW_LD_EXEC_MODULE *pmodule   = LW_NULL;
    LW_LD_EXEC_MODULE *pmodVProc = LW_NULL;
    LW_LD_VPROC       *pvproc;
    BOOL               bIsGlobal;
    PCHAR              pcFileName;

    if (LW_NULL == pcFile) {
        _ErrorHandle(ERROR_LOADER_PARAM_NULL);
        return  (LW_NULL);
    }

    if (iMode & LW_OPTION_LOADER_SYM_GLOBAL) {
        bIsGlobal = LW_TRUE;
    } else {
        bIsGlobal = LW_FALSE;
    }
    
    _PathLastName(pcFile, &pcFileName);                                 /*  取出文件名                  */

    pvproc = (LW_LD_VPROC *)pvVProc;
    if (LW_NULL == pvproc) {                                            /*  内核模块                    */
        uid_t   euid = geteuid();
        if (euid != 0) {
            _ErrorHandle(EACCES);                                       /*  没有权限                    */
            return  (LW_NULL);
        }
        pvproc = &_G_vprocKernel;
    }

    if (pvproc->VP_ringModules) {
        pmodVProc = _LIST_ENTRY(pvproc->VP_ringModules, LW_LD_EXEC_MODULE, EMOD_ringModules);
    }

    if (pmodVProc) {
        pmodule = moduleLoadSub(pmodVProc, pcFileName, LW_FALSE);       /*  查找进程已装载模块链表      */
    }

    if (LW_NULL == pmodule) {
        /*
        *  获取动态链接库位置
        */
        if (ERROR_NONE != moduleGetLibPath(pcFile, cLibPath, MAX_FILENAME_LENGTH, "LD_LIBRARY_PATH")) {
            if (ERROR_NONE != moduleGetLibPath(pcFile, cLibPath, MAX_FILENAME_LENGTH, "PATH")) {
                fprintf(stderr, "can not find dependent library: %s\n", pcFile);
                _ErrorHandle(ERROR_LOADER_NO_MODULE);
                return  (LW_NULL);
            }
        }
    
        pmodule = moduleCreate(cLibPath, LW_TRUE, bIsGlobal,
                               pcInit, pcExit, pcEntry,
                               pcSection, pvproc);                      /*  创建新子模块                */
        if (pmodule) {
            LW_VP_LOCK(pmodule->EMOD_pvproc);
            _List_Ring_Add_Last(&pmodule->EMOD_ringModules, &pvproc->VP_ringModules);
            LW_VP_UNLOCK(pmodule->EMOD_pvproc);
        }
    } else {
        if (bIsGlobal) {                                                /*  只要一次global加载则为global*/
            pmodule->EMOD_bIsGlobal = bIsGlobal;
        }

        if (LW_NULL == pmodule->EMOD_pvproc) {
            pmodule->EMOD_pvproc = (LW_LD_VPROC *)pvVProc;
        }
    }
                                                                        /*  创建模块                    */
    if (LW_NULL == pmodule) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "create module error\r\n");
        _ErrorHandle(ERROR_LOADER_CREATE);
        return  (LW_NULL);
    }

    LW_VP_LOCK(pmodule->EMOD_pvproc);
    pmodule->EMOD_ulRefs++;
    if (__elfListLoad(pmodule, cLibPath) < 0) {                         /*  加载elf文件                 */
        LW_VP_UNLOCK(pmodule->EMOD_pvproc);
        moduleDelRef(pmodule);
        moduleDelAndDestory(pmodule);
        return  (LW_NULL);
    }
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    if (pvproc->VP_ringModules == &pmodule->EMOD_ringModules) {         /*  如果是首个模块则初始化进程  */
        __moduleVpPatchInit(pmodule);
    }

    if (ERROR_NONE != initArrayCall(pmodule)) {                         /*  调用c++初始化代码           */
        API_ModuleUnload(pmodule);
        return  (LW_NULL);
    }

    return  ((PVOID)pmodule);
}
/*********************************************************************************************************
** 函数名称: API_ModuleUnload
** 功能描述: 卸载已经加载的elf文件.
** 输　入  : pvModule      模块句柄
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
INT  API_ModuleUnload (PVOID  pvModule)
{
    LW_LD_EXEC_MODULE *pmodule = (LW_LD_EXEC_MODULE *)pvModule;
    LW_LD_VPROC       *pvproc  = pmodule->EMOD_pvproc;

    if ((pmodule == LW_NULL) || (pmodule->EMOD_ulMagic != __LW_LD_EXEC_MODULE_MAGIC)) {
        _ErrorHandle(ERROR_LOADER_PARAM_NULL);
        return  (PX_ERROR);
    }
    
    if (pvproc == &_G_vprocKernel) {
        uid_t   euid = geteuid();
        if (euid != 0) {
            _ErrorHandle(EACCES);                                       /*  没有权限                    */
            return  (PX_ERROR);
        }
    }

    moduleDelRef(pmodule);

    finiArrayCall(pmodule, LW_TRUE);                                    /*  调用c++析构函数代码         */

    LW_VP_LOCK(pmodule->EMOD_pvproc);
    __elfListUnload(pmodule);
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    moduleDelAndDestory(pmodule);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_ModuleFinish
** 功能描述: 除了不回收内存空间以外, 清除进程的一切信息
** 输　入  : pvproc     进程控制块指针
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
INT API_ModuleFinish (PVOID pvVProc)
{
    LW_LD_VPROC       *pvproc = (LW_LD_VPROC *)pvVProc;
    LW_LD_EXEC_MODULE *pmodule;

    if (pvproc == LW_NULL) {
        _ErrorHandle(ERROR_LOADER_PARAM_NULL);
        return  (PX_ERROR);
    }

    if (pvproc->VP_ringModules == LW_NULL) {                            /*  是否已经进行了回收          */
        return  (ERROR_NONE);
    }

    pmodule = _LIST_ENTRY(pvproc->VP_ringModules, LW_LD_EXEC_MODULE, EMOD_ringModules);

    moduleCleanup(pmodule);

    finiArrayCall(pmodule, !pvproc->VP_bForceTerm);                     /*  调用c++析构函数代码         */

    __moduleVpPatchFini(pmodule);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_ModuleTerminal
** 功能描述: 清空进程中已经加载的elf文件. (首先应该调用 API_ModuleFinish 才能待用此函数)
** 输　入  : pvproc     进程控制块指针
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
INT API_ModuleTerminal (PVOID pvVProc)
{
    LW_LD_VPROC       *pvproc = (LW_LD_VPROC *)pvVProc;
    LW_LD_EXEC_MODULE *pmodule;

    if (pvproc == LW_NULL) {
        _ErrorHandle(ERROR_LOADER_PARAM_NULL);
        return  (PX_ERROR);
    }

    if (pvproc->VP_ringModules == LW_NULL) {                            /*  是否已经进行了回收          */
        return  (ERROR_NONE);
    }

    pmodule = _LIST_ENTRY(pvproc->VP_ringModules, LW_LD_EXEC_MODULE, EMOD_ringModules);

    LW_VP_LOCK(pmodule->EMOD_pvproc);
    __elfListUnload(pmodule);
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);

    moduleDelAndDestory(pmodule);
    
    pvproc->VP_ringModules = LW_NULL;                                   /*  进程不再含有模块            */

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_ModuleShareRefresh
** 功能描述: 清除(刷新)操作系统对于共享库共享信息的缓冲, 之后装载的共享库, 将从新计算共享代码段.
**           主要用于更新正在运行程序或者动态库, 更新之前必须首先运行此函数,
** 输　入  : pcFileName    共享库或应用程序名 (NULL 表示全部清除)
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
INT  API_ModuleShareRefresh (CPCHAR  pcFileName)
{
    struct stat64  stat64Get;

    if (pcFileName == LW_NULL) {
        LW_LD_VMSAFE_SHARE_ABORT((dev_t)-1, (ino64_t)-1);
        
        MONITOR_EVT(MONITOR_EVENT_ID_LOADER, MONITOR_EVENT_LOADER_REFRESH, LW_NULL);
    
    } else {
        if (stat64(pcFileName, &stat64Get)) {
            return  (PX_ERROR);
        }
    
        LW_LD_VMSAFE_SHARE_ABORT(stat64Get.st_dev, stat64Get.st_ino);
        
        MONITOR_EVT(MONITOR_EVENT_ID_LOADER, MONITOR_EVENT_LOADER_REFRESH, pcFileName);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_ModuleShareConfig
** 功能描述: 设置共享库装载器,
** 输　入  : bShare        是否是能段共享
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
INT  API_ModuleShareConfig (BOOL  bShare)
{
    return  (LW_LD_VMSAFE_SHARE_CONFIG(bShare, LW_NULL));
}
/*********************************************************************************************************
** 函数名称: __moduleTreeFindSym
** 功能描述: 递归查找模块内符号.
** 输　入  : pmodule       模块指针
**           pcSymName     符号名
**           pulSymVal     符号值
**           iFlag         符号属性
**           iLayer        最大递归级数
** 输　出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static INT __moduleTreeFindSym (LW_LD_EXEC_MODULE  *pmodule,
                                CPCHAR              pcSymName,
                                addr_t             *pulSymVal,
                                INT                 iFlag,
                                INT                 iLayer)
{
    INT                 i;
    LW_LD_EXEC_MODULE **pmodUsedArr = LW_NULL;

    if (ERROR_NONE == __moduleFindSym(pmodule, pcSymName, pulSymVal, iFlag)) {
        return  (ERROR_NONE);
    }

    iLayer--;
    if (iLayer == 0) {                                                  /*  达到最大递归级数            */
        return  (PX_ERROR);
    }

    pmodUsedArr = (LW_LD_EXEC_MODULE **)pmodule->EMOD_pvUsedArr;
    if (!pmodUsedArr) {
        return  (PX_ERROR);
    }

    for (i = 0; i < pmodule->EMOD_ulUsedCnt; i++) {
        if (LW_NULL == pmodUsedArr[i]) {
            continue;
        }
        
        if (__moduleTreeFindSym(pmodUsedArr[i],
                                pcSymName,
                                pulSymVal,
                                iFlag,
                                iLayer) == ERROR_NONE) {                /*  递归查找                    */
            return  (ERROR_NONE);
        }
    }

    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_ModuleSym
** 功能描述: 查询模块中的指定符号地址
** 输　入  : pvModule      模块句柄
**           pcName        函数名
** 输　出  : 函数地址
** 全局变量:
** 调用模块:
                                           API 函数
*********************************************************************************************************/
LW_API
PVOID  API_ModuleSym (PVOID  pvModule, CPCHAR  pcName)
{
    LW_LD_EXEC_MODULE *pmodule = (LW_LD_EXEC_MODULE *)pvModule;
    INT                iError;
    INT                iLayer = 20;                                     /*  最大递归 20 次              */
    addr_t             ulValue = (addr_t)LW_NULL;
    
    if ((pmodule == LW_NULL) || (pmodule->EMOD_ulMagic != __LW_LD_EXEC_MODULE_MAGIC)) {
        _ErrorHandle(ERROR_LOADER_PARAM_NULL);
        return  (LW_NULL);
    }
    
    LW_VP_LOCK(pmodule->EMOD_pvproc);                                   /*  需要锁定 vproc              */
    iError = __moduleTreeFindSym(pmodule, pcName, &ulValue, LW_LD_SYM_ANY, iLayer);
    LW_VP_UNLOCK(pmodule->EMOD_pvproc);
    
    if (iError) {
        _ErrorHandle(ERROR_LOADER_NO_SYMBOL);
    }

    return  ((PVOID)ulValue);
}
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
