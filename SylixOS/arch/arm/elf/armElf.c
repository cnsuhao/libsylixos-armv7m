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
** 文   件   名: armElf.c
**
** 创   建   人: Jiang.Taijin (蒋太金)
**
** 文件创建日期: 2010 年 04 月 17 日
**
** 描        述: 实现 ARM 体系结构的 ELF 文件重定位.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
#ifdef   LW_CFG_CPU_ARCH_ARM                                            /*  ARM 体系结构                */
/*********************************************************************************************************
  裁剪支持
*********************************************************************************************************/
#if LW_CFG_MODULELOADER_EN > 0
#include "../SylixOS/loader/elf/elf_type.h"
#include "../SylixOS/loader/elf/elf_arch.h"
#include "../SylixOS/loader/include/loader_lib.h"
/*********************************************************************************************************
  宏定义
*********************************************************************************************************/
#define JMP_TABLE_ITEMLEN   8                                           /*  跳转表条目长度              */
/*********************************************************************************************************
  这里指令码以机器字长度的整数表示，不需要处理大小端问题，因为整数的高字节也是指令的高字节
*********************************************************************************************************/
#define LDR_INSTRUCTION     0xE51FF004                                  /*  跳转指令                    */
/*********************************************************************************************************
  跳转表项类型定义
*********************************************************************************************************/
typedef struct long_jmp_item {
    ULONG          ulJmpInstruction;                                    /*  跳转指令                    */
    Elf_Addr       addrJmpDest;                                         /*  目标地址                    */
} LONG_JMP_ITEM;
/*********************************************************************************************************
** 函数名称: jmpItemFind
** 功能描述: 查找跳转表项，如果没有，则新建一跳转表项
** 输　入  : addrSymVal    要查找的符号的值
**           pcBuffer      跳转表起始地址
**           stBuffLen     跳转表长度
** 输　出  : 返回跳转表项跳转指令的地址
** 全局变量:
** 调用模块:
*********************************************************************************************************/
static Elf_Addr jmpItemFind (Elf_Addr addrSymVal,
                             PCHAR    pcBuffer,
                             size_t   stBuffLen)
{
    LONG_JMP_ITEM *pJmpTable    = (LONG_JMP_ITEM *)pcBuffer;
    ULONG          ulJmpItemCnt = stBuffLen / 8;
    ULONG          i;

    for(i = 0; i < ulJmpItemCnt; i++) {
        if (pJmpTable[i].ulJmpInstruction != LDR_INSTRUCTION) {         /*  创建新表项                  */
            pJmpTable[i].ulJmpInstruction = LDR_INSTRUCTION;
            pJmpTable[i].addrJmpDest = addrSymVal;
            LD_DEBUG_MSG(("long jump item: %lx %lx\r\n",
                          (ULONG)&pJmpTable[i],
                          pJmpTable[i].addrJmpDest));
        }

        if (pJmpTable[i].addrJmpDest == addrSymVal) {
            break;
        }
    }

    if (i >= ulJmpItemCnt) {
        return  (0);
    }

    return  (Elf_Addr)(&pJmpTable[i]);
}
/*********************************************************************************************************
** 函数名称: archElfRelocateRela
** 功能描述: 重定位 RELA 类型的重定位项
** 输  入  : prela        RELA 表项
**           addrSymVal   重定位符号的值
**           pcTargetSec  重定位目目标节区
**           pcBuffer     跳转表起始地址
**           stBuffLen    跳转表长度
** 输  出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  archElfRelocateRela (Elf_Rela   *prela,
                          Elf_Addr    addrSymVal,
                          PCHAR       pcTargetSec,
                          PCHAR       pcBuffer,
                          size_t      stBuffLen)
{
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: archElfRelocateRel
** 功能描述: 重定位 REL 类型的重定位项
** 输  入  : prel         REL 表项
**           addrSymVal   重定位符号的值
**           pcTargetSec  重定位目目标节区
**           pcBuffer     跳转表起始地址
**           stBuffLen    跳转表长度
** 输  出  : ERROR_NONE 表示没有错误, PX_ERROR 表示错误
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  archElfRelocateRel (Elf_Rel     *prel,
                         Elf_Addr     addrSymVal,
                         PCHAR        pcTargetSec,
                         PCHAR        pcBuffer,
                         size_t       stBuffLen)
{
    Elf_Addr  *paddrWhere;
    Elf_Addr   addrTmp;
    Elf_Sword  swordAddend;
    Elf_Sword  swordTopBits;

    Elf_Addr  upper, lower, sign, j1, j2;

    paddrWhere = (Elf_Addr *)((size_t)pcTargetSec + prel->r_offset);    /*  计算重定位目标地址          */

    switch (ELF_R_TYPE(prel->r_info)) {

    case R_ARM_NONE:
        break;

    case R_ARM_TARGET1:
    case R_ARM_ABS32:                                                   /*  绝对地址重定位 （A+S）      */
        *paddrWhere += (Elf_Addr)addrSymVal;
        LD_DEBUG_MSG(("R_ARM_ABS32: %lx -> %lx\r\n", (ULONG)paddrWhere, *paddrWhere));
        break;

    case R_ARM_JUMP_SLOT:                                               /*  PLT                         */
    case R_ARM_GLOB_DAT:
        LD_DEBUG_MSG(("R_ARM_ABS32: %lx -> %lx\r\n", (ULONG)paddrWhere, addrSymVal));
        *paddrWhere = (Elf_Addr)addrSymVal;
        break;

    case R_ARM_RELATIVE:
        LD_DEBUG_MSG(("R_ARM_RELATIVE: %lx -> %lx\r\n", (ULONG)paddrWhere, addrSymVal));
        *paddrWhere += (Elf_Addr)pcTargetSec;
        break;

    case R_ARM_PC24:                                                    /*  相对地址重定位              */
    case R_ARM_PLT32:
    case R_ARM_CALL:
    case R_ARM_JUMP24:
        LD_DEBUG_MSG(("R_ARM_PC24: %lx -> %lx\r\n", (ULONG)paddrWhere, addrSymVal));
        swordAddend = *paddrWhere & 0x00ffffff;
        if (swordAddend & 0x00800000)
            swordAddend |= 0xff000000;
        addrTmp = addrSymVal - (Elf_Addr)paddrWhere + (swordAddend << 2);

        swordTopBits = addrTmp & 0xfe000000;
        if (swordTopBits != 0xfe000000 && swordTopBits != 0x00000000) {  /*  超出相对地址跳转范围       */
            addrSymVal = jmpItemFind(addrSymVal, pcBuffer, stBuffLen);
            if (0 == addrSymVal) {
                return  (PX_ERROR);
            }
            addrTmp = addrSymVal - (Elf_Addr)paddrWhere + (swordAddend << 2);
        }

        addrTmp >>= 2;
        *paddrWhere = (*paddrWhere & 0xff000000) | (addrTmp & 0x00ffffff);
        break;

    case R_ARM_REL32:
        *paddrWhere += (Elf_Addr)addrSymVal - (Elf_Addr)paddrWhere;     /*  prel->r_offset              */
        break;

    case R_ARM_V4BX:
        *paddrWhere &= 0xf000000f;
        *paddrWhere |= 0x01a0f000;
        break;

    case R_ARM_PREL31:
        *paddrWhere = (*paddrWhere) + addrSymVal - (Elf_Addr)paddrWhere;
        *paddrWhere &= 0x7fffffff;
        break;

    case R_ARM_MOVW_ABS_NC:
    case R_ARM_MOVT_ABS:
        addrTmp = *paddrWhere;
        addrTmp = ((addrTmp & 0xf0000) >> 4) | (addrTmp & 0xfff);
        addrTmp = (addrTmp ^ 0x8000) - 0x8000;
        addrTmp += addrSymVal;
        if (ELF32_R_TYPE(prel->r_info) == R_ARM_MOVT_ABS) {
            addrTmp >>= 16;
        }

        *paddrWhere &= 0xfff0f000;
        *paddrWhere |= ((addrTmp & 0xf000) << 4) |
                       (addrTmp & 0x0fff);
        break;

    case R_ARM_THM_PC22:
    case R_ARM_THM_JUMP24:
        upper = *(UINT16 *)paddrWhere;
        lower = *(UINT16 *)(paddrWhere + 2);

        /*
         *  25 bit signed address range (Thumb-2 BL and B.W
         *  instructions):
         *   S:I1:I2:imm10:imm11:0
         *  where:
         *   S     = upper[10]   = offset[24]
         *   I1    = ~(J1 ^ S)   = offset[23]
         *   I2    = ~(J2 ^ S)   = offset[22]
         *   imm10 = upper[9:0]  = offset[21:12]
         *   imm11 = lower[10:0] = offset[11:1]
         *   J1    = lower[13]
         *   J2    = lower[11]
         */
        sign = (upper >> 10) & 1;
        j1 = (lower >> 13) & 1;
        j2 = (lower >> 11) & 1;
        addrTmp = (sign << 24) | ((~(j1 ^ sign) & 1) << 23) |
        ((~(j2 ^ sign) & 1) << 22) |
        ((upper & 0x03ff) << 12) |
        ((lower & 0x07ff) << 1);
        if (addrTmp & 0x01000000) {
            addrTmp -= 0x02000000;
        }
        addrTmp += addrSymVal - (Elf_Addr)paddrWhere;

        /* 
         *  only Thumb addresses allowed (no interworking)
         */
        if (!(addrTmp & 1) ||
            addrTmp <= (UINT32)0xff000000 ||
            addrTmp >= (UINT32)0x01000000) {
            return (PX_ERROR);
        }

        sign = (addrTmp >> 24) & 1;
        j1 = sign ^ (~(addrTmp >> 23) & 1);
        j2 = sign ^ (~(addrTmp >> 22) & 1);
        *(UINT16 *)paddrWhere = (UINT16)((upper & 0xf800) | (sign << 10) |
        ((addrTmp >> 12) & 0x03ff));
        *(UINT16 *)(paddrWhere + 2) = (UINT16)((lower & 0xd000) |
        (j1 << 13) | (j2 << 11) |
        ((addrTmp >> 1) & 0x07ff));
        upper = *(UINT16 *)paddrWhere;
        lower = *(UINT16 *)(paddrWhere + 2);
        break;


    default:
        _DebugFormat(__ERRORMESSAGE_LEVEL, "unknown relocate type %d.\r\n", ELF_R_TYPE(prel->r_info));
        return  (PX_ERROR);
    }

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: archElfRGetJmpBuffItemLen
** 功能描述: 返回跳转表项长度
** 输  入  :
** 输  出  : 跳转表项长度
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  archElfRGetJmpBuffItemLen (VOID)
{
    return  (JMP_TABLE_ITEMLEN);
}

#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
#endif                                                                  /*  LW_CFG_CPU_ARCH_ARM         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
