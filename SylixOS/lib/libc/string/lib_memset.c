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
** 文   件   名: lib_memset.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 13 日
**
** 描        述: 库

** BUG:
2011.06.22  当 iCount 小于 0 时, 不处理.
2013.03.29  memset iC 先转换为 uchar 类型.
*********************************************************************************************************/
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  按字对齐方式拷贝
*********************************************************************************************************/
#define __LONGSIZE              sizeof(ULONG)
#define __LONGMASK              (__LONGSIZE - 1)
#define __TLOOP(s)              if (iTemp) {        \
                                    __TLOOP1(s);    \
                                }
#define __TLOOP1(s)             do {                \
                                    s;              \
                                } while (--iTemp)
/*********************************************************************************************************
** 函数名称: lib_memset
** 功能描述: 
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
PVOID  lib_memset (PVOID  pvDest, INT  iC, size_t  stCount)
{
    REGISTER INT       i;
    REGISTER INT       iTemp;
    REGISTER PUCHAR    pucDest = (PUCHAR)pvDest;
             UCHAR     ucC     = (UCHAR)iC;
             ULONG     ulFill  = (ULONG)ucC;
             
    if (stCount == 0) {
        return  (pvDest);
    }
             
    for (i = 1; i < (__LONGSIZE / sizeof(UCHAR)); i++) {                /*  构建 ulong 对齐的赋值变量   */
        ulFill = (ulFill << 8) + ucC;
    }

    if ((ULONG)pucDest & __LONGMASK) {                                  /*  处理前端非对齐部分          */
        if (stCount < __LONGSIZE) {
            iTemp = stCount;
        } else {
            iTemp = (INT)(__LONGSIZE - ((ULONG)pucDest & __LONGMASK));
        }
        stCount -= iTemp;
        __TLOOP1(*pucDest++ = ucC);
    }
    
    iTemp = (INT)(stCount / __LONGSIZE);
    __TLOOP(*(ULONG *)pucDest = ulFill; pucDest += __LONGSIZE);
    iTemp = (INT)(stCount & __LONGMASK);
    __TLOOP(*pucDest++ = ucC);
    
    return  (pvDest);
}
/*********************************************************************************************************
** 函数名称: lib_bzero
** 功能描述: 置字节字符串s的前n个字节为零。
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
VOID    lib_bzero (PVOID   pvStr, size_t  stCount)
{
    REGISTER INT       iTemp;
    REGISTER PUCHAR    pucDest = (PUCHAR)pvStr;
    
    if ((ULONG)pucDest & __LONGMASK) {                                  /*  处理前端非对齐部分          */
        if (stCount < __LONGSIZE) {
            iTemp = stCount;
        } else {
            iTemp = (INT)(__LONGSIZE - ((ULONG)pucDest & __LONGMASK));
        }
        stCount -= iTemp;
        __TLOOP1(*pucDest++ = 0);
    }
    
    iTemp = (INT)(stCount / __LONGSIZE);
    __TLOOP(*(ULONG *)pucDest = 0ul; pucDest += __LONGSIZE);
    iTemp = (INT)(stCount & __LONGMASK);
    __TLOOP(*pucDest++ = 0);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
