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
** 文   件   名: virPage.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2008 年 12 月 09 日
**
** 描        述: 虚拟内存管理.

** BUG:
2013.05.24  加入虚拟空间对齐开辟.
*********************************************************************************************************/

#ifndef __VIRPAGE_H
#define __VIRPAGE_H

ULONG           __vmmVirtualCreate(addr_t  ulAddr, size_t  stSize);
PLW_VMM_PAGE    __vmmVirtualPageAlloc(ULONG  ulPageNum);
PLW_VMM_PAGE    __vmmVirtualPageAllocAlign(ULONG  ulPageNum, size_t  stAlign);
VOID            __vmmVirtualPageFree(PLW_VMM_PAGE  pvmpage);
ULONG           __vmmVirtualPageGetMinContinue(VOID);

#endif                                                                  /*  __VIRPAGE_H                 */
/*********************************************************************************************************
  END
*********************************************************************************************************/
