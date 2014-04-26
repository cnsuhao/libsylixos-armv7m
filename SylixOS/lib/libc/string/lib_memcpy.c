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
** 文   件   名: lib_memcpy.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 13 日
**
** 描        述: 库
*********************************************************************************************************/
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
  按字对齐方式拷贝
*********************************************************************************************************/
#define __LONGSIZE              sizeof(ULONG)
#define __LONGMASK              (__LONGSIZE - 1)
#define __TLOOP(s)              if (ulTemp) {       \
                                    __TLOOP1(s);    \
                                }
#define __TLOOP1(s)             do {                \
                                    s;              \
                                } while (--ulTemp)
/*********************************************************************************************************
** 函数名称: lib_memcpy
** 功能描述: 
** 输　入  : 
** 输　出  : 
** 全局变量: 
** 调用模块: 
** 注  意  : 
*********************************************************************************************************/
PVOID  lib_memcpy (PVOID  pvDest, CPVOID   pvSrc, size_t  stCount)
{
    REGISTER PUCHAR    pucDest;
    REGISTER PUCHAR    pucSrc;
             ULONG     ulTemp;
    
    pucDest = (PUCHAR)pvDest;
    pucSrc  = (PUCHAR)pvSrc;
    
    if (stCount == 0 || pucDest == pucSrc) {
        return  (pvDest);
    }
    
    if (pucDest < pucSrc) {
        /*
         *  正常循序拷贝
         */
        ulTemp = (ULONG)pucSrc;
        if ((ulTemp | (ULONG)pucDest) & __LONGMASK) {
            /*
             *  拷贝非对齐部分
             */
            if (((ulTemp ^ (ULONG)pucDest) & __LONGMASK) || (stCount < __LONGSIZE)) {
                ulTemp = (ULONG)stCount;
            } else {
                ulTemp = (ULONG)(__LONGSIZE - (ulTemp & __LONGMASK));
            }
            stCount -= (UINT)ulTemp;
            __TLOOP1(*pucDest++ = *pucSrc++);
        }
        /*
         *  按字对齐拷贝
         */
        ulTemp = (ULONG)(stCount / __LONGSIZE);
        __TLOOP(*(ULONG *)pucDest = *(ULONG *)pucSrc; pucSrc += __LONGSIZE; pucDest += __LONGSIZE);
        ulTemp = (ULONG)(stCount & __LONGMASK);
        __TLOOP(*pucDest++ = *pucSrc++);
    
    } else {
        /*
         *  反向循序拷贝
         */
        pucSrc  += stCount;
        pucDest += stCount;
        
        ulTemp = (ULONG)pucSrc;
        if ((ulTemp | (ULONG)pucDest) & __LONGMASK) {
            /*
             *  拷贝非对齐部分
             */
            if (((ulTemp ^ (ULONG)pucDest) & __LONGMASK) || (stCount <= __LONGSIZE)) {
                ulTemp = (ULONG)stCount;
            } else {
                ulTemp &= __LONGMASK;
            }
            stCount -= (UINT)ulTemp;
            __TLOOP1(*--pucDest = *--pucSrc);
        }
        
        ulTemp = (ULONG)(stCount / __LONGSIZE);
        __TLOOP(pucSrc -= __LONGSIZE; pucDest -= __LONGSIZE; *(ULONG *)pucDest = *(ULONG *)pucSrc);
        ulTemp = (ULONG)(stCount & __LONGMASK);
        __TLOOP(*--pucDest = *--pucSrc);
    }
    
    return  (pvDest);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/
