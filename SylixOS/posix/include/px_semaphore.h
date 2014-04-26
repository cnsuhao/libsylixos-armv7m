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
** 文   件   名: px_semaphore.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 12 月 30 日
**
** 描        述: posix 兼容库信号量部分.
*********************************************************************************************************/

#ifndef __PX_SEMAPHORE_H
#define __PX_SEMAPHORE_H

#include "SylixOS.h"                                                    /*  操作系统头文件              */

/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_POSIX_EN > 0
/*********************************************************************************************************
  由于 sem_t 中包含 __PX_NAME_NODE. 所以这里必须加入 posixLib.h 头文件.
*********************************************************************************************************/
#include "posixLib.h"                                                   /*  posix 内部公共库            */

#ifdef __cplusplus
extern "C" {
#endif                                                                  /*  __cplusplus                 */

#define SEM_FAILED              (LW_NULL)

#ifndef SEM_VALUE_MAX
#define SEM_VALUE_MAX           (__ARCH_INT_MAX)                        /*  posix sem is 'int' type     */
#endif                                                                  /*  SEM_VALUE_MAX               */

/*********************************************************************************************************
  sem handle
*********************************************************************************************************/

typedef struct {
    PVOID                   SEM_pvPxSem;                                /*  信号量内部结构              */
    PLW_RESOURCE_RAW        SEM_presraw;                                /*  资源管理节点                */
    ULONG                   SEM_ulPad[5];
} sem_t;
#define SEMAPHORE_INITIALIZER   {LW_NULL, LW_NULL}

/*********************************************************************************************************
  sem api
*********************************************************************************************************/
LW_API int          sem_init(sem_t  *psem, int  pshared, unsigned int  value);
LW_API int          sem_destroy(sem_t  *psem);
LW_API sem_t       *sem_open(const char  *name, int  flag, ...);
LW_API int          sem_close(sem_t  *psem);
LW_API int          sem_unlink(const char *name);
LW_API int          sem_wait(sem_t  *psem);
LW_API int          sem_trywait(sem_t  *psem);
LW_API int          sem_timedwait(sem_t  *psem, const struct timespec *timeout);
#if LW_CFG_POSIXEX_EN > 0
LW_API int          sem_reltimedwait_np(sem_t  *psem, const struct timespec *rel_timeout);
#endif                                                                  /*  LW_CFG_POSIXEX_EN > 0       */
LW_API int          sem_post(sem_t  *psem);
LW_API int          sem_getvalue(sem_t  *psem, int  *pivalue);

#ifdef __cplusplus
}
#endif                                                                  /*  __cplusplus                 */

#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
#endif                                                                  /*  __PX_SEMAPHORE_H            */
/*********************************************************************************************************
  END
*********************************************************************************************************/
