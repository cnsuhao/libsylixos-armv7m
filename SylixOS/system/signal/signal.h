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
** 文   件   名: signal.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2007 年 06 月 03 日
**
** 描        述: 这是系统信号相关定义.

** BUG:
2011.11.22  将信号个数升级到 1 ~ 63.
2012.12.09  加入对标准信号 code 的支持.
2013.05.05  加入 SA_RESTART 支持.
*********************************************************************************************************/

#ifndef  __SIGNAL_H
#define  __SIGNAL_H

#if  LW_CFG_SIGNAL_EN > 0
/*********************************************************************************************************
  SIGNAL NUMBER 不可修改!
  
  信号句柄是软件层次中断, 在执行信号句柄时, 理论是不允许调用带有阻塞可能的函数.
  SylixOS 规定信号句柄绝对不允许调用可能会有 suspend 操作的函数, 例如: API_ThreadSuspend
  API_ThreadDelete...
*********************************************************************************************************/

/*********************************************************************************************************
  注意:
  信号屏蔽与忽略:
  
  信号屏蔽: 当前如果是 kill 信号到来, 且被屏蔽, 那么信号将得不到运行. 但是, 一旦被解除屏蔽, 不过接收到多少
            次 kill 的信号, 将会被执行一次. 
            当前如果是 queue 信号到来, 且被屏蔽, 那么信号将得不到运行. 但是, 一旦被解除屏蔽, 在屏蔽的过程中
            接收到多少信号, 就运行多少次.
            当前如果是其他信号到来, 如 timer, aio, msgq 等等, 则直接忽略.
            
  信号忽略: 当信号的处理句柄为忽略时, 接收到的信号会直接抛弃, 完全不管屏蔽状态.
*********************************************************************************************************/

#define  NSIG                   63                                      /*  信号数量                    */

/*********************************************************************************************************
  信号类型
*********************************************************************************************************/

#define  SIGHUP                 1                                       /*  挂断控制终端或进程          */
#define  SIGINT                 2                                       /*  来自键盘的中断              */
#define  SIGQUIT                3                                       /*  来自键盘的退出              */
#define  SIGILL                 4                                       /*  非法指令                    */
#define  SIGTRAP                5                                       /*  跟踪断点                    */
#define  SIGABRT                6                                       /*  异常结束                    */
#define  SIGUNUSED              7                                       /*  没有使用                    */
#define  SIGFPE                 8                                       /*  协处理器出错                */
#define  SIGKILL                9                                       /*  强迫进程结束                */
#define  SIGBUS                10                                       /*  bus error                   */
#define  SIGSEGV               11                                       /*  无效内存引用                */
#define  SIGUNUSED2            12                                       /*  暂时没有使用2               */
#define  SIGPIPE               13                                       /*  管道写出错，无读者          */
#define  SIGALRM               14                                       /*  实时定时器报警              */
#define  SIGTERM               15                                       /*  进程中止                    */
#define  SIGCNCL               16                                       /*  线程取消信号                */
#define  SIGSTOP               17                                       /*  停止进程执行                */
#define  SIGTSTP               18                                       /*  tty发出停止进程             */
#define  SIGCONT               19                                       /*  恢复进程继续执行            */
#define  SIGCHLD               20                                       /*  子进程停止或被终止          */
#define  SIGTTIN               21                                       /*  后台进程请求输入            */
#define  SIGTTOU               22                                       /*  后台进程请求输出            */
#define  SIGCANCEL             SIGTERM

#define  SIGUSR1               30                                       /*  user defined signal 1       */
#define  SIGUSR2               31                                       /*  user defined signal 2       */

#define  SIGPOLL               32                                       /*  pollable event              */
#define  SIGPROF               33                                       /*  profiling timer expired     */
#define  SIGSYS                34                                       /*  bad system call             */
#define  SIGURG                35                                       /*  high bandwidth data is      */
                                                                        /*  available at socket         */
#define  SIGVTALRM             36                                       /*  virtual timer expired       */
#define  SIGXCPU               37                                       /*  CPU time limit exceeded     */
#define  SIGXFSZ               38                                       /* file size time limit exceeded*/

#define  SIGWINCH              41

/*********************************************************************************************************
  实时信号范围
*********************************************************************************************************/

#define  SIGRTMIN              48                                       /*  最小实时信号                */
#define  SIGRTMAX              63                                       /*  最大实时信号                */

/*********************************************************************************************************
  不可执行的信号句柄定义
*********************************************************************************************************/

#define  SIG_ERR               (PSIGNAL_HANDLE)-1                       /*  错误信号句柄                */
#define  SIG_DFL               (PSIGNAL_HANDLE)0                        /*  默认信号句柄                */
#define  SIG_IGN               (PSIGNAL_HANDLE)1                        /*　忽略的信号句柄　　　        */

/*********************************************************************************************************
  sa_flag 参数
*********************************************************************************************************/

#define  SA_NOCLDSTOP          0x00000001                               /*  子进程被删除时不要产生      */
                                                                        /*  SIGCHLD信号                 */
#define  SA_SIGINFO            0x00000002                               /*  信号句柄需要 siginfo 参数   */

#define  SA_RESTART            0x10000000                               /*  执行信号句柄后, 重启调用    */
#define  SA_INTERRUPT          0x20000000
#define  SA_NOMASK             0x40000000                               /*  不阻止在指定信号处理句柄中  */
                                                                        /*  再收到信号                  */
#define  SA_ONESHOT            0x80000000                               /*  信号句柄一旦被调用就恢复到  */
                                                                        /*  默认状态                    */

#define  SA_RESETHAND          SA_ONESHOT                               /*  执行句柄后, 将信号句柄设置为*/
                                                                        /*  忽略                        */
/*********************************************************************************************************
  sigprocmask() 改变信号集屏蔽码
*********************************************************************************************************/

#define  SIG_BLOCK             1                                        /*  在阻塞信号集上给定的信号集  */
#define  SIG_UNBLOCK           2                                        /*  从阻塞信号集上删除的信号集  */
#define  SIG_SETMASK           3                                        /*  设置信号阻塞集              */

/*********************************************************************************************************
  信号产生源定义
*********************************************************************************************************/

#define  SI_KILL               0                                        /* 使用 kill() 发送的信号       */
#define  SI_QUEUE              2                                        /* 使用 sigqueue() 发送的信号   */
#define  SI_TIMER              3                                        /* POSIX 定时器发起的信号       */
#define  SI_ASYNCIO            4                                        /* 异步 I/O 系统完成发送的信号  */
#define  SI_MESGQ              5                                        /* 接收到一则消息产生的信号     */

/*********************************************************************************************************
  信号 code
*********************************************************************************************************/
/*********************************************************************************************************
  SIGILL
*********************************************************************************************************/

#define  ILL_ILLOPC            1                                        /* Illegal opcode               */
#define  ILL_ILLOPN            2                                        /* Illegal operand              */
#define  ILL_ILLADR            3                                        /* Illegal addressing mode      */
#define  ILL_ILLTRP            4                                        /* Illegal trap                 */
#define  ILL_PRVOPC            5                                        /* Priviledged instruction      */
#define  ILL_PRVREG            6                                        /* Priviledged register         */
#define  ILL_COPROC            7                                        /* Coprocessor error            */
#define  ILL_BADSTK            8                                        /* Internal stack error         */

/*********************************************************************************************************
  SIGFPE
*********************************************************************************************************/

#define  FPE_INTDIV            1                                        /* Integer divide by zero       */
#define  FPE_INTOVF            2                                        /* Integer overflow             */
#define  FPE_FLTDIV            3                                        /* Floating-point divide by zero*/
#define  FPE_FLTOVF            4                                        /* Floating-point overflow      */
#define  FPE_FLTUND            5                                        /* Floating-point underflow     */
#define  FPE_FLTRES            6                                        /* Floating-point inexact result*/
#define  FPE_FLTSUB            7                                        /* Subscript out of range       */

/*********************************************************************************************************
  SIGSEGV
*********************************************************************************************************/

#define  SEGV_MAPERR           1                                        /* Address not mapped to object */
#define  SEGV_ACCERR           2                                        /* Invalid permissions for      */
                                                                        /* mapped object                */
/*********************************************************************************************************
  SIGBUS
*********************************************************************************************************/

#define  BUS_ADRALN            1                                        /* Invalid address alignment    */
#define  BUS_ADRERR            2                                        /* Nonexistent physical memory  */
#define  BUS_OBJERR            3                                        /* Object-specific hardware err */

/*********************************************************************************************************
  SIGTRAP
*********************************************************************************************************/

#define  TRAP_BRKPT            1                                        /* Process breakpoint           */
#define  TRAP_TRACE            2                                        /* Process trap trace           */

/*********************************************************************************************************
  SIGCHLD
*********************************************************************************************************/

#define  CLD_EXITED            1                                        /* Child has exited             */
#define  CLD_KILLED            2                                        /* Child has terminated abnormally (no core) */
#define  CLD_DUMPED            3                                        /* Child has terminated abnormally           */
#define  CLD_TRAPPED           4                                        /* Traced child has trapped     */
#define  CLD_STOPPED           5                                        /* Child has stopped            */
#define  CLD_CONTINUED         6                                        /* Stopped child has continued  */

/*********************************************************************************************************
  SIGPOLL
*********************************************************************************************************/

#define  POLL_IN               1                                        /* Data input available         */
#define  POLL_OUT              2                                        /* Output buffers available     */
#define  POLL_MSG              3                                        /* Input message available      */
#define  POLL_ERR              4                                        /* I/O error                    */
#define  POLL_PRI              5                                        /* High priority input available*/
#define  POLL_HUP              6                                        /* Device disconnected          */

/*********************************************************************************************************
  基本操作
*********************************************************************************************************/

#ifdef __SYLIXOS_KERNEL
#define  __issig(m)            (1 <= (m) && (m) <= NSIG)                /*  信号是否有效                */
#define  __sigmask(m)          (sigset_t)((1 << ((m) - 1)))             /*  获得信号掩码                */
#define  __sigindex(m)         (m - 1)                                  /*  索引表下标                  */
#endif                                                                  /*  __SYLIXOS_KERNEL            */

/*********************************************************************************************************
  struct sigevent 中, sigev_notify 参数
  
  SIGEV_THREAD 需要 POSIX 库支持.
*********************************************************************************************************/

#define SIGEV_NONE             1                                        /*  No asynchronous notification*/
#define SIGEV_SIGNAL           2                                        /*  A queued signal             */

#if LW_CFG_POSIX_EN > 0
#define SIGEV_THREAD           3                                        /*  invoke sigev_notify_function*/
                                                                        /*  as if it were the start     */
                                                                        /*  function of a new thread.   */
                                                                        
#define SIGEV_THREAD_ID        4                                        /*  As for SIGEV_SIGNAL, but the*/
                                                                        /*  signal is targeted at the   */
                                                                        /*  thread whose ID is given in */
                                                                        /*  sigev_notify_thread_id      */
#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
/*********************************************************************************************************
  SIGNAL FUNCTION 
*********************************************************************************************************/

LW_API  VOID      (*signal(INT   iSigNo, PSIGNAL_HANDLE  pfuncHandler))(INT);
LW_API  INT         raise(INT    iSigNo);
LW_API  INT         kill(LW_OBJECT_HANDLE  ulId, INT   iSigNo);
LW_API  INT         sigqueue(LW_OBJECT_HANDLE  ulId, INT   iSigNo, const union   sigval   sigvalue);

LW_API  INT         sigemptyset(sigset_t       *psigset);
LW_API  INT         sigfillset(sigset_t        *psigset);
LW_API  INT         sigaddset(sigset_t         *psigset, INT  iSigNo);
LW_API  INT         sigdelset(sigset_t         *psigset, INT  iSigNo);
LW_API  INT         sigismember(const sigset_t *psigset, INT  iSigNo);

LW_API  INT         sigaction(INT  iSigNo, const struct sigaction  *psigactionNew,
                              struct sigaction  *psigactionOld);

LW_API  INT         sigsetmask(INT  iMask);                         /*  建议使用 sigprocmask       */
LW_API  INT         sigblock(INT    iBlock);                        /*  建议使用 sigprocmask       */

LW_API  INT         sigprocmask(INT  iHow, const sigset_t *sigset, sigset_t *sigsetOld);

LW_API  INT         sigvec(INT  iSigNo, const struct sigvec *pvec, struct sigvec *pvecOld);

LW_API  INT         sigpending(sigset_t  *sigset);

LW_API  INT         sigsuspend(const sigset_t  *sigsetMask);
LW_API  INT         pause(VOID);
LW_API  INT         sigwait(const sigset_t      *sigset, INT  *piSig);
LW_API  INT         sigtimedwait(const sigset_t *sigset, struct siginfo *__value,
                                 const struct timespec *);
LW_API  INT         sigwaitinfo(const sigset_t *sigset, struct  siginfo  *psiginfo);

/*********************************************************************************************************
  SIGNAL 内核函数
*********************************************************************************************************/

#ifdef __SYLIXOS_KERNEL
LW_API  INT         sigTrap(LW_OBJECT_HANDLE  ulId, const union sigval  sigvalue);
#endif                                                                  /*  __SYLIXOS_KERNEL            */

#endif                                                                  /*  LW_CFG_SIGNAL_EN > 0        */
#endif                                                                  /*  __SIGNAL_H                  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
