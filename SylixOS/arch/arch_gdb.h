/*********************************************************************************************************
**
**                                    �й�������Դ��֯
**
**                                   Ƕ��ʽʵʱ����ϵͳ
**
**                                SylixOS(TM)  LW : long wing
**
**                               Copyright All Rights Reserved
**
**--------------�ļ���Ϣ--------------------------------------------------------------------------------
**
** ��   ��   ��: arch_gdb.h
**
** ��   ��   ��: Han.Hui (����)
**
** �ļ���������: 2014 �� 05 �� 22 ��
**
** ��        ��: SylixOS ��ϵ���� GDB ���Խӿ�.
*********************************************************************************************************/

#ifndef __ARCH_GDB_H
#define __ARCH_GDB_H

#include "config/cpu/cpu_cfg.h"

#if (defined LW_CFG_CPU_ARCH_ARM)
#include "./arm/arm_gdb.h"

#elif (defined LW_CFG_CPU_ARCH_X86)
#include "./x86/x86_gdb.h"

#elif (defined LW_CFG_CPU_ARCH_MIPS)
#include "./mips/mips_gdb.h"

#elif (defined LW_CFG_CPU_ARCH_MIPS64)
#include "./mips/mips64_gdb.h"

#elif (defined LW_CFG_CPU_ARCH_PPC)
#include "./ppc/ppc_gdb.h"
#endif                                                                  /*  LW_CFG_CPU_ARCH_ARM         */

#endif                                                                  /*  __ARCH_GDB_H                */
/*********************************************************************************************************
  END
*********************************************************************************************************/