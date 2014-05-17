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
** 文   件   名: armCp15.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 12 月 09 日
**
** 描        述: ARM CP15 基本操作.
*********************************************************************************************************/

#ifndef __ARMCP15_H
#define __ARMCP15_H

/*********************************************************************************************************
  基本操作
*********************************************************************************************************/
#if LW_CFG_ARM_CP15 > 0

UINT32  armCp15MainIdReg(VOID);
UINT32  armCp15TcmTypeReg(VOID);
UINT32  armCp15LtbTypeReg(VOID);
UINT32  armCp15ControlReg(VOID);
UINT32  armCp15AuxCtrlReg(VOID);
UINT32  armCp15CACtrlReg(VOID);
VOID    armFastBusMode(VOID);
VOID    armAsyncBusMode(VOID);
VOID    armSyncBusMode(VOID);
VOID    armWaitForInterrupt(VOID);

VOID    armVectorBaseAddrSet(addr_t addr);
VOID    armHighVectorEnable(VOID);
VOID    armHighVectorDisable(VOID);

/*********************************************************************************************************
  ARMv7 以上版本
*********************************************************************************************************/

VOID    armBranchPredictorInvalidate(VOID);
VOID    armBranchPredictorInvalidateInnerShareable(VOID);
VOID    armBranchPredictionDisable(VOID);
VOID    armBranchPredictionEnable(VOID);

#define AUX_CONTROL_PARITY                          (1 << 9)
#define AUX_CONTROL_ALLOC_IN_ONE_WAY                (1 << 8)
#define AUX_CONTROL_EXECLUSIVE_CACHE                (1 << 7)
#define AUX_CONTROL_SMP                             (1 << 6)
#define AUX_CONTROL_WRITE_FULL_LINE                 (1 << 3)
#define AUX_CONTROL_L1_PREFETCH                     (1 << 2)
#define AUX_CONTROL_L2_PREFETCH                     (1 << 1)
#define AUX_CONTROL_MAINTENANCE_BROADCAST           (1 << 0)

VOID    armAuxControlFeatureDisable(UINT32  uiFeature);
VOID    armAuxControlFeatureEnable(UINT32  uiFeature);

#define CP15_CONTROL_THUMB_EXCEPT                   (1 << 30)
#define CP15_CONTROL_ACCESSFLAG                     (1 << 29)
#define CP15_CONTROL_TEXREMAP                       (1 << 28)
#define CP15_CONTROL_NMFI                           (1 << 27)
#define CP15_CONTROL_EE                             (1 << 25)
#define CP15_CONTROL_HA                             (1 << 17)
#define CP15_CONTROL_RR_EN                          (1 << 14)
#define CP15_CONTROL_HVECTOR_EN                     (1 << 13)
#define CP15_CONTROL_ICACHE_EN                      (1 << 12)
#define CP15_CONTROL_Z_EN                           (1 << 11)
#define CP15_CONTROL_SWP_EN                         (1 << 10)
#define CP15_CONTROL_DCACHE_EN                      (1 <<  2)
#define CP15_CONTROL_ALIGN_CHCK                     (1 <<  1)
#define CP15_CONTROL_MMU                            (1 <<  0)

VOID    armControlFeatureEnable(UINT32  uiFeature);
VOID    armControlFeatureDisable(UINT32  uiFeature);

#endif                                                                  /*  LW_CFG_ARM_CP15 > 0         */
#endif                                                                  /*  __ARMCP15_H                 */
/*********************************************************************************************************
  END
*********************************************************************************************************/
