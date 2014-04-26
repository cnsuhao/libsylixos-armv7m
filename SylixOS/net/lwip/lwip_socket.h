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
** 文   件   名: lwip_socket.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2012 年 12 月 18 日
**
** 描        述: socket 接口. (使用 sylixos io 系统包裹 lwip 文件描述符)
*********************************************************************************************************/

#ifndef __LWIP_SOCKET_H
#define __LWIP_SOCKET_H

/*********************************************************************************************************
  close-on-exec and non-blocking
*********************************************************************************************************/

#define SOCK_CLOEXEC    0x10000000                                      /* Atomically close-on-exec     */
#define SOCK_NONBLOCK   0x20000000                                      /* Atomically non-blocking      */

/*********************************************************************************************************
  The maximum backlog queue length.
*********************************************************************************************************/

#define SOMAXCONN       255                                             /* lwip max backlog is 0xff     */

/*********************************************************************************************************
  sockaddr_storage
  Desired design of maximum size and alignment.
*********************************************************************************************************/

#define _SS_MAXSIZE     128                                             /* maximum size.                */
#define _SS_ALIGNSIZE   (sizeof(int64_t))                               /* desired alignment.           */

/*********************************************************************************************************
  Definitions used for sockaddr_storage structure paddings design.
*********************************************************************************************************/

#define _SS_PAD1SIZE    (_SS_ALIGNSIZE - sizeof(sa_family_t))
#define _SS_PAD2SIZE    (_SS_MAXSIZE - (sizeof(sa_family_t) + \
                         _SS_PAD1SIZE + _SS_ALIGNSIZE))

/*********************************************************************************************************
  sockaddr_storage
*********************************************************************************************************/

struct sockaddr_storage {
  u8_t          ss_len;
  sa_family_t   ss_family;
  
  /*
   * Following fields are implementation-defined.
   */
  /* 
   * 6-byte pad; this is to make implementation-defined
   * pad up to alignment field that follows explicit in
   * the data structure. 
   */
  char          _ss_pad1[_SS_PAD1SIZE];
  
  /* 
   * Field to force desired structure
   * storage alignment. 
   */
  int64_t       _ss_align;
  
  /* 
   * 112-byte pad to achieve desired size,
   * _SS_MAXSIZE value minus size of ss_family
   * __ss_pad1, __ss_align fields is 112. 
   */
  char          _ss_pad2[_SS_PAD2SIZE];
};

/*********************************************************************************************************
  AF_PACKET flags
*********************************************************************************************************/

#ifndef MSG_TRUNC
#define MSG_TRUNC           0x200
#endif

/*********************************************************************************************************
  AF_UNIX sockopt
*********************************************************************************************************/

#ifndef SO_PASSCRED
#define SO_PASSCRED         16
#endif

#ifndef SO_PEERCRED
#define SO_PEERCRED         17
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL        0x4000
#endif

#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC    0x40000
#endif

struct msghdr {
    void         *msg_name;                                             /* optional address             */
    socklen_t     msg_namelen;                                          /* size of address              */
    struct iovec *msg_iov;                                              /* scatter/gather array         */
    size_t        msg_iovlen;                                           /* # elements in msg_iov        */
    void         *msg_control;                                          /* ancillary data, see below    */
    socklen_t     msg_controllen;                                       /* ancillary data buffer len    */
    int           msg_flags;                                            /* flags on received message    */
};

struct cmsghdr {
    socklen_t     cmsg_len;                                             /* Data byte count,             */
                                                                        /* including the cmsghdr.       */
    int           cmsg_level;                                           /* Originating protocol.        */
    int           cmsg_type;                                            /* Protocol-specific type.      */
    /* 
     * followed by u_char cmsg_data[];
     */
};

/*********************************************************************************************************
  RFC 2292 additions
*********************************************************************************************************/

#define CMSG_ALIGN(nbytes)      ROUND_UP(nbytes, sizeof(size_t))
#define CMSG_LEN(nbytes)        (CMSG_ALIGN(sizeof(struct cmsghdr)) + (nbytes))
#define CMSG_SPACE(nbytes)      (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(nbytes))

/*********************************************************************************************************
  given pointer to struct adatahdr, return pointer to data
*********************************************************************************************************/

#define	CMSG_DATA(cmsg)         ((u_char *)(cmsg) + CMSG_ALIGN(sizeof(struct cmsghdr)))

/*********************************************************************************************************
  given pointer to struct adatahdr, return pointer to next adatahdr
*********************************************************************************************************/

#define CMSG_NXTHDR(mhdr, cmsg) \
        (((caddr_t)(cmsg) + CMSG_ALIGN((cmsg)->cmsg_len) + \
        CMSG_ALIGN(sizeof(struct cmsghdr)) > \
	    (caddr_t)(mhdr)->msg_control + (mhdr)->msg_controllen) ? \
	    (struct cmsghdr *)NULL : \
	    (struct cmsghdr *)((caddr_t)(cmsg) + CMSG_ALIGN((cmsg)->cmsg_len)))
        
#define CMSG_FIRSTHDR(mhdr)     ((struct cmsghdr *)(mhdr)->msg_control)

/*********************************************************************************************************
  "Socket"-level control message types:
*********************************************************************************************************/

#define SCM_RIGHTS              0x01                                    /* access rights (array of int) */
#define SCM_CREDENTIALS         0x02                                    /* Linux certificate            */
#define SCM_CRED                0x03                                    /* BSD certificate              */

/*********************************************************************************************************
  AF_UNIX Linux certificate 
*********************************************************************************************************/

struct ucred {                                                          /* Linux                        */
    uint32_t        pid;                                                /* Sender's process ID          */
    uint32_t        uid;                                                /* Sender's user ID             */
    uint32_t        gid;                                                /* Sender's group ID            */
};

/*********************************************************************************************************
  AF_UNIX BSD certificate 
*********************************************************************************************************/

#define CMGROUP_MAX 16

struct cmsgcred {                                                       /* FreeBSD                      */
    pid_t           cmcred_pid;                                         /* PID of sending process       */
    uid_t           cmcred_uid;                                         /* real UID of sending process  */
    uid_t           cmcred_euid;                                        /* effective UID of sending process */
    gid_t           cmcred_gid;                                         /* real GID of sending process  */
    short           cmcred_ngroups;                                     /* number or groups             */
    gid_t           cmcred_groups[CMGROUP_MAX];                         /* groups                       */
};

/*********************************************************************************************************
  socket api
*********************************************************************************************************/

LW_API int          socketpair(int domain, int type, int protocol, int sv[2]);
LW_API int          socket(int domain, int type, int protocol);
LW_API int          accept(int s, struct sockaddr *addr, socklen_t *addrlen);
LW_API int          accept4(int s, struct sockaddr *addr, socklen_t *addrlen, int flags);
LW_API int          bind(int s, const struct sockaddr *name, socklen_t namelen);
LW_API int          shutdown(int s, int how);
LW_API int          connect(int s, const struct sockaddr *name, socklen_t namelen);
LW_API int          getsockname(int s, struct sockaddr *name, socklen_t *namelen);
LW_API int          getpeername(int s, struct sockaddr *name, socklen_t *namelen);
LW_API int          setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen);
LW_API int          getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen);
LW_API int          listen(int s, int backlog);
LW_API ssize_t      recv(int s, void *mem, size_t len, int flags);
LW_API ssize_t      recvfrom(int s, void *mem, size_t len, int flags,
                             struct sockaddr *from, socklen_t *fromlen);
LW_API ssize_t      recvmsg(int  s, struct msghdr *msg, int flags);
LW_API ssize_t      send(int s, const void *data, size_t size, int flags);
LW_API ssize_t      sendto(int s, const void *data, size_t size, int flags,
                           const struct sockaddr *to, socklen_t tolen);
LW_API ssize_t      sendmsg(int  s, const struct msghdr *msg, int flags);

/*********************************************************************************************************
  kernel function
*********************************************************************************************************/

#ifdef __SYLIXOS_KERNEL
VOID  __socketInit(VOID);
#endif                                                                  /*  __SYLIXOS_KERNEL            */

#endif                                                                  /*  __LWIP_SOCKET_H             */
/*********************************************************************************************************
  END
*********************************************************************************************************/
