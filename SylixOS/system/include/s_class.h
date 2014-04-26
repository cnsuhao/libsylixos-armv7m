/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                       SylixOS(TM)
**
**                               Copyright  All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: s_class.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 02 月 13 日
**
** 描        述: 这是系统系统级控件类型定义文件。

** BUG
2007.04.08  删除裁剪宏控制，类型的定义，和裁剪无关
2008.03.23  今天是 2008 台湾地区领导人选举后的第一天, 真希望两岸和平统一.
            改进 I/O 系统, 加入 BSD 和 Linux 系统的相关功能.
2009.02.13  今天开始大刀阔斧改进 I/O 系统, 修改为类似 UNIX/BSD I/O 管理模式, 但是使用特殊算法, 是常用 API
            保持时间复杂度 O(1) 时间复杂度.
2009.08.31  加入对应于设备的功耗管理模块.
2009.10.02  设备头打开次数使用 atomic_t 型.
2009.11.21  加入 io 环境概念.
2009.12.14  在驱动程序接口中加入 symlink 功能. 注意: 符号链接只有部分文件系统支持(部分功能)! 
2010.09.10  file_operations 加入 fstat 操作方法.
2011.07.28  file_operations 加入 mmap 操作方法.
2011.08.07  file_operations 加入 lstat 操作方法.
2012.10.19  LW_FD_NODE 加入引用计数 FDNODE_ulRef, 例如 mmap() 时产生计数. munmap() 时减少计数, 当计数为 0
            时关闭文件.
2012.11.21  文件结构中加入了 unlink 标志, 开始支持文件没有关闭时的 unlink 操作.
2012.12.21  大幅度的更新文件描述符表
            加入一种新的机制: FDENTRY_bRemoveReq 表示如果此时文件结构链表忙, 则请求推迟删除其中一个节点,
            这个新的机制事遍历文件解构表时, 不再锁定 IO 系统.
2013.01.09  LW_IO_ENV 中加入 umask 参数.
2013.05.27  fd entry 加入去除符号连接以外的真实文件名.
*********************************************************************************************************/

#ifndef __S_CLASS_H
#define __S_CLASS_H

/*********************************************************************************************************
  IO 环境
*********************************************************************************************************/

typedef struct {
    mode_t               IOE_modeUMask;                                 /*  umask                       */
    CHAR                 IOE_cDefPath[MAX_FILENAME_LENGTH];             /*  默认当前路径                */
} LW_IO_ENV;
typedef LW_IO_ENV       *PLW_IO_ENV;

/*********************************************************************************************************
  IO 用户信息
*********************************************************************************************************/

typedef struct {
    uid_t                IOU_uid;
    gid_t                IOU_gid;
} LW_IO_USR;
typedef LW_IO_USR       *PLW_IO_USR;

/*********************************************************************************************************
  功耗管理节点 (每个设备可以建立多个功耗控制点, 每个功耗控制点可以有不同的功耗控制等级, 例如显示器背光
                可以在根据空闲时间选择不同的亮度. 绝大多数情况下一个设备仅一个电源管理节点即可, 仅仅需要
                控制睡眠与唤醒即可.)
  注意         每次操作设备时(读, 写, 控制等等), 都需要调用 iosDevPowerMSignal(), 以便通知操作系统相关设备
               处于活动状态, 操作系统会复位相关功耗定时器.
               当长时间不使用此设备时(功耗定时器移溢出), 系统将会调用相关回调来使此设备进入低功耗状态.
*********************************************************************************************************/

#if (LW_CFG_POWERM_EN > 0) && (LW_CFG_MAX_POWERM_NODES > 0)

typedef struct {
    LW_LIST_LINE              DEVPM_lineManage;                         /*  电源管理节点链表            */
    LW_OBJECT_HANDLE          DEVPM_ulPowerM;                           /*  电源管理节点对象            */
    
    FUNCPTR                   DEVPM_pfuncPowerOff;                      /*  将设备至于低功耗模式        */
    PVOID                     DEVPM_pvArgPowerOff;                      /*  低功耗模式参数              */
    FUNCPTR                   DEVPM_pfuncPowerSignal;                   /*  激活设备                    */
                                                                        /*  会被 iosDevPowerMSignal 调用*/
    PVOID                     DEVPM_pvArgPowerSignal;                   /*  激活设备参数                */
    FUNCPTR                   DEVPM_pfuncRemove;                        /*  移除设备时系统会调用此函数  */
                                                                        /*  用来清除用户需要清除的结构  */
    PVOID                     DEVPM_pvArgRemove;                        /*  移除设备参数                */
} LW_DEV_PM;
typedef LW_DEV_PM            *PLW_DEV_PM;
#endif                                                                  /*  LW_CFG_POWERM_EN            */
                                                                        /*  LW_CFG_MAX_POWERM_NODES     */
/*********************************************************************************************************
  设备头
  
  这里使用的是普通的双向链表, 考虑到只有打开设备可能需要检索设备链, 其余操作几乎都是 O(1) 时间复杂度.
  所以这里避免使用了 hash 哈希散列表做检索.
  如果设备很多, 又对嵌入式系统的启动有特殊要求, 可以使用以霍纳(horner)多项式作为哈希函数的哈希检索表.
*********************************************************************************************************/

typedef struct {
    LW_LIST_LINE               DEVHDR_lineManage;                       /*  设备头管理链表              */
    UINT16                     DEVHDR_usDrvNum;                         /*  设备驱动程序索引号          */
    PCHAR                      DEVHDR_pcName;                           /*  设备名称                    */
    UCHAR                      DEVHDR_ucType;                           /*  设备 dirent d_type          */
    atomic_t                   DEVHDR_atomicOpenNum;                    /*  打开的次数                  */
    LW_LIST_LINE_HEADER        DEVHDR_plinePowerMHeader;                /*  设备功耗管理节点链          */
} LW_DEV_HDR;
typedef LW_DEV_HDR            *PLW_DEV_HDR;

static LW_INLINE INT  LW_DEV_INC_USE_COUNT(PLW_DEV_HDR  pdevhdrHdr)
{
    return  ((pdevhdrHdr) ? (API_AtomicInc(&pdevhdrHdr->DEVHDR_atomicOpenNum)) : (PX_ERROR));
}

static LW_INLINE INT  LW_DEV_DEC_USE_COUNT(PLW_DEV_HDR  pdevhdrHdr)
{
    return  ((pdevhdrHdr) ? (API_AtomicDec(&pdevhdrHdr->DEVHDR_atomicOpenNum)) : (PX_ERROR));
}

static LW_INLINE INT  LW_DEV_GET_USE_COUNT(PLW_DEV_HDR  pdevhdrHdr)
{
    return  ((pdevhdrHdr) ? (API_AtomicGet(&pdevhdrHdr->DEVHDR_atomicOpenNum)) : (PX_ERROR));
}

/*********************************************************************************************************
  驱动程序许可证
*********************************************************************************************************/

typedef struct {
    PCHAR                      DRVLIC_pcLicense;                        /*  许可证                      */
    PCHAR                      DRVLIC_pcAuthor;                         /*  作者                        */
    PCHAR                      DRVLIC_pcDescription;                    /*  描述                        */
} LW_DRV_LICENSE;

/*********************************************************************************************************
  驱动程序类型
*********************************************************************************************************/

#define LW_DRV_TYPE_ORIG      0                                         /*  原始设备驱动, 兼容 VxWorks  */
#define LW_DRV_TYPE_SOCKET    1                                         /*  SOCKET 型设备驱动程序       */
#define LW_DRV_TYPE_NEW_1     2                                         /*  NEW_1 型设备驱动程序        */

/*********************************************************************************************************
  驱动程序控制块 (主设备) (只有使能 SylixOS 新的目录管理功能才能使用符号链接)
*********************************************************************************************************/

typedef struct {
    LONGFUNCPTR                DEVENTRY_pfuncDevCreate;                 /*  建立函数                    */
    FUNCPTR                    DEVENTRY_pfuncDevDelete;                 /*  删除函数                    */
    
    LONGFUNCPTR                DEVENTRY_pfuncDevOpen;                   /*  打开函数                    */
    FUNCPTR                    DEVENTRY_pfuncDevClose;                  /*  关闭函数                    */
    
    SSIZETFUNCPTR              DEVENTRY_pfuncDevRead;                   /*  读设备函数                  */
    SSIZETFUNCPTR              DEVENTRY_pfuncDevWrite;                  /*  写设备函数                  */
    
    SSIZETFUNCPTR              DEVENTRY_pfuncDevReadEx;                 /*  读设备函数扩展函数          */
    SSIZETFUNCPTR              DEVENTRY_pfuncDevWriteEx;                /*  读设备函数扩展函数          */
    
    FUNCPTR                    DEVENTRY_pfuncDevIoctl;                  /*  设备控制函数                */
    FUNCPTR                    DEVENTRY_pfuncDevSelect;                 /*  select 功能                 */
    OFFTFUNCPTR                DEVENTRY_pfuncDevLseek;                  /*  lseek 功能                  */
    
    FUNCPTR                    DEVENTRY_pfuncDevFstat;                  /*  fstat 功能                  */
    FUNCPTR                    DEVENTRY_pfuncDevLstat;                  /*  lstat 功能                  */
    
    FUNCPTR                    DEVENTRY_pfuncDevSymlink;                /*  建立链接文件                */
    SSIZETFUNCPTR              DEVENTRY_pfuncDevReadlink;               /*  读取链接文件                */
    
    FUNCPTR                    DEVENTRY_pfuncDevMmap;                   /*  文件映射                    */
    FUNCPTR                    DEVENTRY_pfuncDevUnmap;                  /*  映射结束                    */
    
    BOOL                       DEVENTRY_bInUse;                         /*  是否被使用                  */
    INT                        DEVENTRY_iType;                          /*  设备驱动类型                */
    
    LW_DRV_LICENSE             DEVENTRY_drvlicLicense;                  /*  驱动程序许可证              */
} LW_DEV_ENTRY;
typedef LW_DEV_ENTRY          *PLW_DEV_ENTRY;

/*********************************************************************************************************
  设备文件操作控制块
*********************************************************************************************************/

typedef struct file_operations {
    struct module              *owner;
    
    long                      (*fo_create)();                           /*  DEVENTRY_pfuncDevCreate     */
    int                       (*fo_release)();                          /*  DEVENTRY_pfuncDevDelete     */
    
    long                      (*fo_open)();                             /*  DEVENTRY_pfuncDevOpen       */
    int                       (*fo_close)();                            /*  DEVENTRY_pfuncDevClose      */
    
    ssize_t                   (*fo_read)();                             /*  DEVENTRY_pfuncDevRead       */
    ssize_t                   (*fo_read_ex)();                          /*  DEVENTRY_pfuncDevReadEx     */
    
    ssize_t                   (*fo_write)();                            /*  DEVENTRY_pfuncDevWrite      */
    ssize_t                   (*fo_write_ex)();                         /*  DEVENTRY_pfuncDevWriteEx    */
    
    int                       (*fo_ioctl)();                            /*  DEVENTRY_pfuncDevIoctl      */
    int                       (*fo_select)();                           /*  DEVENTRY_pfuncDevSelect     */
    
    int                       (*fo_lock)();                             /*  not support now             */
    off_t                     (*fo_lseek)();                            /*  DEVENTRY_pfuncDevLseek      */
    
    int                       (*fo_fstat)();                            /*  DEVENTRY_pfuncDevFstat      */
    int                       (*fo_lstat)();                            /*  DEVENTRY_pfuncDevLstat      */
    
    int                       (*fo_symlink)();                          /*  DEVENTRY_pfuncDevSymlink    */
    ssize_t                   (*fo_readlink)();                         /*  DEVENTRY_pfuncDevReadlink   */
    
    int                       (*fo_mmap)();                             /*  DEVENTRY_pfuncDevMmap       */
    int                       (*fo_unmap)();                            /*  DEVENTRY_pfuncDevUnmmap     */
    
    ULONG                       fo_pad[16];                             /*  reserve                     */
} FILE_OPERATIONS;
typedef FILE_OPERATIONS        *PFILE_OPERATIONS;

/*********************************************************************************************************
  fo_mmap 和 fo_unmap 地址信息参数
*********************************************************************************************************/

typedef struct {
    PVOID                       DMAP_pvAddr;                            /*  起始地址                    */
    size_t                      DMAP_stLen;                             /*  内存长度 (单位:字节)        */
    off_t                       DMAP_offPages;                          /*  文件映射偏移量 (单位:页面)  */
    ULONG                       DMAP_ulFlag;                            /*  当前虚拟地址 VMM 属性       */
} LW_DEV_MMAP_AREA;
typedef LW_DEV_MMAP_AREA       *PLW_DEV_MMAP_AREA;

/*********************************************************************************************************
  SylixOS I/O 系统结构
  
  (ORIG 型驱动)
  
       0                       1                       N
  +---------+             +---------+             +---------+
  | FD_DESC |             | FD_DESC |     ...     | FD_DESC |
  +---------+             +---------+             +---------+
       |                       |                       |
       |                       |                       |
       \-----------------------/                       |
                   |                                   |
                   |                                   |
             +------------+                      +------------+
  HEADER ->  |  FD_ENTERY |   ->    ...   ->     |  FD_ENTERY |  ->  NULL
             +------------+                      +------------+
                   |                                   |
                   |                                   |
                   |                                   |
             +------------+                      +------------+
             |  DEV_HDR   |                      |  DEV_HDR   |
             +------------+                      +------------+
                   |                                   |
                   |                                   |
                   \-----------------------------------/
                                    |
                                    |
                              +------------+
                              |   DRIVER   |
                              +------------+
  
  (NEW_1 型驱动)
  
       0                       1                       N
  +---------+             +---------+             +---------+
  | FD_DESC |             | FD_DESC |     ...     | FD_DESC |
  +---------+             +---------+             +---------+
       |                       |                       |
       |                       |                       |
       \-----------------------/                       |
                   |                                   |
                   |                                   |
             +------------+                      +------------+
  HEADER ->  |  FD_ENTERY |   ->    ...   ->     |  FD_ENTERY |  ->  NULL
             +------------+                      +------------+
                   |                 |                 |
                   |                 |                 |
                   \-----------------/                 |
                             |                         |
                             |                         |
                      +------------+             +------------+
                      |   FD_NODE  |    ...      |   FD_NODE  |
                      | lockf->... |             | lockf->... |
                      +------------+             +------------+
                             |           |             |
                             |           |             |
                             \-----------/             |
                                   |                   |
                                   |                   |
                            +------------+       +------------+
                            |  DEV_HDR   |       |  DEV_HDR   |
                            +------------+       +------------+
                                   |                   |
                                   |                   |
                                   \-------------------/
                                             |
                                             |
                                       +------------+
                                       |   DRIVER   |
                                       +------------+
*********************************************************************************************************/
/*********************************************************************************************************
  文件记录锁
  只有 NEW_1 或更高级的设备驱动类型可以支持文件记录锁.
*********************************************************************************************************/

typedef struct __fd_lockf {
    LW_OBJECT_HANDLE           FDLOCK_ulBlock;                          /*  阻塞锁                      */
    INT16                      FDLOCK_usFlags;                          /*  F_WAIT F_FLOCK F_POSIX      */
    INT16                      FDLOCK_usType;                           /*  F_RDLCK F_WRLCK             */
    off_t                      FDLOCK_oftStart;                         /*  start of the lock           */
    off_t                      FDLOCK_oftEnd;                           /*  end of the lock (-1=EOF)    */
    pid_t                      FDLOCK_pid;                              /*  resource holding process    */
    
    struct  __fd_lockf       **FDLOCK_ppfdlockHead;                     /*  back to the head of list    */
    struct  __fd_lockf        *FDLOCK_pfdlockNext;                      /*  Next lock on this fd_node,  */
                                                                        /*  or blocking lock            */
    
    LW_LIST_LINE_HEADER        FDLOCK_plineBlockHd;                     /*  阻塞在当前锁的记录锁队列    */
    LW_LIST_LINE               FDLOCK_lineBlock;                        /*  请求等待链表 header is lockf*/
    LW_LIST_LINE               FDLOCK_lineBlockQ;                       /*  阻塞链表, header is fd_node */
} LW_FD_LOCKF;
typedef LW_FD_LOCKF           *PLW_FD_LOCKF;

#ifdef __SYLIXOS_KERNEL
#define F_WAIT                 0x10                                     /* wait until lock is granted   */
#define F_FLOCK                0x20                                     /* BSD semantics for lock       */
#define F_POSIX                0x40                                     /* POSIX semantics for lock     */
#define F_ABORT                0x80                                     /* lock wait abort!             */
#endif                                                                  /* __SYLIXOS_KERNEL             */

/*********************************************************************************************************
  文件节点 
  
  只有 NEW_1 或更高级的设备驱动类型会用到此结构
  一个 dev_t 和 一个 ino_t 对应唯一一个实体文件, 操作系统统多次打开同一个实体文件时, 只有一个文件节点
  多个 fd_entry 同时指向这个节点.
  需要说明的是 FDNODE_oftSize 字段需要 NEW_1 驱动程序自己来维护.
  
  fd node 被锁定时, 将不允许写, 也不允许删除. 当关闭文件后此文件的 lock 将自动被释放.
*********************************************************************************************************/

typedef struct {
    LW_LIST_LINE               FDNODE_lineManage;                       /*  同一设备 fd_node 链表       */
    
    LW_OBJECT_HANDLE           FDNODE_ulSem;                            /*  内部操作锁                  */
    dev_t                      FDNODE_dev;                              /*  设备                        */
    ino64_t                    FDNODE_inode64;                          /*  inode (64bit 为了兼容性)    */
    mode_t                     FDNODE_mode;                             /*  文件 mode                   */
    uid_t                      FDNODE_uid;                              /*  文件所属用户信息            */
    gid_t                      FDNODE_gid;
    
    off_t                      FDNODE_oftSize;                          /*  当前文件大小                */
    
    struct  __fd_lockf        *FDNODE_pfdlockHead;                      /*  第一个锁                    */
    LW_LIST_LINE_HEADER        FDNODE_plineBlockQ;                      /*  当前有阻塞的记录锁队列      */
    
    ULONG                      FDNODE_ulLock;                           /*  锁定, 不允许写, 不允许删除  */
    ULONG                      FDNODE_ulRef;                            /*  fd_entry 引用此 fd_node 数量*/
    PVOID                      FDNODE_pvFile;                           /*  驱动可使用, 当前未使用      */
} LW_FD_NODE;
typedef LW_FD_NODE            *PLW_FD_NODE;

/*********************************************************************************************************
  文件结构 (文件表)
  FDENTRY_ulCounter 计数器是所有对应的 LW_FD_DESC 计数器总和. 
  如果没有 dup 过, 则 FDENTRY_ulCounter 应该与 FDDESC_ulRef 相同.
  
  如果是 ORIG 类型驱动:
  则 open 后传入驱动的首参数为 FDENTRY_lValue.
  
  如果是 NEW_1 类型驱动:
  则 open 后传入驱动的首参数为 fd_entry 本身, FDENTRY_lValue 为 open 返回值其首参数必须为 LW_FD_NODE.
  需要说明的是 FDENTRY_oftPtr 字段需要驱动程序自己来维护.
*********************************************************************************************************/

typedef struct {
    PLW_DEV_HDR                FDENTRY_pdevhdrHdr;                      /*  设备头                      */
    PCHAR                      FDENTRY_pcName;                          /*  文件名                      */
    PCHAR                      FDENTRY_pcRealName;                      /*  去除符号链接的真实文件名    */
    LW_LIST_LINE               FDENTRY_lineManage;                      /*  文件控制信息遍历表          */
    
#define FDENTRY_pfdnode        FDENTRY_lValue
    LONG                       FDENTRY_lValue;                          /*  驱动程序内部数据            */
                                                                        /*  如果为 NEW_1 驱动则为fd_node*/
    INT                        FDENTRY_iType;                           /*  文件类型 (根据驱动判断)     */
    INT                        FDENTRY_iFlag;                           /*  文件属性                    */
    INT                        FDENTRY_iAbnormity;                      /*  文件异常                    */
    
    BOOL                       FDENTRY_bNeedUnlink;                     /*  是否在文件未关闭时有 unlink */
    ULONG                      FDENTRY_ulCounter;                       /*  总引用计数器                */
    off_t                      FDENTRY_oftPtr;                          /*  文件当前指针                */
                                                                        /*  只有 NEW_1 或更高级驱动使用 */
    BOOL                       FDENTRY_bRemoveReq;                      /*  删除请求                    */
} LW_FD_ENTRY;
typedef LW_FD_ENTRY           *PLW_FD_ENTRY;

/*********************************************************************************************************
  文件描述符表
*********************************************************************************************************/

typedef struct {
    PLW_FD_ENTRY               FDDESC_pfdentry;                         /*  文件结构                    */
    BOOL                       FDDESC_bCloExec;                         /*  FD_CLOEXEC                  */
    ULONG                      FDDESC_ulRef;                            /*  对应文件描述符的引用计数    */
} LW_FD_DESC;
typedef LW_FD_DESC            *PLW_FD_DESC;

/*********************************************************************************************************
    线程池
*********************************************************************************************************/

typedef struct {
    LW_LIST_MONO               TPCB_monoResrcList;                       /*  资源链表                   */
    PTHREAD_START_ROUTINE      TPCB_pthreadStartAddress;                 /*  线程代码入口点             */
    PLW_LIST_RING              TPCB_pringFirstThread;                    /*  第一个线程                 */
    LW_CLASS_THREADATTR        TPCB_threakattrAttr;                      /*  线程建立属性块             */
    
    UINT16                     TPCB_usMaxThreadCounter;                  /*  最多线程数量               */
    UINT16                     TPCB_usThreadCounter;                     /*  当前线程数量               */
    
    LW_OBJECT_HANDLE           TPCB_hMutexLock;                          /*  操作锁                     */
    
    UINT16                     TPCB_usIndex;                             /*  索引下标                   */
                                                                         /*  名字                       */
    CHAR                       TPCB_cThreadPoolName[LW_CFG_OBJECT_NAME_SIZE];
} LW_CLASS_THREADPOOL;
typedef LW_CLASS_THREADPOOL   *PLW_CLASS_THREADPOOL;

#endif                                                                   /*  __S_CLASS_H                */
/*********************************************************************************************************
  END
*********************************************************************************************************/
