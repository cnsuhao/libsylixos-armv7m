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
** 文   件   名: Lw_Api_Cache.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 14 日
**
** 描        述: 这是系统提供给 C / C++ 用户的内核应用程序接口层。
                 为了适应不同语言习惯的人，这里使用了很多重复功能.
*********************************************************************************************************/

#ifndef __LW_API_CACHE_H
#define __LW_API_CACHE_H

/*********************************************************************************************************
    CACHE API
*********************************************************************************************************/

#define Lw_Cache_Location                   API_CacheLocation
#define Lw_Cache_Enable                     API_CacheEnable
#define Lw_Cache_Disable                    API_CacheDisable

#define Lw_Cache_Lock                       API_CacheLock
#define Lw_Cache_Unlock                     API_CacheUnlock

#define Lw_Cache_Flush                      API_CacheFlush
#define Lw_Cache_Invalidate                 API_CacheInvalidate
#define Lw_Cache_Clear                      API_CacheClear
#define Lw_Cache_CacheTextUpdate            API_CacheTextUpdate

#define Lw_Cache_DmaMalloc                  API_CacheDmaMalloc
#define Lw_Cache_DmaMallocAlgin             API_CacheDmaMallocAlign
#define Lw_Cache_DmaFree                    API_CacheDmaFree

#define Lw_Cache_DrvFlush                   API_CacheDrvFlush
#define Lw_Cache_DrvInvalidate              API_CacheDrvInvalidate

#define Lw_Cache_FuncsSet                   API_CacheFuncsSet

#endif                                                                  /*  __LW_API_CACHE_H            */
/*********************************************************************************************************
    OBJECT
*********************************************************************************************************/
