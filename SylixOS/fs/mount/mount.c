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
** 文   件   名: mount.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 09 月 26 日
**
** 描        述: mount 挂载库, 这是 SylixOS 第二类块设备挂载方式.
                 详细见 blockIo.h (不推荐使用! 可能存在问题)
** BUG:
2012.03.10  加入对 NFS 的支持.
2012.03.12  API_Mount() 节点记录绝对路径, 删除节点也使用绝对路径.
2012.03.21  加入 API_MountShow() 函数.
2012.04.11  加入独立的 mount 锁, 当设备卸载失败时, umount 失败.
2012.06.27  第二类块设备支持直接读写数据接口.
            当物理设备不是块设备时, 可以使用 __LW_MOUNT_DEFAULT_SECSIZE 进行操作, 这样可以直接无缝挂载
            文件系统镜像文件, 例如: romfs 文件.
2012.08.16  使用 pread 与 pwrite 代替 lseek->read/write 操作.
2012.09.01  支持卸载非 mount 设备.
2012.12.07  将默认文件系统设置为 vfat.
2012.12.08  非 block 文件磁盘需要过滤 ioctl 命令.
2012.12.25  mount 和 umount 必须在内核空间执行, 以保证创建出来的文件描述符为内核文件描述符, 所遇进程在内核
            空间时均可以访问.
2013.04.02  加入 sys/mount.h 支持.
2013.06.25  logic 设备 BLKD_pvLink 不能为 NULL.
2014.05.24  加入对 ramfs 支持.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "../SylixOS/fs/include/fs_fs.h"
#include "../SylixOS/fs/fsCommon/fsCommon.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_MAX_VOLUMES > 0) && (LW_CFG_MOUNT_EN > 0)
#include "sys/mount.h"
/*********************************************************************************************************
  宏定义
*********************************************************************************************************/
#define __LW_MOUNT_DEFAULT_FS       "vfat"                              /*  默认挂载文件系统格式        */
#define __LW_MOUNT_NFS_FS           "nfs"                               /*  nfs 挂载                    */
#define __LW_MOUNT_RAM_FS           "ramfs"                             /*  ram 挂载                    */
/*********************************************************************************************************
  默认参数
*********************************************************************************************************/
#define __LW_MOUNT_DEFAULT_SECSIZE  512                                 /*  当 dev 不是块设备时扇区大小 */
/*********************************************************************************************************
  挂载节点
*********************************************************************************************************/
typedef struct {
    LW_BLK_DEV              MN_blkd;                                    /*  SylixOS 第一类块设备        */
    LW_LIST_LINE            MN_lineManage;                              /*  管理链表                    */
    INT                     MN_iFd;                                     /*  第二类块设备文件描述符      */
    mode_t                  MN_mode;                                    /*  第二类设备类型              */
    CHAR                    MN_cVolName[1];                             /*  过载卷的名字                */
} __LW_MOUNT_NODE;
typedef __LW_MOUNT_NODE    *__PLW_MOUNT_NODE;
/*********************************************************************************************************
  挂载点
*********************************************************************************************************/
static LW_LIST_LINE_HEADER  _G_plineMountDevHeader = LW_NULL;           /*  没有必要使用 hash 查询      */
static LW_OBJECT_HANDLE     _G_ulMountLock = 0ul;

#define __LW_MOUNT_LOCK()   API_SemaphoreMPend(_G_ulMountLock, LW_OPTION_WAIT_INFINITE)
#define __LW_MOUNT_UNLOCK() API_SemaphoreMPost(_G_ulMountLock)
/*********************************************************************************************************
** 函数名称: __mountDevReset
** 功能描述: 复位块设备.
** 输　入  : pmnDev             mount 节点
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __mountDevReset (__PLW_MOUNT_NODE  pmnDev)
{
    if (!S_ISBLK(pmnDev->MN_mode)) {
        return  (ERROR_NONE);
    }
    
    return  (ioctl(pmnDev->MN_iFd, LW_BLKD_CTRL_RESET, 0));
}
/*********************************************************************************************************
** 函数名称: __mountDevStatusChk
** 功能描述: 检测块设备.
** 输　入  : pmnDev             mount 节点
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __mountDevStatusChk (__PLW_MOUNT_NODE  pmnDev)
{
    if (!S_ISBLK(pmnDev->MN_mode)) {
        return  (ERROR_NONE);
    }
    
    return  (ioctl(pmnDev->MN_iFd, LW_BLKD_CTRL_STATUS, 0));
}
/*********************************************************************************************************
** 函数名称: __mountDevIoctl
** 功能描述: 控制块设备.
** 输　入  : pmnDev            mount 节点
**           iCmd              控制命令
**           lArg              控制参数
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __mountDevIoctl (__PLW_MOUNT_NODE  pmnDev, INT  iCmd, LONG  lArg)
{
    if (iCmd == LW_BLKD_CTRL_GETFD) {
        if (lArg) {
            *((INT *)lArg) = pmnDev->MN_iFd;
        }
        return  (ERROR_NONE);
    }
    
    if (!S_ISBLK(pmnDev->MN_mode)) {
        if ((iCmd != FIOFLUSH) && 
            (iCmd != FIOSYNC) &&
            (iCmd != FIODATASYNC)) {                                    /*  非 BLK 设备只能执行这些命令 */
            _ErrorHandle(ENOSYS);
            return  (PX_ERROR);
        }
    }
    
    return  (ioctl(pmnDev->MN_iFd, iCmd, lArg));
}
/*********************************************************************************************************
** 函数名称: __mountDevWrt
** 功能描述: 写块设备.
** 输　入  : pmnDev            mount 节点
**           pvBuffer          缓冲区
**           ulStartSector     起始扇区号
**           ulSectorCount     扇区数量
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __mountDevWrt (__PLW_MOUNT_NODE  pmnDev, 
                           VOID             *pvBuffer, 
                           ULONG             ulStartSector, 
                           ULONG             ulSectorCount)
{
    LW_BLK_CTL      blkc;
    
    if (S_ISBLK(pmnDev->MN_mode)) {
        blkc.BLKC_bIsRead       = LW_FALSE;
        blkc.BLKC_pvBuffer      = pvBuffer;
        blkc.BLKC_ulStartSector = ulStartSector;
        blkc.BLKC_ulSectorCount = ulSectorCount;
        return  (ioctl(pmnDev->MN_iFd, LW_BLKD_CTRL_WRITE, &blkc));
    
    } else {
        size_t  stBytes  = (size_t)ulSectorCount * __LW_MOUNT_DEFAULT_SECSIZE;
        off_t   oftStart = (off_t)ulStartSector * __LW_MOUNT_DEFAULT_SECSIZE;
        
        if (pwrite(pmnDev->MN_iFd, pvBuffer, stBytes, oftStart) == stBytes) {
            return  (ERROR_NONE);
        }
    
        _ErrorHandle(ENOTBLK);
        return  (PX_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: __mountDevRd
** 功能描述: 读块设备.
** 输　入  : pmnDev            mount 节点
**           pvBuffer          缓冲区
**           ulStartSector     起始扇区号
**           ulSectorCount     扇区数量
** 输　出  : 0
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __mountDevRd (__PLW_MOUNT_NODE   pmnDev,
                          VOID              *pvBuffer, 
                          ULONG              ulStartSector, 
                          ULONG              ulSectorCount)
{
    LW_BLK_CTL      blkc;
    
    if (S_ISBLK(pmnDev->MN_mode)) {
        blkc.BLKC_bIsRead       = LW_TRUE;
        blkc.BLKC_pvBuffer      = pvBuffer;
        blkc.BLKC_ulStartSector = ulStartSector;
        blkc.BLKC_ulSectorCount = ulSectorCount;
        return  (ioctl(pmnDev->MN_iFd, LW_BLKD_CTRL_READ, &blkc));
        
    } else {
        size_t  stBytes  = (size_t)ulSectorCount * __LW_MOUNT_DEFAULT_SECSIZE;
        off_t   oftStart = (off_t)ulStartSector * __LW_MOUNT_DEFAULT_SECSIZE;
    
        if (pread(pmnDev->MN_iFd, pvBuffer, stBytes, oftStart) == stBytes) {
            return  (ERROR_NONE);
        }
        
        _ErrorHandle(ENOTBLK);
        return  (PX_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: __mountInit
** 功能描述: 初始化 mount 库.
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID __mountInit (VOID)
{
    if (_G_ulMountLock == 0) {
        _IosLock();
        if (_G_ulMountLock == 0) {
            _G_ulMountLock = API_SemaphoreMCreate("mount_lock", LW_PRIO_DEF_CEILING, 
                                LW_OPTION_DELETE_SAFE | LW_OPTION_OBJECT_GLOBAL, LW_NULL);
        }
        _IosUnlock();
    }
}
/*********************************************************************************************************
** 函数名称: __mount
** 功能描述: 挂载一个分区(内部函数)
** 输　入  : pcDevName         块设备名   例如: /dev/sda1
**           pcVolName         挂载目标   例如: /mnt/usb (不能使用相对路径, 否则无法卸载)
**           pcFileSystem      文件系统格式 "vfat" "iso9660" "ntfs" "nfs" "romfs" "ramfs" ... 
                               NULL 表示使用默认文件系统
**           pcOption          选项, 当前支持 ro 或者 rw
** 输　出  : < 0 表示失败
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __mount (CPCHAR  pcDevName, CPCHAR  pcVolName, CPCHAR  pcFileSystem, CPCHAR  pcOption)
{
#define __LW_MOUNT_OPT_RO   "ro"
#define __LW_MOUNT_OPT_RW   "rw"

    REGISTER PCHAR      pcFs;
             FUNCPTR    pfuncFsCreate;
             INT        iFd;
             INT        iOpenFlag = O_RDWR;
             size_t     stLen;
             
      struct stat       statBuf;
    __PLW_MOUNT_NODE    pmnDev;
    
             CHAR           cVolNameBuffer[MAX_FILENAME_LENGTH];

    if (!pcDevName || !pcVolName) {
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }
    
    __mountInit();
    
    if (pcOption) {                                                     /*  文件系统挂载选项            */
        if (lib_strcasecmp(__LW_MOUNT_OPT_RO, pcOption) == 0) {
            iOpenFlag = O_RDONLY;
        } else if (lib_strcasecmp(__LW_MOUNT_OPT_RW, pcOption) == 0) {
            iOpenFlag = O_RDWR;
        }
    }
    
    pcFs = (!pcFileSystem) ? __LW_MOUNT_DEFAULT_FS : (PCHAR)pcFileSystem;
    pfuncFsCreate = __fsCreateFuncGet(pcFs);                            /*  文件系统创建函数            */
    if (pfuncFsCreate == LW_NULL) {
        _ErrorHandle(ERROR_IO_NO_DRIVER);                               /*  没有文件系统驱动            */
        return  (PX_ERROR);
    }
    
    if ((lib_strcmp(pcFileSystem, __LW_MOUNT_NFS_FS) == 0) ||
        (lib_strcmp(pcFileSystem, __LW_MOUNT_RAM_FS) == 0)) {           /*  NFS 或者 RAM FS             */
        iFd = -1;                                                       /*  不需要操作设备文件          */
    
    } else {
        iFd = open(pcDevName, iOpenFlag);                               /*  打开块设备                  */
        if (iFd < 0) {
            iOpenFlag = O_RDONLY;
            iFd = open(pcDevName, iOpenFlag);                           /*  只读方式打开                */
            if (iFd < 0) {
                return  (PX_ERROR);
            }
        }
        if (fstat(iFd, &statBuf) < 0) {                                 /*  获得设备属性                */
            close(iFd);
            return  (PX_ERROR);
        }
    }
    
    _PathGetFull(cVolNameBuffer, MAX_FILENAME_LENGTH, pcVolName);
    
    pcVolName = cVolNameBuffer;                                         /*  使用绝对路径                */
    
    stLen = lib_strlen(pcVolName);
    pmnDev = (__PLW_MOUNT_NODE)__SHEAP_ALLOC(sizeof(__LW_MOUNT_NODE) + stLen);
    if (pmnDev == LW_NULL) {
        if (iFd >= 0) {
            close(iFd);
        }
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (PX_ERROR);
    }
    lib_bzero(pmnDev, sizeof(__LW_MOUNT_NODE));
    
    pmnDev->MN_blkd.BLKD_pcName = (PCHAR)__SHEAP_ALLOC(lib_strlen(pcDevName) + 1);
    if (pmnDev->MN_blkd.BLKD_pcName == LW_NULL) {
        if (iFd >= 0) {
            close(iFd);
        }
        __SHEAP_FREE(pmnDev);
        _DebugHandle(__ERRORMESSAGE_LEVEL, "system low memory.\r\n");
        _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
        return  (PX_ERROR);
    }
    lib_strcpy(pmnDev->MN_blkd.BLKD_pcName, pcDevName);                 /*  记录设备名(nfs ram rom 使用)*/
    lib_strcpy(pmnDev->MN_cVolName, pcVolName);                         /*  保存卷挂载名                */
    
    pmnDev->MN_blkd.BLKD_pfuncBlkRd        = __mountDevRd;
    pmnDev->MN_blkd.BLKD_pfuncBlkWrt       = __mountDevWrt;
    pmnDev->MN_blkd.BLKD_pfuncBlkIoctl     = __mountDevIoctl;
    pmnDev->MN_blkd.BLKD_pfuncBlkReset     = __mountDevReset;
    pmnDev->MN_blkd.BLKD_pfuncBlkStatusChk = __mountDevStatusChk;
    
    
    if (S_ISBLK(statBuf.st_mode)) {                                     /*  标准块设备                  */
        pmnDev->MN_blkd.BLKD_ulNSector        = 0;                      /*  通过 ioctl 获取             */
        pmnDev->MN_blkd.BLKD_ulBytesPerSector = 0;
        pmnDev->MN_blkd.BLKD_ulBytesPerBlock  = 0;
    
    } else {                                                            /*  非块设备使用默认扇区大小    */
        pmnDev->MN_blkd.BLKD_ulBytesPerSector = __LW_MOUNT_DEFAULT_SECSIZE;
        pmnDev->MN_blkd.BLKD_ulNSector        = (ULONG)(statBuf.st_size 
                                              / __LW_MOUNT_DEFAULT_SECSIZE);
        pmnDev->MN_blkd.BLKD_ulBytesPerBlock  = __LW_MOUNT_DEFAULT_SECSIZE;
    }
    
    pmnDev->MN_blkd.BLKD_bRemovable       = LW_TRUE;
    pmnDev->MN_blkd.BLKD_bDiskChange      = LW_FALSE;
    pmnDev->MN_blkd.BLKD_iRetry           = 3;                          /*  default 3 times             */
    pmnDev->MN_blkd.BLKD_iFlag            = iOpenFlag;
    pmnDev->MN_blkd.BLKD_iLogic           = 1;                          /*  格式化不产生分区表          */
    pmnDev->MN_blkd.BLKD_pvLink           = (PLW_BLK_DEV)&pmnDev->MN_blkd;
    /*
     *  以下参数初始化为 0 
     */
    
    pmnDev->MN_iFd  = iFd;
    pmnDev->MN_mode = statBuf.st_mode;                                  /*  设备文件类型                */
    
    if (pfuncFsCreate(pcVolName, (PLW_BLK_DEV)pmnDev) < 0) {            /*  挂载文件系统                */
        if (iFd >= 0) {
            close(iFd);
        }
        __SHEAP_FREE(pmnDev->MN_blkd.BLKD_pcName);
        __SHEAP_FREE(pmnDev);                                           /*  释放控制块                  */
        return  (PX_ERROR);
    }
    
    __LW_MOUNT_LOCK();
    _List_Line_Add_Ahead(&pmnDev->MN_lineManage,
                         &_G_plineMountDevHeader);                      /*  挂入链表                    */
    __LW_MOUNT_UNLOCK();
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __unmount
** 功能描述: 卸载一个分区(内部函数)
** 输　入  : pcVolName         挂载目标   例如: /mnt/usb
** 输　出  : < 0 表示失败
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static INT  __unmount (CPCHAR  pcVolName)
{
    INT                 iError;
    __PLW_MOUNT_NODE    pmnDev;
    PLW_LIST_LINE       plineTemp;
    CHAR                cVolNameBuffer[MAX_FILENAME_LENGTH];
    
    __mountInit();
    
    _PathGetFull(cVolNameBuffer, MAX_FILENAME_LENGTH, pcVolName);
    
    pcVolName = cVolNameBuffer;                                         /*  使用绝对路径                */
    
    __LW_MOUNT_LOCK();
    for (plineTemp  = _G_plineMountDevHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
        
        pmnDev = _LIST_ENTRY(plineTemp, __LW_MOUNT_NODE, MN_lineManage);
        if (lib_strcmp(pmnDev->MN_cVolName, pcVolName) == 0) {
            break;
        }
    }
    if (plineTemp == LW_NULL) {                                         /*  没有找到                    */
        INT iError = PX_ERROR;
        
        if (API_IosDevMatchFull(pcVolName)) {                           /*  如果是设备, 这里就卸载设备  */
            iError = unlink(pcVolName);
            __LW_MOUNT_UNLOCK();
            return  (iError);
        }
        
        __LW_MOUNT_UNLOCK();
        _ErrorHandle(ENOENT);
        return  (PX_ERROR);
    
    } else {
        iError = unlink(pmnDev->MN_cVolName);                           /*  卸载卷                      */
        if (iError < 0) {
            if (errno != ENOENT) {                                      /*  如果不是被卸载过了          */
                __LW_MOUNT_UNLOCK();
                return  (PX_ERROR);                                     /*  卸载失败                    */
            }
        }
        if (pmnDev->MN_iFd >= 0) {
            close(pmnDev->MN_iFd);                                      /*  关闭块设备                  */
        }
        _List_Line_Del(&pmnDev->MN_lineManage,
                       &_G_plineMountDevHeader);                        /*  退出挂载链表                */
    }
    __LW_MOUNT_UNLOCK();
    
    __SHEAP_FREE(pmnDev->MN_blkd.BLKD_pcName);
    __SHEAP_FREE(pmnDev);                                               /*  释放控制块                  */
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: API_MountEx
** 功能描述: 挂载一个分区
** 输　入  : pcDevName         块设备名   例如: /dev/sda1
**           pcVolName         挂载目标   例如: /mnt/usb (不能使用相对路径, 否则无法卸载)
**           pcFileSystem      文件系统格式 "vfat" "iso9660" "ntfs" "nfs" "romfs" "ramfs" ... 
                               NULL 表示使用默认文件系统
**           pcOption          选项, 当前支持 ro 或者 rw
** 输　出  : < 0 表示失败
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_MountEx (CPCHAR  pcDevName, CPCHAR  pcVolName, CPCHAR  pcFileSystem, CPCHAR  pcOption)
{
    INT     iRet;
    
    __KERNEL_SPACE_ENTER();
    iRet = __mount(pcDevName, pcVolName, pcFileSystem, pcOption);
    __KERNEL_SPACE_EXIT();
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: API_Mount
** 功能描述: 挂载一个分区
** 输　入  : pcDevName         块设备名   例如: /dev/sda1
**           pcVolName         挂载目标   例如: /mnt/usb (不能使用相对路径, 否则无法卸载)
**           pcFileSystem      文件系统格式 "vfat" "iso9660" "ntfs" "nfs" "romfs" "ramfs" .. 
                               NULL 表示使用默认文件系统
** 输　出  : < 0 表示失败
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_Mount (CPCHAR  pcDevName, CPCHAR  pcVolName, CPCHAR  pcFileSystem)
{
    INT     iRet;
    
    __KERNEL_SPACE_ENTER();
    iRet = __mount(pcDevName, pcVolName, pcFileSystem, LW_NULL);
    __KERNEL_SPACE_EXIT();
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: API_Unmount
** 功能描述: 卸载一个分区
** 输　入  : pcVolName         挂载目标   例如: /mnt/usb
** 输　出  : < 0 表示失败
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  API_Unmount (CPCHAR  pcVolName)
{
    INT     iRet;
    
    __KERNEL_SPACE_ENTER();
    iRet = __unmount(pcVolName);
    __KERNEL_SPACE_EXIT();
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: API_MountShow
** 功能描述: 显示当前挂载的信息
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
VOID  API_MountShow (VOID)
{
    PCHAR               pcMountInfoHdr = "       VOLUME                    BLK NAME\n"
                                         "-------------------- --------------------------------\n";
    __PLW_MOUNT_NODE    pmnDev;
    PLW_LIST_LINE       plineTemp;
    
    CHAR                cBlkNameBuffer[MAX_FILENAME_LENGTH];
    PCHAR               pcBlkName;

    if (LW_CPU_GET_CUR_NESTING()) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "called from ISR.\r\n");
        _ErrorHandle(ERROR_KERNEL_IN_ISR);
        return;
    }
    
    __mountInit();
    
    printf("all mount point show >>\n");
    printf(pcMountInfoHdr);                                             /*  打印欢迎信息                */
    
    __LW_MOUNT_LOCK();
    for (plineTemp  = _G_plineMountDevHeader;
         plineTemp != LW_NULL;
         plineTemp  = _list_line_get_next(plineTemp)) {
        
        pmnDev = _LIST_ENTRY(plineTemp, __LW_MOUNT_NODE, MN_lineManage);
        
        if (pmnDev->MN_blkd.BLKD_pcName) {
            pcBlkName = pmnDev->MN_blkd.BLKD_pcName;
        } else {
            INT     iRet;
            __KERNEL_SPACE_ENTER();                                     /*  此文件描述符属于内核        */
            iRet = API_IosFdGetName(pmnDev->MN_iFd, cBlkNameBuffer, MAX_FILENAME_LENGTH);
            __KERNEL_SPACE_EXIT();
            if (iRet < ERROR_NONE) {
                pcBlkName = "<unknown>";
            } else {
                pcBlkName = cBlkNameBuffer;
            }
        }
        printf("%-20s %-32s\n", pmnDev->MN_cVolName, pcBlkName);
    }
    __LW_MOUNT_UNLOCK();
}
/*********************************************************************************************************
** 函数名称: mount
** 功能描述: linux 兼容 mount.
** 输　入  : pcDevName     设备名
**           pcVolName     挂载目标
**           pcFileSystem  文件系统
**           ulFlag        挂载参数
**           pvData        附加信息(未使用)
** 输　出  : 挂载结果
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  mount (CPCHAR  pcDevName, CPCHAR  pcVolName, CPCHAR  pcFileSystem, 
            ULONG   ulFlag, CPVOID pvData)
{
    PCHAR   pcOption = "rw";

    if (ulFlag & MS_RDONLY) {
        pcOption = "ro";
    }
    
    (VOID)pvData;
    
    return  (API_MountEx(pcDevName, pcVolName, pcFileSystem, pcOption));
}
/*********************************************************************************************************
** 函数名称: umount
** 功能描述: linux 兼容 umount.
** 输　入  : pcVolName     挂载节点
** 输　出  : 解除挂载结果
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  umount (CPCHAR  pcVolName)
{
    return  (API_Unmount(pcVolName));
}
/*********************************************************************************************************
** 函数名称: umount2
** 功能描述: linux 兼容 umount2.
** 输　入  : pcVolName     挂载节点
**           iFlag         MNT_FORCE 表示立即解除
** 输　出  : 解除挂载结果
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API 
INT  umount2 (CPCHAR  pcVolName, INT iFlag)
{
    INT iFd;

    if (iFlag & MNT_FORCE) {
        iFd = open(pcVolName, O_RDONLY);
        if (iFd >= 0) {
            ioctl(iFd, FIOSETFORCEDEL, LW_TRUE);                        /*  允许立即卸载设备            */
            close(iFd);
        }
    }
    
    return  (API_Unmount(pcVolName));
}
#endif                                                                  /*  LW_CFG_MAX_VOLUMES > 0      */
                                                                        /*  LW_CFG_MOUNT_EN > 0         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
