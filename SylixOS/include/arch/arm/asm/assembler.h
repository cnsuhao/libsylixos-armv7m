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
** 文   件   名: assembler.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 06 月 14 日
**
** 描        述: ARM 汇编相关.
*********************************************************************************************************/

#ifndef __ASMARM_ASSEMBLER_H
#define __ASMARM_ASSEMBLER_H

/*********************************************************************************************************
  arm architecture detect
*********************************************************************************************************/

#ifdef __GNUC__
#  if defined(__ARM_ARCH_2__)
#    define __SYLIXOS_ARM_ARCH__    2

#  elif defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
#    define __SYLIXOS_ARM_ARCH__    3

#  elif defined(__ARM_ARCH_4__) || defined(__ARM_ARCH_4T__)
#    define __SYLIXOS_ARM_ARCH__    4

#  elif defined(__ARM_ARCH_5__)  || defined(__ARM_ARCH_5E__) || \
        defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_5TE__) || \
        defined(__ARM_ARCH_5TEJ__) 
#    define __SYLIXOS_ARM_ARCH__    5

#  elif defined(__ARM_ARCH_6__)   || defined(__ARM_ARCH_6J__) || \
        defined(__ARM_ARCH_6K__)  || defined(__ARM_ARCH_6Z__) || \
        defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__)
#    define __SYLIXOS_ARM_ARCH__    6

#  elif defined(__ARM_ARCH_7__)  || defined(__ARM_ARCH_7A__) || \
        defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || \
        defined(__ARM_ARCH_7EM__)
#    define __SYLIXOS_ARM_ARCH__    7

#  elif defined(__ARM_ARCH_8__)  || defined(__ARM_ARCH_8A__)
#    define __SYLIXOS_ARM_ARCH__    8
#  endif                                                                /*  user define only            */

#else
#  define __SYLIXOS_ARM_ARCH__      4                                   /*  default ARMv4               */
#endif

/*********************************************************************************************************
  arm architecture assembly special code
*********************************************************************************************************/

#if defined(__ASSEMBLY__) || defined(ASSEMBLY)

/*********************************************************************************************************
  Data preload for architectures that support it
*********************************************************************************************************/

#if __SYLIXOS_ARM_ARCH__ >= 5
#  define PLD(code...)              code
#else
#  define PLD(code...)
#endif

/*********************************************************************************************************
  SMP
*********************************************************************************************************/

#if LW_CFG_SMP_EN > 0
#  define SMP(code...)              code
#  define UP(code...)
#else
#  define SMP(code...)
#  define UP(code...)               code
#endif

/*********************************************************************************************************
  assembler define
*********************************************************************************************************/

#ifdef __GNUC__
#  define EXPORT_LABEL(label)       .global label
#  define IMPORT_LABEL(label)       .extern label

#  define FUNC_LABEL(func)          func:
#  define LINE_LABEL(line)          line:

#  define FUNC_DEF(func)  \
        .code   32; \
        .balign 4;  \
        .type func, %function;  \
func:

#  define FUNC_END()    \
        .ltorg
        
#  define MACRO_DEF(mfunc...)   \
        .macro  mfunc
        
#  define MACRO_END()   \
        .endm

#  define FILE_BEGIN()  \
        .text;  \
        .balign 4;

#  define FILE_END()    \
        .end
        
#  define SECTION(sec)  \
        .section sec
		
#  define WEAK(sym)     \
        .weak sym

#else                                                                   /*  __GNUC__                    */
#  define EXPORT_LABEL(label)       EXPORT label
#  define IMPORT_LABEL(label)       IMPORT label

#  define FUNC_LABEL(func)          func
#  define LINE_LABEL(line)          line

#  define FUNC_DEF(func)  \
func

#  define FUNC_END()

#  define MACRO_DEF(mfunc...)   \
        MACRO;  \
        mfunc...
        
#  define MACRO_END()   \
        MEND

#  define FILE_BEGIN()  \
        CODE32; \
        PRESERVE8

#  define FILE_END()    \
        END

#  define SECTION(sec)  \
        AREA sec
        
#  define WEAK(sym)
#endif                                                                  /*  !__GNUC__                   */

/*********************************************************************************************************
  arm memory barrier
*********************************************************************************************************/

#if __SYLIXOS_ARM_ARCH__ >= 7
#  define ARM_ISB()     ISB
#  define ARM_DSB()     DSB
#  define ARM_DMB()     DMB

#elif __SYLIXOS_ARM_ARCH__ == 6
#  define ARM_ISB() \
        MOV     R0, #0; \
        MCR     p15, 0, R0, c7, c5, 4;

#  define ARM_DSB() \
        MOV     R0, #0; \
        MCR     p15, 0, R0, c7, c10, 4;
        
#  define ARM_DMB() \
        MOV     R0, #0; \
        MCR     p15, 0, R0, c7, c10, 5;
        
#else
#  define ARM_ISB()
#  define ARM_DSB()
#  define ARM_DMB()
#endif

/*********************************************************************************************************
  size define
*********************************************************************************************************/

#ifndef LW_CFG_KB_SIZE
#define LW_CFG_KB_SIZE  (1024)
#define LW_CFG_MB_SIZE  (1024 * LW_CFG_KB_SIZE)
#define LW_CFG_GB_SIZE  (1024 * LW_CFG_MB_SIZE)
#endif

/*********************************************************************************************************
  CPSR & SPSR bit
*********************************************************************************************************/

#  define USR32_MODE    0x10
#  define FIQ32_MODE    0x11
#  define IRQ32_MODE    0x12
#  define SVC32_MODE    0x13
#  define ABT32_MODE    0x17
#  define UND32_MODE    0x1B
#  define SYS32_MODE    0x1F
#  define DIS_FIQ       0x40
#  define DIS_IRQ       0x80
#  define DIS_INT       (DIS_FIQ | DIS_IRQ)

/*********************************************************************************************************
  CP15 bit
*********************************************************************************************************/

#  define P15_R1_I      (1 << 12)
#  define P15_R1_C      (1 <<  2)
#  define P15_R1_A      (1 <<  1)
#  define P15_R1_M      (1 <<  0)
#  define P15_R1_iA     (1 << 31)
#  define P15_R1_nF     (1 << 30)
#  define P15_R1_W      (1 <<  3)
#  define P15_R1_RR     (1 << 14)
#  define P15_R1_END    (1 <<  7)
#  define P15_R1_SYS    (1 <<  8)
#  define P15_R1_ROM    (1 <<  9)

/*********************************************************************************************************
  CP15 bit (armv7)
*********************************************************************************************************/

#  define P15_R1_SW     (1 << 10)
#  define P15_R1_Z      (1 << 11)
#  define P15_R1_V      (1 << 13)
#  define P15_R1_HA     (1 << 17)
#  define P15_R1_EE     (1 << 25)
#  define P15_R1_NMFI   (1 << 27)
#  define P15_R1_TRE    (1 << 28)
#  define P15_R1_AFE    (1 << 29)

#endif                                                                  /*  __ASSEMBLY__                */

#endif                                                                  /*  __ASMARM_ASSEMBLER_H        */
/*********************************************************************************************************
  END
*********************************************************************************************************/
