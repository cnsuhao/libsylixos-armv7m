/**********************************************************************************************************
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
** 文   件   名: armMmuCommon.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 12 月 09 日
**
** 描        述: ARM 体系架构 MMU 通用函数支持.
*********************************************************************************************************/

#ifndef __ARMMMUCOMMON_H
#define __ARMMMUCOMMON_H

/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_VMM_EN > 0

/*********************************************************************************************************
  基本操作
*********************************************************************************************************/

UINT32  armMmuAbtFaultStatus(VOID);
UINT32  armMmuPreFaultStatus(VOID);
UINT32  armMmuAbtFaultAddr(VOID);
VOID    armMmuInitSysRom(VOID);
VOID    armMmuEnable(VOID);
VOID    armMmuDisable(VOID);
VOID    armMmuEnableWriteBuffer(VOID);
VOID    armMmuDisableWriteBuffer(VOID);
VOID    armMmuEnableAlignFault(VOID);
VOID    armMmuDisableAlignFault(VOID);
VOID    armMmuSetDomain(UINT32  uiDomainAttr);
VOID    armMmuSetTTBase(LW_PGD_TRANSENTRY  *pgdEntry);
VOID    armMmuSetTTBase1(LW_PGD_TRANSENTRY  *pgdEntry);
VOID    armMmuInvalidateTLB(VOID);
VOID    armMmuInvalidateTLBMVA(PVOID pvVAddr);
VOID    armMmuSetProcessId(pid_t  pid);

#endif                                                                  /*  LW_CFG_VMM_EN > 0           */
/*********************************************************************************************************
  异常信息获取
*********************************************************************************************************/

addr_t  armGetAbtAddr(VOID);
ULONG   armGetAbtType(VOID);
addr_t  armGetPreAddr(addr_t  ulRetLr);
ULONG   armGetPreType(VOID);

#endif                                                                  /*  __ARMMMUCOMMON_H            */
/*********************************************************************************************************
  END
*********************************************************************************************************/
