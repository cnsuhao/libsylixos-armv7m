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
** 文   件   名: px_sched.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 12 月 30 日
**
** 描        述: posix 调度兼容库.
*********************************************************************************************************/

#ifndef __PX_SCHED_H
#define __PX_SCHED_H

#include "SylixOS.h"                                                    /*  操作系统头文件              */

/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_POSIX_EN > 0

#include "px_sched_param.h"

#ifdef __cplusplus
extern "C" {
#endif                                                                  /*  __cplusplus                 */

#define PX_ID_VERIFY(id, type)  do {            \
                                    if ((id) == 0) {    \
                                        (id) = (type)API_ThreadIdSelf();    \
                                    }   \
                                } while (0)

/*********************************************************************************************************
  sched policies
*********************************************************************************************************/

#define SCHED_FIFO              LW_OPTION_SCHED_FIFO
#define SCHED_RR                LW_OPTION_SCHED_RR
#define SCHED_OTHER             LW_OPTION_SCHED_RR

/*********************************************************************************************************
  sched api
*********************************************************************************************************/

LW_API int              sched_get_priority_max(int  iPolicy);
LW_API int              sched_get_priority_min(int  iPolicy);
LW_API int              sched_yield(void);
LW_API int              sched_setparam(pid_t  lThreadId, const struct sched_param  *pschedparam);
LW_API int              sched_getparam(pid_t  lThreadId, struct sched_param  *pschedparam);
LW_API int              sched_setscheduler(pid_t                      lThreadId, 
                                           int                        iPolicy, 
                                           const struct sched_param  *pschedparam);
LW_API int              sched_getscheduler(pid_t  lThreadId);
LW_API int              sched_rr_get_interval(pid_t  lThreadId, struct timespec  *interval);

#ifdef __cplusplus
}
#endif                                                                  /*  __cplusplus                 */

#endif                                                                  /*  LW_CFG_POSIX_EN > 0         */
#endif                                                                  /*  __PX_SCHED_H                */
/*********************************************************************************************************
  END
*********************************************************************************************************/
