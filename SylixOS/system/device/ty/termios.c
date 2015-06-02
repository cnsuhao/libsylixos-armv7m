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
** 文   件   名: termios.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2012 年 01 月 10 日
**
** 描        述: sio -> termios 有限兼容库.
**
** BUG:
2015.06.02  加入波特率转换表
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
#include "termios.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_SIO_DEVICE_EN > 0)
/*********************************************************************************************************
  波特率表
*********************************************************************************************************/
#define TBAUD_SZ    16

struct termios_baud {
    const LONG      baud;
    const tcflag_t  bcode;
};

static const struct termios_baud   baud_table[TBAUD_SZ] = {
    {0,     B0}, 
    {50,    B50}, 
    {75,    B75}, 
    {110,   B110}, 
    {134,   B134}, 
    {150,   B150}, 
    {200,   B200}, 
    {300,   B300}, 
    {600,   B600}, 
    {1200,  B1200}, 
    {1800,  B1800}, 
    {2400,  B2400}, 
    {4800,  B4800}, 
    {9600,  B9600}, 
    {19200, B19200}, 
    {38400, B38400}
};

static const struct termios_baud   baud_table_ex[TBAUD_SZ] = {
    {57600,   B57600},
    {115200,  B115200},
    {230400,  B230400},
    {460800,  B460800},
    {500000,  B500000},
    {576000,  B576000},
    {921600,  B921600},
    {1000000, B1000000},
    {1152000, B1152000},
    {1500000, B1500000},
    {2000000, B2000000},
    {2500000, B2500000},
    {3000000, B3000000},
    {3500000, B3500000},
    {4000000, B4000000}
};
/*********************************************************************************************************
  波特率表操作
*********************************************************************************************************/
static LW_INLINE  tcflag_t  baud_to_bcode (LONG  baud)
{
    int  i;
    const struct termios_baud  *pbaudtbl;
    
    if (baud >= 57600) {
        pbaudtbl = baud_table_ex;
        
    } else {
        pbaudtbl = baud_table;
    }
    
    for (i = 0; i < TBAUD_SZ; i++) {
        if (pbaudtbl->baud == baud) {
            return  (pbaudtbl->bcode);
        }
        pbaudtbl++;
    }
    
    return  (0);
}

static LW_INLINE  LONG  bcode_to_baud (tcflag_t  bcode)
{
    int  i;
    const struct termios_baud  *pbaudtbl;
    
    if (bcode & CBAUDEX) {
        pbaudtbl = baud_table_ex;
        
    } else {
        pbaudtbl = baud_table;
    }
    
    for (i = 0; i < TBAUD_SZ; i++) {
        if (pbaudtbl->bcode == bcode) {
            return  (pbaudtbl->baud);
        }
        pbaudtbl++;
    }
    
    return  (9600);
}
/*********************************************************************************************************
** 函数名称: cfgetispeed
** 功能描述: 获得 termios 结构中的输入波特率
** 输　入  : tp     termios 结构
** 输　出  : 波特率
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
speed_t cfgetispeed (const struct termios *tp)
{
    return  (tp->c_cflag & CBAUD);
}
/*********************************************************************************************************
** 函数名称: cfgetospeed
** 功能描述: 获得 termios 结构中的输出波特率
** 输　入  : tp     termios 结构
** 输　出  : 波特率
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
speed_t cfgetospeed (const struct termios *tp)
{
    return  (tp->c_cflag & CBAUD);
}
/*********************************************************************************************************
** 函数名称: cfsetispeed
** 功能描述: 设置 termios 结构中的输入波特率
** 输　入  : tp     termios 结构
**           speed  波特率
** 输　出  : 设置结果
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int cfsetispeed (struct termios *tp, speed_t speed)
{
    if (speed & ~CBAUD) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    tp->c_cflag |= speed;
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: cfsetospeed
** 功能描述: 设置 termios 结构中的输出波特率
** 输　入  : tp     termios 结构
**           speed  波特率
** 输　出  : 设置结果
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int cfsetospeed (struct termios *tp, speed_t speed)
{
    if (speed & ~CBAUD) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    tp->c_cflag |= speed;
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: tcdrain
** 功能描述: 等待指定的文件描述符数据发送完毕
** 输　入  : fd      文件描述符
** 输　出  : 等待结果
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  tcdrain (int  fd)
{
    INT     iRet;

    __THREAD_CANCEL_POINT();                                            /*  测试取消点                  */

    iRet = ioctl(fd, FIOSYNC, 0);                                       /*  将所有数据同步输出          */
    
    return  (iRet);
}
/*********************************************************************************************************
** 函数名称: tcflow
** 功能描述: 挂起设备输出, 目前不支持
** 输　入  : fd      文件描述符
**           action  行为
** 输　出  : 结果
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  tcflow (int  fd, int  action)
{
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: tcflush
** 功能描述: 清空设备缓冲
** 输　入  : fd      文件描述符
**           queue   行为
** 输　出  : 结果
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  tcflush (int  fd, int  queue)
{
    switch (queue) {
    
    case TCIFLUSH:
        return  (ioctl(fd, FIORFLUSH, 0));
    
    case TCOFLUSH:
        return  (ioctl(fd, FIOWFLUSH, 0));
    
    case TCIOFLUSH:
        return  (ioctl(fd, FIOFLUSH, 0));
    
    default:
        errno = EINVAL;
        return  (PX_ERROR);
    }
}
/*********************************************************************************************************
** 函数名称: tcgetsid
** 功能描述: 获得当前所属会话进程 (不支持)
** 输　入  : fd      文件描述符
** 输　出  : 进程描述符
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
pid_t tcgetsid (int fd)
{
    errno = ENOSYS;
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: tcsendbreak
** 功能描述: 传送连续的 0 值比特流，持续一段时间，如果终端使用异步串行数据传输的话。如果 duration 是 0，
             它至少传输 0.25 秒，不会超过 0.5 秒。如果 duration 非零，它发送的时间长度由实现定义。
** 输　入  : fd      文件描述符
**           duration
** 输　出  : 进程描述符
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int tcsendbreak (int  fd, int  duration)
{
    errno = ENOSYS;
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: tcgetattr
** 功能描述: 传送连续的 0 值比特流，持续一段时间，如果终端使用异步串行数据传输的话。如果 duration 是 0，
             它至少传输 0.25 秒，不会超过 0.5 秒。如果 duration 非零，它发送的时间长度由实现定义。
** 输　入  : fd      文件描述符
**           tp      termios 结构
** 输　出  : 进程描述符
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  tcgetattr (int  fd, struct termios *tp)
{
    LONG    lOpt = 0;
    LONG    lHwOpt = 0;
    LONG    lBaud = 0;
    INT     iError;
    
    if (!tp) {
        errno = EINVAL;
        return  (PX_ERROR);
    }
    
    lib_bzero(tp, sizeof(struct termios));
    
    iError = ioctl(fd, FIOGETOPTIONS, &lOpt);
    if (iError < ERROR_NONE) {
        return  (iError);
    }
    
    ioctl(fd, SIO_HW_OPTS_GET, &lHwOpt);                                /*  这两条命令是由驱动程序提供  */
    ioctl(fd, SIO_BAUD_GET, &lBaud);

    iError = ioctl(fd, FIOGETCC, tp->c_cc);
    if (iError < ERROR_NONE) {
        return  (iError);
    }
    
    if (lOpt & OPT_ECHO) {
        tp->c_lflag |= (ECHO | ECHOE);
    }
    if (lOpt & OPT_CRMOD) {
        tp->c_iflag |= (ICRNL | INLCR);
        tp->c_oflag |= ONLCR;
    }
    if (lOpt & OPT_TANDEM) {
        tp->c_iflag |= (IXON | IXOFF);
    }
    if (lOpt & OPT_MON_TRAP) {
        tp->c_iflag |= BRKINT;
    }
    if (lOpt & OPT_ABORT) {
        tp->c_iflag |= BRKINT;
    }
    if (lOpt & OPT_LINE) {
        tp->c_lflag |= (ICANON | ECHOK);
    }
    
    if ((lHwOpt & CLOCAL) == 0) {
        tp->c_cflag |= CRTSCTS;
    } else {
        tp->c_cflag |= CLOCAL;
    }
    if (lHwOpt & CREAD) {
        tp->c_cflag |= CREAD;
    }
    
    tp->c_cflag |= (unsigned int)(lHwOpt & CSIZE);
    
    if (lHwOpt & HUPCL) {
        tp->c_cflag |= HUPCL;
    }
    if (lHwOpt & STOPB) {
        tp->c_cflag |= STOPB;
    }
    if (lHwOpt & PARENB) {
        tp->c_cflag |= PARENB;
    }
    if (lHwOpt & PARODD) {
        tp->c_cflag |= PARODD;
    }
    
    tp->c_cflag |= baud_to_bcode(lBaud);
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: tcgetattr
** 功能描述: 传送连续的 0 值比特流，持续一段时间，如果终端使用异步串行数据传输的话。如果 duration 是 0，
             它至少传输 0.25 秒，不会超过 0.5 秒。如果 duration 非零，它发送的时间长度由实现定义。
** 输　入  : fd      文件描述符
**           opt     选项 TCSANOW, TCSADRAIN, TCSAFLUSH
**           tp      termios 结构
** 输　出  : 进程描述符
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
int  tcsetattr (int  fd, int  opt, const struct termios  *tp)
{
    LONG    lOpt = 0;
    LONG    lHwOpt = 0;
    LONG    lBaud = 0;
    
    LONG    lOptOld = 0;
    LONG    lHwOptOld = 0;
    LONG    lBaudOld = 0;
    INT     iError;

    if (opt == TCSADRAIN) {
        iError = ioctl(fd, FIOSYNC, 0);
        if (iError < ERROR_NONE) {
            return  (iError);
        }
    
    } else if (opt == TCSAFLUSH) {
        iError = ioctl(fd, FIOSYNC, 0);
        if (iError < ERROR_NONE) {
            return  (iError);
        }
        iError = ioctl(fd, FIOFLUSH, 0);
        if (iError < ERROR_NONE) {
            return  (iError);
        }
    }

    if (tp->c_lflag & ECHO) {
        lOpt |= OPT_ECHO;
    }
    if ((tp->c_iflag & ICRNL) ||
        (tp->c_oflag & ONLCR)) {
        lOpt |= OPT_CRMOD;
    }
    if (tp->c_iflag & IXON) {
        lOpt |= OPT_TANDEM;
    }
    if (tp->c_iflag & BRKINT) {
        lOpt |= OPT_ABORT;
        lOpt |= OPT_MON_TRAP;
    }
    if (tp->c_lflag & ICANON) {
        lOpt |= OPT_LINE;
    }
    
    if (tp->c_cflag & CLOCAL) {
        lHwOpt |= CLOCAL;
    }
    if (tp->c_cflag & CRTSCTS) {
        lHwOpt &= ~CLOCAL;
    }
    if (tp->c_cflag & CREAD) {
        lHwOpt |= CREAD;
    }
    
    lHwOpt |= (LONG)(tp->c_cflag & CSIZE);
    
    if (tp->c_cflag & HUPCL) {
        lHwOpt |= HUPCL;
    }
    if (tp->c_cflag & STOPB) {
        lHwOpt |= STOPB;
    }
    if (tp->c_cflag & PARENB) {
        lHwOpt |= PARENB;
    }
    if (tp->c_cflag & PARODD) {
        lHwOpt |= PARODD;
    }
    
    lBaud = bcode_to_baud(tp->c_cflag & CBAUD);
    
    iError = ioctl(fd, FIOGETOPTIONS, &lOptOld);
    if (iError < ERROR_NONE) {
        return  (iError);
    }
    if (lOptOld != lOpt) {
        iError = ioctl(fd, FIOSETOPTIONS, lOpt);
        if (iError < ERROR_NONE) {
            return  (iError);
        }
    }
    
    iError = ioctl(fd, SIO_HW_OPTS_GET, &lHwOptOld);                    /*  pty 没有此功能               */
    if ((iError >= ERROR_NONE) && (lHwOptOld != lHwOpt)) {
        ioctl(fd, SIO_HW_OPTS_SET, lHwOpt);
    }
    
    iError = ioctl(fd, SIO_BAUD_GET, &lBaudOld);                        /*  pty 没有此功能               */
    if ((iError >= ERROR_NONE) && (lBaudOld != lBaud)) {
        ioctl(fd, SIO_BAUD_SET, lBaud);
    }
    
    iError = ioctl(fd, FIOSETCC, tp->c_cc);
    if (iError < ERROR_NONE) {
        return  (iError);
    }
    
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: cfmakeraw
** 功能描述: 将选项设置为原始模式.
** 输　入  : tp      termios 结构
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
void  cfmakeraw (struct termios *tp)
{
    if (!tp) {
        return;
    }
    
    tp->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                 | INLCR | IGNCR | ICRNL | IXON);
    tp->c_oflag &= ~OPOST;
    tp->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tp->c_cflag &= ~(CSIZE | PARENB);
    tp->c_cflag |= CS8;
}

#endif                                                                  /*  (LW_CFG_DEVICE_EN > 0) &&   */
                                                                        /*  (LW_CFG_SIO_DEVICE_EN > 0)  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
