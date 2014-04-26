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
** 文   件   名: blockIo.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 09 月 26 日
**
** 描        述: 系统块设备定义.

** BUG:
2009.07.06  加入电源打开和磁盘初始化计数器.
2009.12.01  加入 unix 系统类似 mount 块设备方式.
2012.06.27  加入 LW_BLKD_CTRL_GETFD 命令.
*********************************************************************************************************/

#ifndef __BLOCKIO_H
#define __BLOCKIO_H

/*********************************************************************************************************
  注意: SylixOS 的文件系统有两种装载方式, 用户可以根据自身系统的特点灵活选择.
  
  1: LW_BLK_DEV 模式. 这也是操作系统的"默认"挂载模式. 如图所示:
     
                          +------------------+
                          |       USER       |
                          +------------------+
                                   |
                          +------------------+
                          |       I/O        |  (用户仅可见文件系统)
                          +------------------+
                                   |
                          +------------------+
                          |        FS        |
                          +------------------+
                                   |
                          +------------------+
                          |      BLK_DEV     |  (对用户来说不可见)
                          +------------------+
                          
    LW_BLK_DEV 整体与 I/O 系统无关, 仅是一个文件系统设备操作的实体. 对于用户应用程序不可见, 类似于很多
    嵌入式第三方文件系统软件提供的方式. 此方式更加适合于嵌入式系统推荐使用此方式.
    
    此模式的使用请参考 SylixOS 提供的 ramDisk 方式.
    
  2: LW_BLK_CTL 模式. 这是 SylixOS 可选择的文件系统挂载方式. 如图所示:
    
                          +------------------+
                          |       USER       |
                          +------------------+        /------------------\
                                   |                  |                  |
                          +------------------+        |         +------------------+
                          |       I/O        |        |         |       I/O        |
                          +------------------+        |         +------------------+
                                   |                  |                  |
                          +------------------+        |         +------------------+
                          |        FS        |        |         |      BLK_CTL     |
                          +------------------+        |         +------------------+
                                   |                  |
                                   \-------------------
                                   
    LW_BLK_CTL 是存在于 I/O 系统中的一个设备, 用户可以通过 I/O 系统直接访问此设备, 对于用户来说是可见的
    设备. 使用 mount 将此设备挂接文件系统后, 文件系统将通过 ioctl 操作此设备. 此方法类似于 Linux 等大型
    操作系统提供的方法. 例如插入一个优盘, I/O 系统中会出现一个 /udev0 的设备. 然后通过 mount 指令将其挂载
    入文件系统操作. mount -t vfat /udev0 /udisk 此时, (SylixOS 系统会自动识别分区信息)
    
    用户操作 /udev0   等于绕过文件系统直接操作物理设备.
    用户操作 /udisk0  表示使用文件系统操作物理设备.
    
    此操作类型需要 LW_CFG_MOUNT_EN 与 LW_CFG_SHELL_EN 支持.
    
    (推荐使用第一种方法简单可靠, 直接使用 oemDiskMount/oemDiskMountEx 即可)
*********************************************************************************************************/

/*********************************************************************************************************
                                          LW_BLK_DEV 模式
  注意:
  BLKD_iRetry               至少为 1;
  BLKD_ulNSector            为 0 时, 系统将通过 BLKD_pfuncBlkIoctl 函数获取
  BLKD_ulBytesPerBlock      为 0 时, 系统将通过 BLKD_pfuncBlkIoctl 函数获取
  BLKD_ulBytesPerSector     最小为 512 必须为 2 的 n 次方. (FAT 扇区最大支持 4096 字节)

  BLKD_pfuncBlkReset        fatFsDevCreate() 将会挂载设备, 同时为设备通电, 然后立即调用此函数复位设备.
                            当一个块设备存在多个分区时, 每次挂载不同分区时都会调用此函数.
                            
  BLKD_bRemovable           是否是可移动设备.
  BLKD_bDiskChange          磁盘介质是否发生改变. (初始化时必须为 FALSE, 当磁盘发生改变时, 将无法再次操作
                                                   必须重新建立卷)
  BLKD_iFlag                O_RDONLY 表示磁盘写保护.
  
  BLKD_iLogic               是否为逻辑磁盘, 用户驱动程序只要将其设置为 0 即可.
  BLKD_iLinkCounter         物理设备驱动相关字段, 初始化时必须为 0
  BLKD_pvLink               物理设备驱动相关字段, 初始化时必须为 NULL
  
  BLKD_uiPowerCounter       电源控制计数器, 初始化时必须为 0
  BLKD_uiInitCounter        磁盘初始化计数器, 初始化时必须为 0
  
  当一个物理设备存在多个分区时, 操作系统并没有互斥物理设备的访问, 所以在驱动程序中, 需要注意互斥访问问题.
  在单分区设备中将不会有任何问题. 因为文件系统的基本互斥操作单位是卷.
*********************************************************************************************************/

typedef struct {
    PCHAR       BLKD_pcName;                                            /* 可以为 NULL 或者 "\0"        */
                                                                        /* nfs romfs 文件系统使用       */
    FUNCPTR     BLKD_pfuncBlkRd;		                                /* function to read blocks      */
    FUNCPTR     BLKD_pfuncBlkWrt;		                                /* function to write blocks     */
    FUNCPTR     BLKD_pfuncBlkIoctl;		                                /* function to ioctl device     */
    FUNCPTR     BLKD_pfuncBlkReset;		                                /* function to reset device     */
    FUNCPTR     BLKD_pfuncBlkStatusChk;                                 /* function to check status     */
    
    ULONG       BLKD_ulNSector;                                         /* number of sectors            */
    ULONG       BLKD_ulBytesPerSector;                                  /* bytes per sector             */
    ULONG       BLKD_ulBytesPerBlock;                                   /* bytes per block              */
    
    BOOL        BLKD_bRemovable;                                        /* removable medium flag        */
    BOOL        BLKD_bDiskChange;                                       /* media change flag            */
    INT         BLKD_iRetry;                                            /* retry count for I/O errors   */
    INT         BLKD_iFlag;                                             /* O_RDONLY or O_RDWR           */
    
    /*
     *  以下参数操作系统使用, 必须初始化为 0.
     */
    INT         BLKD_iLogic;                                            /* if this is a logic disk      */
    UINT        BLKD_uiLinkCounter;                                     /* must be 0                    */
    PVOID       BLKD_pvLink;                                            /* must be NULL                 */
    
    UINT        BLKD_uiPowerCounter;                                    /* must be 0                    */
    UINT        BLKD_uiInitCounter;                                     /* must be 0                    */
} LW_BLK_DEV;
typedef LW_BLK_DEV          BLK_DEV;
typedef LW_BLK_DEV         *PLW_BLK_DEV;
typedef LW_BLK_DEV         *BLK_DEV_ID;

/*********************************************************************************************************
  磁盘设备必须要支持的通用 FIO 命令如下:

  FIOSYNC       与 FIOFLUSH 相同.
  FIOFLUSH      回写磁盘数据到物理磁盘.
  FIOUNMOUNT    卸载磁盘     (系统在卸载此磁盘时将会调用此函数)
  FIODISKINIT   初始化磁盘   (系统将会多次调用此函数, 例如: 挂载卷, 分析分区表, 格式化...)
  
  可移动磁盘介质还需支持的命令:
  
  FIODATASYNC   数据回写, 可以与 FIOSYNC 做相同处理.
  FIOCANCEL     放弃还没写入磁盘的数据(磁盘介质发生变化!)
  FIODISKCHANGE 磁盘介质发生变化, 放弃还没写入磁盘的数据. 然后必须将 BLKD_bDiskChange 设置为 LW_TRUE 使
                操作系统立即停止对相应卷的操作, 等待重新挂载.
*********************************************************************************************************/
/*********************************************************************************************************
  IOCTL 通用指令 (磁盘设备扩展)
*********************************************************************************************************/

#define LW_BLKD_CTRL_POWER          201                                 /*  控制设备电源                */
#define LW_BLKD_CTRL_LOCK           202                                 /*  锁定设备(保留)              */
#define LW_BLKD_CTRL_EJECT          203                                 /*  弹出设备(保留)              */

/*********************************************************************************************************
  当 LW_BLK_DEV 相关字段为 0 时, 文件系统需要调用以下 ioctl 命令获取信息
*********************************************************************************************************/

#define LW_BLKD_GET_SECNUM          204                                 /*  获得设备扇区数量            */
#define LW_BLKD_GET_SECSIZE         205                                 /*  获得扇区的大小, 单位:字节   */
#define LW_BLKD_GET_BLKSIZE         206                                 /*  获得块大小 单位:字节        */
                                                                        /*  可以与扇区大小相同          */

/*********************************************************************************************************
  电源控制指令参数
*********************************************************************************************************/

#define LW_BLKD_POWER_OFF           0                                   /*  关闭磁盘电源                */
#define LW_BLKD_POWER_ON            1                                   /*  打开磁盘电源                */

/*********************************************************************************************************
                                          LW_BLK_CTL 模式
  注意:
  链接的目标设备必须支持 stat 操作, 而且 st_mode 必须为 S_IFBLK 设备.
  
  LW_BLK_CTL 结构体是文件系统操作磁盘设备 ioctl 时的参数.
*********************************************************************************************************/

typedef struct {
    BOOL                    BLKC_bIsRead;                               /*  读操作, 否则为写            */
    PVOID                   BLKC_pvBuffer;                              /*  缓冲区                      */
    ULONG                   BLKC_ulStartSector;                         /*  起始扇区                    */
    ULONG                   BLKC_ulSectorCount;                         /*  操作扇区数量                */
} LW_BLK_CTL;
typedef LW_BLK_CTL         *PLW_BLK_CTL;

/*********************************************************************************************************
  第二种模式磁盘设备 (I/O) 除了需要支持以上所有的 ioctl 命令还需要支持:
  
  LW_BLKD_CTRL_WRITE        写磁盘          (参数为: PLW_BLK_CTL)
  LW_BLKD_CTRL_READ         读磁盘          (参数为: PLW_BLK_CTL)
  LW_BLKD_CTRL_RESET        复位磁盘        (无参数)
  LW_BLKD_CTRL_STATUS       检查磁盘状态    (无参数)
  
  LW_BLKD_CTRL_GETFD        不需要支持, mount 会提供相关操作.
*********************************************************************************************************/

#define LW_BLKD_CTRL_WRITE          207                                 /*  写磁盘                      */
#define LW_BLKD_CTRL_READ           208                                 /*  读磁盘                      */
#define LW_BLKD_CTRL_RESET          209                                 /*  复位磁盘                    */
#define LW_BLKD_CTRL_STATUS         210                                 /*  检查磁盘状态                */
#define LW_BLKD_CTRL_GETFD          211                                 /*  获取块设备操作文件描述符    */

#endif                                                                  /*  __BLOCKIO_H                 */
/*********************************************************************************************************
  END
*********************************************************************************************************/
