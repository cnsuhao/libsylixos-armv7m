;/**********************************************************************************************************
;**
;**                                    中国软件开源组织
;**
;**                                   嵌入式实时操作系统
;**
;**                                       SylixOS(TM)
;**
;**                               Copyright  All Rights Reserved
;**
;**--------------文件信息--------------------------------------------------------------------------------
;**
;** 文   件   名: armCp15Asm.S
;**
;** 创   建   人: Han.Hui (韩辉)
;**
;** 文件创建日期: 2013 年 12 月 09 日
;**
;** 描        述: ARM 体系架构 CP15 基本操作.
;*********************************************************************************************************/

#ifndef ASSEMBLY
#define ASSEMBLY 1
#endif

#include <arch/assembler.h>

#if LW_CFG_ARM_CP15 > 0

    FILE_BEGIN()

    EXPORT_LABEL(armCp15MainIdReg)
    EXPORT_LABEL(armCp15TcmTypeReg)
    EXPORT_LABEL(armCp15LtbTypeReg)
    EXPORT_LABEL(armCp15ControlReg)
    EXPORT_LABEL(armCp15AuxCtrlReg)
    EXPORT_LABEL(armFastBusMode)
    EXPORT_LABEL(armAsyncBusMode)
    EXPORT_LABEL(armSyncBusMode)
    EXPORT_LABEL(armWaitForInterrupt)
    EXPORT_LABEL(armBranchPredictorInvalidate)
    EXPORT_LABEL(armBranchPredictorInvalidateInnerShareable)
    EXPORT_LABEL(armBranchPredictionEnable)
    EXPORT_LABEL(armBranchPredictionDisable)
    EXPORT_LABEL(armVectorBaseAddrSet)
    EXPORT_LABEL(armHighVectorEnable)
    EXPORT_LABEL(armHighVectorDisable)
    EXPORT_LABEL(armControlFeatureDisable)
    EXPORT_LABEL(armControlFeatureEnable)
    EXPORT_LABEL(armAuxControlFeatureEnable)
    EXPORT_LABEL(armAuxControlFeatureDisable)
    
;/*********************************************************************************************************
;  获得 CP15 基本信息
;*********************************************************************************************************/

FUNC_DEF(armCp15MainIdReg)
    MRC     p15, 0, R0, c0, c0, 0
    BX      LR
    FUNC_END()
    
FUNC_DEF(armCp15TcmTypeReg)
    MRC     p15, 0, R0, c0, c0, 2
    BX      LR
    FUNC_END()
    
FUNC_DEF(armCp15LtbTypeReg)
    MRC     p15, 0, R0, c0, c0, 3
    BX      LR
    FUNC_END()
    
FUNC_DEF(armCp15ControlReg)
    MRC     p15, 0, R0, c1, c0, 0
    BX      LR
    FUNC_END()
    
FUNC_DEF(armCp15AuxCtrlReg)
    MRC     p15, 0, R0, c1, c0, 1
    BX      LR
    FUNC_END()
    
FUNC_DEF(armCp15CACtrlReg)
    MRC     p15, 0, R0, c1, c0, 2
    BX      LR
    FUNC_END()
    
;/*********************************************************************************************************
;  设置总线模式
;*********************************************************************************************************/

FUNC_DEF(armFastBusMode)
    MRC     p15, 0, R0, c1, c0, 0
    BIC     R0, R0, #(P15_R1_iA | P15_R1_nF)
    MCR     p15, 0, R0, c1, c0, 0
    BX      LR
    FUNC_END()
    
FUNC_DEF(armAsyncBusMode)
    MRC     p15, 0, R0, c1, c0,0
    ORR     R0, R0, #(P15_R1_iA | P15_R1_nF)
    MCR     p15, 0, R0, c1, c0,0
    BX      LR
    FUNC_END()
    
FUNC_DEF(armSyncBusMode)
    MRC     p15, 0, R0, c1, c0,0
    BIC     R0, R0, #P15_R1_iA
    ORR     R0, R0, #P15_R1_nF
    MCR     p15, 0, R0, c1, c0,0
    BX      LR
    FUNC_END()
    
;/*********************************************************************************************************
;  ARM 进入节能状态，等待中断
;*********************************************************************************************************/

FUNC_DEF(armWaitForInterrupt)
    MOV     R0, #0
    MCR     p15, 0, R0, c7, c0, 4
    BX      LR
    FUNC_END()
    
;/*********************************************************************************************************
;  分支预测操作
;*********************************************************************************************************/
;/*********************************************************************************************************
;  This API invalidates the branch predictor
;*********************************************************************************************************/

FUNC_DEF(armBranchPredictorInvalidate)
    ARM_DSB()
    ARM_ISB()
    MOV     R0, #0
    MCR     p15, 0, R0, c7, c5, 6
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()

;/*********************************************************************************************************
;  This API invalidates the branch predictor array inner shareable
;*********************************************************************************************************/

FUNC_DEF(armBranchPredictorInvalidateInnerShareable)
    ARM_DSB()
    ARM_ISB()
    MOV     R0, #0
    MCR     p15, 0, R0, c7, c1, 6
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()

;/*********************************************************************************************************
;  This API enables the branch predictor
;*********************************************************************************************************/

FUNC_DEF(armBranchPredictionEnable)
    ARM_DSB()
    ARM_ISB()
    MRC     p15, 0, R0, c1, c0, 0
    ORR     R0, R0, #(1 << 11)
    MCR     p15, 0, R0, c1, c0, 0
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()

;/*********************************************************************************************************
;  This API disables the branch predictor
;*********************************************************************************************************/

FUNC_DEF(armBranchPredictionDisable)
    ARM_DSB()
    ARM_ISB()
    MRC     p15, 0, R0, c1, c0, 0
    BIC     R0, R0, #(1 << 11)
    MCR     p15, 0, R0, c1, c0, 0
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()
    
;/*********************************************************************************************************
;  向量地址操作
;*********************************************************************************************************/
;/*********************************************************************************************************
;  This API sets the interrupt vector table base address
;  R0 - Interrput Vector Base Address
;*********************************************************************************************************/

FUNC_DEF(armVectorBaseAddrSet)
    ARM_DSB()
    ARM_ISB()
    MCR     p15, 0, R0, c12, c0, 0
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()

FUNC_DEF(armHighVectorEnable)
    ARM_DSB()
    ARM_ISB()
    MRC     p15, 0, R0, c1, c0, 0
    ORR     R0, R0, #(1 << 13)
    MCR     p15, 0, R0, c1, c0, 0
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()

FUNC_DEF(armHighVectorDisable)
    ARM_DSB()
    ARM_ISB()
    MRC     p15, 0, R0, c1, c0, 0
    BIC     R0, R0, #(1 << 13)
    MCR     p15, 0, R0, c1, c0, 0
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()
    
;/*********************************************************************************************************
;  一些特性控制操作
;*********************************************************************************************************/
;/*********************************************************************************************************
;  This API Disables specified features in CP15 control register
;  R0 - features   Features to disable in Coprocessor 15 control register.
;                  'features' can take any OR a combination of the below values.
;                  CP15_CONTROL_TEXREMAP - TEX remap flag
;                  CP15_CONTROL_ACCESSFLAG - Access flag Control
;                  CP15_CONTROL_ALIGN_CHCK - Alignment Fault Checking
;                  CP15_CONTROL_MMU - To enable MMU
;
;  Note: Other fields of the CP15 c1 control register are not given here
;        as they are not of importance for StarterWare. However, optionally
;        they can also be ADDed.
;*********************************************************************************************************/

FUNC_DEF(armControlFeatureDisable)
    ARM_DSB()
    ARM_ISB()
    MRC     p15, 0, R1, c1, c0, 0
    BIC     R0, R1, R0
    MCR     p15, 0, R0, c1, c0, 0
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()

;/*********************************************************************************************************
;  This API Enables specified features in CP15 control register
;  R0 - features   Features to disable in Coprocessor 15 control register.
;                  'features' can take any OR a combination of the below values.
;                  CP15_CONTROL_TEXREMAP - TEX remap flag
;                  CP15_CONTROL_ACCESSFLAG - Access flag Control
;                  CP15_CONTROL_ALIGN_CHCK - Alignment Fault Checking
;                  CP15_CONTROL_MMU - To enable MMU
;
;  Note: Other fields of the CP15 c1 control register are not given here
;        as they are not of importance for StarterWare. However, optionally
;        they can also be ADDed.
;*********************************************************************************************************/

FUNC_DEF(armControlFeatureEnable)
    ARM_DSB()
    ARM_ISB()
    MRC     p15, 0, R1, c1, c0, 0
    ORR     R0, R1, R0
    MCR     p15, 0, R0, c1, c0, 0
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()

;/*********************************************************************************************************
;  This API Sets the specified fields in Auxiliary Control Register
;  R0 - Bit Mask for the bits to be set in Auxiliary Control Register
;*********************************************************************************************************/

FUNC_DEF(armAuxControlFeatureEnable)
    ARM_DSB()
    ARM_ISB()
    MRC     p15, 0, R1, c1, c0, 1
    ORR     R0,  R0, R1
    MCR     p15, 0, R0, c1, c0, 1
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()

;/*********************************************************************************************************
;  This API Clears the specified fields in Auxiliary Control Register
;  R0 - Bit Mask for the bits to be cleared in Auxiliary Control Register
;*********************************************************************************************************/

FUNC_DEF(armAuxControlFeatureDisable)
    ARM_DSB()
    ARM_ISB()
    MRC     p15, 0, R1, c1, c0, 1
    BIC     R0,  R1, R0
    MCR     p15, 0, R0, c1, c0, 1
    ARM_DSB()
    ARM_ISB()
    BX      LR
    FUNC_END()
    
    FILE_END()

#endif
;/*********************************************************************************************************
;  END
;*********************************************************************************************************/
