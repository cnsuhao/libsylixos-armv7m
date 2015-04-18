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
** 文   件   名: procBsp.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 12 月 03 日
**
** 描        述: proc 文件系统 bsp 信息文件.

** BUG:
2009.12.11  生成文件内容后需要调用 API_ProcFsNodeSetRealFileSize(p_pfsn, stRealSize); 回写文件实际大小.
2010.01.07  修改一些 errno.
2010.08.11  简化 read 操作.
2011.03.04  proc 文件 mode 为 S_IFREG.
2013.05.04  加入 self/auxv 文件.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "../procFs.h"
#include "elf.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_PROCFS_EN > 0) && (LW_CFG_PROCFS_BSP_INFO > 0)
/*********************************************************************************************************
  bsp proc 文件函数声明
*********************************************************************************************************/
static ssize_t  __procFsBspMemRead(PLW_PROCFS_NODE  p_pfsn, 
                                   PCHAR            pcBuffer, 
                                   size_t           stMaxBytes,
                                   off_t            oft);
static ssize_t  __procFsBspCpuRead(PLW_PROCFS_NODE  p_pfsn, 
                                   PCHAR            pcBuffer, 
                                   size_t           stMaxBytes,
                                   off_t            oft);
static ssize_t  __procFsBspAuxvRead(PLW_PROCFS_NODE  p_pfsn, 
                                    PCHAR            pcBuffer, 
                                    size_t           stMaxBytes,
                                    off_t            oft);
/*********************************************************************************************************
  bsp proc 文件操作函数组
*********************************************************************************************************/
static LW_PROCFS_NODE_OP        _G_pfsnoBspMemFuncs = {
    __procFsBspMemRead, LW_NULL
};
static LW_PROCFS_NODE_OP        _G_pfsnoBspCpuFuncs = {
    __procFsBspCpuRead, LW_NULL
};
static LW_PROCFS_NODE_OP        _G_pfsnoBspAuxvFuncs = {
    __procFsBspAuxvRead, LW_NULL
};
/*********************************************************************************************************
  bsp proc 文件目录树
*********************************************************************************************************/
#define __PROCFS_BUFFER_SIZE_BSPMEM     1024
#define __PROCFS_BUFFER_SIZE_CPUINFO    1024
#define __PROCFS_BUFFER_SIZE_AUXV       1024

static LW_PROCFS_NODE           _G_pfsnBsp[] = 
{
    LW_PROCFS_INIT_NODE("self", (S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH), 
                        LW_NULL, LW_NULL,  0),
    
    LW_PROCFS_INIT_NODE("bspmem",  (S_IRUSR | S_IRGRP | S_IROTH | S_IFREG), 
                        &_G_pfsnoBspMemFuncs, "M", __PROCFS_BUFFER_SIZE_BSPMEM),
                        
    LW_PROCFS_INIT_NODE("cpuinfo", (S_IRUSR | S_IRGRP | S_IROTH | S_IFREG), 
                        &_G_pfsnoBspCpuFuncs, "C", __PROCFS_BUFFER_SIZE_CPUINFO),
                        
    LW_PROCFS_INIT_NODE("auxv", (S_IRUSR | S_IRGRP | S_IROTH | S_IFREG), 
                        &_G_pfsnoBspAuxvFuncs, "A", __PROCFS_BUFFER_SIZE_AUXV),
};
/*********************************************************************************************************
** 函数名称: __procFsBspMemRead
** 功能描述: procfs 读一个 bspmem 文件
** 输　入  : p_pfsn        节点控制块
**           pcBuffer      缓冲区
**           stMaxBytes    缓冲区大小
**           oft           文件指针
** 输　出  : 实际读取的数目
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static ssize_t  __procFsBspMemRead (PLW_PROCFS_NODE  p_pfsn, 
                                    PCHAR            pcBuffer, 
                                    size_t           stMaxBytes,
                                    off_t            oft)
{
    REGISTER PCHAR      pcFileBuffer;
             size_t     stRealSize;                                     /*  实际的文件内容大小          */
             size_t     stCopeBytes;

    /*
     *  程序运行到这里, 文件缓冲一定已经分配了预置的内存大小.
     */
    pcFileBuffer = (PCHAR)API_ProcFsNodeBuffer(p_pfsn);
    if (pcFileBuffer == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (0);
    }
     
    stRealSize = API_ProcFsNodeGetRealFileSize(p_pfsn);
    if (stRealSize == 0) {                                              /*  需要生成文件                */
        stRealSize = bnprintf(pcFileBuffer, __PROCFS_BUFFER_SIZE_BSPMEM, 0,
                 "ROM SIZE: 0x%08zx Bytes (0x%08lx - 0x%08lx)\n"
                 "RAM SIZE: 0x%08zx Bytes (0x%08lx - 0x%08lx)\n"
                 "use \"mems\" \"zones\" \"virtuals\"... can print memory usage factor.\n",
                 bspInfoRomSize(),
                 bspInfoRomBase(),
                 (bspInfoRomBase() + bspInfoRomSize() - 1),
                 bspInfoRamSize(),
                 bspInfoRamBase(),
                 (bspInfoRamBase() + bspInfoRamSize() - 1));            /*  将信息打印到缓冲            */
        API_ProcFsNodeSetRealFileSize(p_pfsn, stRealSize);
    }
    if (oft >= stRealSize) {
        _ErrorHandle(ENOSPC);
        return  (0);
    }
    
    stCopeBytes  = __MIN(stMaxBytes, (size_t)(stRealSize - oft));       /*  计算实际读取的数量          */
    lib_memcpy(pcBuffer, (CPVOID)(pcFileBuffer + oft), (UINT)stCopeBytes);
    
    return  ((ssize_t)stCopeBytes);
}
/*********************************************************************************************************
** 函数名称: __procFsBspCpuRead
** 功能描述: procfs 读一个 cpuinfo 文件
** 输　入  : p_pfsn        节点控制块
**           pcBuffer      缓冲区
**           stMaxBytes    缓冲区大小
**           oft           文件指针
** 输　出  : 实际读取的数目
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static ssize_t  __procFsBspCpuRead (PLW_PROCFS_NODE  p_pfsn, 
                                    PCHAR            pcBuffer, 
                                    size_t           stMaxBytes,
                                    off_t            oft)
{
    REGISTER PCHAR      pcFileBuffer;
             size_t     stRealSize;                                     /*  实际的文件内容大小          */
             size_t     stCopeBytes;

    /*
     *  程序运行到这里, 文件缓冲一定已经分配了预置的内存大小.
     */
    pcFileBuffer = (PCHAR)API_ProcFsNodeBuffer(p_pfsn);
    if (pcFileBuffer == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (0);
    }
    
    stRealSize = API_ProcFsNodeGetRealFileSize(p_pfsn);
    if (stRealSize == 0) {                                              /*  需要生成文件                */
        PCHAR   pcPowerLevel;
        UINT    uiPowerLevel;
        ULONG   ulActive;
        
#if LW_CFG_POWERM_EN > 0
        API_PowerMCpuGet(&ulActive, &uiPowerLevel);
#else
        ulActive     = 1;
        uiPowerLevel = LW_CPU_POWERLEVEL_TOP;
#endif                                                                  /*  LW_CFG_POWERM_EN > 0        */
        
        switch (uiPowerLevel) {
        
        case LW_CPU_POWERLEVEL_TOP:
            pcPowerLevel = "Top level";
            break;
            
        case LW_CPU_POWERLEVEL_FAST:
            pcPowerLevel = "Fast level";
            break;
        
        case LW_CPU_POWERLEVEL_NORMAL:
            pcPowerLevel = "Normal level";
            break;
        
        case LW_CPU_POWERLEVEL_SLOW:
            pcPowerLevel = "Slow level";
            break;
            
        default:
            pcPowerLevel = "<unknown> level";
            break;
        }
        
        stRealSize = bnprintf(pcFileBuffer, __PROCFS_BUFFER_SIZE_CPUINFO, 0,
                 "CPU      : %s\n"
                 "WORDLEN  : %d\n"
                 "NCPU     : %d\n"
                 "ACTIVE   : %d\n"
                 "PWRLevel : %s\n"
                 "CACHE    : %s\n"
                 "PACKET   : %s\n",
                 bspInfoCpu(),
                 LW_CFG_CPU_WORD_LENGHT,
                 (INT)LW_NCPUS,
                 (INT)ulActive,
                 pcPowerLevel,
                 bspInfoCache(),
                 bspInfoPacket());                                      /*  将信息打印到缓冲            */
        API_ProcFsNodeSetRealFileSize(p_pfsn, stRealSize);
    }
    if (oft >= stRealSize) {
        _ErrorHandle(ENOSPC);
        return  (0);
    }
    
    stCopeBytes  = __MIN(stMaxBytes, (size_t)(stRealSize - oft));       /*  计算实际读取的数量          */
    lib_memcpy(pcBuffer, (CPVOID)(pcFileBuffer + oft), (UINT)stCopeBytes);
    
    return  ((ssize_t)stCopeBytes);
}
/*********************************************************************************************************
** 函数名称: __procFsBspAuxvRead
** 功能描述: procfs 读一个 self/auxv 文件
** 输　入  : p_pfsn        节点控制块
**           pcBuffer      缓冲区
**           stMaxBytes    缓冲区大小
**           oft           文件指针
** 输　出  : 实际读取的数目
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static ssize_t  __procFsBspAuxvRead (PLW_PROCFS_NODE  p_pfsn, 
                                     PCHAR            pcBuffer, 
                                     size_t           stMaxBytes,
                                     off_t            oft)
{
    REGISTER PCHAR      pcFileBuffer;
             size_t     stRealSize;                                     /*  实际的文件内容大小          */
             size_t     stCopeBytes;
             Elf_auxv_t *pelfauxv;
    
    /*
     *  程序运行到这里, 文件缓冲一定已经分配了预置的内存大小.
     */
    pcFileBuffer = (PCHAR)API_ProcFsNodeBuffer(p_pfsn);
    if (pcFileBuffer == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (0);
    }
    
    stRealSize = API_ProcFsNodeGetRealFileSize(p_pfsn);
    if (stRealSize == 0) {                                              /*  需要生成文件                */
        pelfauxv = (Elf_auxv_t *)pcFileBuffer;
        pelfauxv->a_type     = AT_UID;
        pelfauxv->a_un.a_val = getuid();
        pelfauxv++;
        
        pelfauxv->a_type     = AT_EUID;
        pelfauxv->a_un.a_val = geteuid();
        pelfauxv++;
        
        pelfauxv->a_type     = AT_GID;
        pelfauxv->a_un.a_val = getgid();
        pelfauxv++;
        
        pelfauxv->a_type     = AT_EGID;
        pelfauxv->a_un.a_val = getegid();
        pelfauxv++;
        
        pelfauxv->a_type     = AT_HWCAP;
        pelfauxv->a_un.a_val = (Elf_Word)bspInfoHwcap();
        pelfauxv++;
        
        pelfauxv->a_type     = AT_CLKTCK;
        pelfauxv->a_un.a_val = LW_TICK_HZ;
        pelfauxv++;
        
        pelfauxv->a_type     = AT_PAGESZ;
        pelfauxv->a_un.a_val = LW_CFG_VMM_PAGE_SIZE;
        pelfauxv++;
        
        pelfauxv->a_type     = AT_NULL;
        pelfauxv->a_un.a_val = 0;
        pelfauxv++;
        
        stRealSize = (size_t)pelfauxv - (size_t)pcFileBuffer;
        
        API_ProcFsNodeSetRealFileSize(p_pfsn, stRealSize);
    }
    if (oft >= stRealSize) {
        _ErrorHandle(ENOSPC);
        return  (0);
    }
    
    stCopeBytes  = __MIN(stMaxBytes, (size_t)(stRealSize - oft));       /*  计算实际读取的数量          */
    lib_memcpy(pcBuffer, (CPVOID)(pcFileBuffer + oft), (UINT)stCopeBytes);
    
    return  ((ssize_t)stCopeBytes);
}
/*********************************************************************************************************
** 函数名称: __procFsBspInfoInit
** 功能描述: procfs 初始化 Bsp proc 文件
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __procFsBspInfoInit (VOID)
{
    API_ProcFsMakeNode(&_G_pfsnBsp[0], "/");
    API_ProcFsMakeNode(&_G_pfsnBsp[1], "/");
    API_ProcFsMakeNode(&_G_pfsnBsp[2], "/");
    API_ProcFsMakeNode(&_G_pfsnBsp[3], "/self");
}
#endif                                                                  /*  LW_CFG_PROCFS_EN > 0        */
                                                                        /*  LW_CFG_PROCFS_BSP_INFO      */
/*********************************************************************************************************
  END
*********************************************************************************************************/
