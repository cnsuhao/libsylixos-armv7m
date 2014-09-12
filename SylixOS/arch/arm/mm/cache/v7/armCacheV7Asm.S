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
;** 文   件   名: armCacheV7Asm.S
;**
;** 创   建   人: Han.Hui (韩辉)
;**
;** 文件创建日期: 2013 年 12 月 09 日
;**
;** 描        述: ARMv7 体系构架 CACHE 驱动.
;*********************************************************************************************************/

#ifndef ASSEMBLY
#define ASSEMBLY 1
#endif

#include <arch/assembler.h>
#include <config/kernel/cache_cfg.h>

#if LW_CFG_CACHE_EN > 0

    FILE_BEGIN()
    
    EXPORT_LABEL(armDCacheV7Disable)
    EXPORT_LABEL(armDCacheV7FlushAll)
    EXPORT_LABEL(armDCacheV7ClearAll)
    
;/*********************************************************************************************************
;  ARMv7 关闭 DCACHE
;*********************************************************************************************************/

FUNC_DEF(armDCacheV7Disable)
    MRC     p15, 0, R0, c1, c0, 0
    AND     R0, R0, #P15_R1_C
    CMP     R0, #0
    BEQ     dcache_not_en

    STMFD   SP!, {LR}
    BL      armDCacheV7ClearAll
    LDMFD   SP!, {LR}
    
    ARM_DSB()
    ARM_ISB()
    
    MRC     p15, 0, R0, c1, c0, 0
    BIC     R0, R0, #P15_R1_C
    MCR     p15, 0, R0, c1, c0, 0
    BX      LR
	
LINE_LABEL(dcache_not_en)
    STMFD   SP!, {LR}
    BL      armDCacheV7InvalidateAll
    LDMFD   SP!, {LR}
    BX      LR
    FUNC_END()
    
;/*********************************************************************************************************
;  ARMv7 回写 DCACHE
;*********************************************************************************************************/

#define _FLD_MAX_WAY    0x3ff
#define _FLD_MAX_IDX    0x7fff

FUNC_DEF(armDCacheV7FlushAll)
    STMFD   SP!, {R4-R11}
    ARM_DMB()
    MRC     p15, 1, R0, c0, c0, 1                                       ;/* Read CLID register           */
    ANDS    R3, R0, #0x7000000                                          ;/* Get Level of Coherency       */
    MOV     R3, R3, LSR #23
    BEQ     ffinished
    MOV     R10, #0

LINE_LABEL(floop1)
    ADD     R2, R10, R10, LSR #1
    MOV     R1, R0, LSR R2
    AND     R1, R1, #7
    CMP     R1, #2
    BLT     fskip
    MCR     p15, 2, R10, c0, c0, 0
    ARM_ISB()
    MRC     p15, 1, R1, c0, c0, 0
    AND     R2, R1, #7
    ADD     R2, R2, #4
    LDR     R4, =_FLD_MAX_WAY
    ANDS    R4, R4, R1, LSR #3
#if __SYLIXOS_ARM_ARCH__ >= 5
    CLZ     R5, R4
#else
    .long   0xe16f5f14
#endif
    LDR     R7, =_FLD_MAX_IDX
    ANDS    R7, R7, R1, LSR #13

LINE_LABEL(floop2)
    MOV     R9, R4

LINE_LABEL(floop3)
    ORR     R11, R10, R9, LSL R5
    ORR     R11, R11, R7, LSL R2
    MCR     p15, 0, R11, c7, c10, 2
    SUBS    R9, R9, #1
    BGE     floop3
    SUBS    R7, R7, #1
    BGE     floop2

LINE_LABEL(fskip)
    ADD     R10, R10, #2
    CMP     R3, R10
    BGT     floop1

LINE_LABEL(ffinished)
    ARM_DSB()
    ARM_ISB()
    LDMFD   SP!, {R4-R11}
    BX      LR
    FUNC_END()
    
;/*********************************************************************************************************
;  ARMv7 回写并无效 DCACHE
;*********************************************************************************************************/

FUNC_DEF(armDCacheV7ClearAll)
    STMFD   SP!, {R4-R11}
    ARM_DMB()
    MRC     p15, 1, R0, c0, c0, 1                                       ;/* Read CLID register           */
    ANDS    R3, R0, #0x7000000                                          ;/* Get Level of Coherency       */
    MOV     R3, R3, LSR #23
    BEQ     cfinished
    MOV     R10, #0

LINE_LABEL(cloop1)
    ADD     R2, R10, R10, LSR #1
    MOV     R1, R0, LSR R2
    AND     R1, R1, #7
    CMP     R1, #2
    BLT     cskip
    MCR     p15, 2, R10, c0, c0, 0
    ARM_ISB()
    MRC     p15, 1, R1, c0, c0, 0
    AND     R2, R1, #7
    ADD     R2, R2, #4
    LDR     R4, =_FLD_MAX_WAY
    ANDS    R4, R4, R1, LSR #3
#if __SYLIXOS_ARM_ARCH__ >= 5
    CLZ     R5, R4
#else
    .long   0xe16f5f14
#endif
    LDR     R7, =_FLD_MAX_IDX
    ANDS    R7, R7, R1, LSR #13

LINE_LABEL(cloop2)
    MOV     R9, R4

LINE_LABEL(cloop3)
    ORR     R11, R10, R9, LSL R5
    ORR     R11, R11, R7, LSL R2
    MCR     p15, 0, R11, c7, c14, 2
    SUBS    R9, R9, #1
    BGE     cloop3
    SUBS    R7, R7, #1
    BGE     cloop2

LINE_LABEL(cskip)
    ADD     R10, R10, #2
    CMP     R3, R10
    BGT     cloop1

LINE_LABEL(cfinished)
    ARM_DSB()
    ARM_ISB()
    LDMFD   SP!, {R4-R11}
    BX      LR
    FUNC_END()

;/*********************************************************************************************************
;  ARMv7 无效 DCACHE
;*********************************************************************************************************/

FUNC_DEF(armDCacheV7InvalidateAll)
    STMFD   SP!, {R4-R11}
    ARM_DMB()
    MRC     p15, 1, R0, c0, c0, 1                                       ;/* Read CLID register           */
    ANDS    R3, R0, #0x7000000                                          ;/* Get Level of Coherency       */
    MOV     R3, R3, LSR #23
    BEQ     ifinished
    MOV     R10, #0

LINE_LABEL(iloop1)
    ADD     R2, R10, R10, LSR #1
    MOV     R1, R0, LSR R2
    AND     R1, R1, #7
    CMP     R1, #2
    BLT     iskip
    MCR     p15, 2, R10, c0, c0, 0
    ARM_ISB()
    MRC     p15, 1, R1, c0, c0, 0
    AND     R2, R1, #7
    ADD     R2, R2, #4
    LDR     R4, =_FLD_MAX_WAY
    ANDS    R4, R4, R1, LSR #3
#if __SYLIXOS_ARM_ARCH__ >= 5
    CLZ     R5, R4
#else
    .long   0xe16f5f14
#endif
    LDR     R7, =_FLD_MAX_IDX
    ANDS    R7, R7, R1, LSR #13

LINE_LABEL(iloop2)
    MOV     R9, R4

LINE_LABEL(iloop3)
    ORR     R11, R10, R9, LSL R5
    ORR     R11, R11, R7, LSL R2
    MCR     p15, 0, R11, c7, c6, 2
    SUBS    R9, R9, #1
    BGE     iloop3
    SUBS    R7, R7, #1
    BGE     iloop2

LINE_LABEL(iskip)
    ADD     R10, R10, #2
    CMP     R3, R10
    BGT     iloop1

LINE_LABEL(ifinished)
    ARM_DSB()
    ARM_ISB()
    LDMFD   SP!, {R4-R11}
    BX      LR
    FUNC_END()

    FILE_END()

#endif
;/*********************************************************************************************************
;  END
;*********************************************************************************************************/
