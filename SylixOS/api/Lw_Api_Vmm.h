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
** 文   件   名: Lw_Api_Vmm.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 14 日
**
** 描        述: 这是系统提供给 C / C++ 用户的内核应用程序接口层。
                 为了适应不同语言习惯的人，这里使用了很多重复功能.
*********************************************************************************************************/

#ifndef __LW_API_VMM_H
#define __LW_API_VMM_H

/*********************************************************************************************************
    VMM API (应用程序尽量不要使用如下 API)
*********************************************************************************************************/

#define Lw_Vmm_Malloc               API_VmmMalloc
#define Lw_Vmm_MallocEx             API_VmmMallocEx
#define Lw_Vmm_MallocAlign          API_VmmMallocAlign
#define Lw_Vmm_Free                 API_VmmFree

#define Lw_Vmm_MallocArea           API_VmmMallocArea
#define Lw_Vmm_MallocAreaEx         API_VmmMallocAreaEx
#define Lw_Vmm_MallocAreaAlign      API_VmmMallocAreaAlign
#define Lw_Vmm_FreeArea             API_VmmFreeArea
#define Lw_Vmm_InvalidateArea       API_VmmInvalidateArea
#define Lw_Vmm_AbortStatus          API_VmmAbortStatus

#define Lw_Vmm_DmaAlloc             API_VmmDmaAlloc
#define Lw_Vmm_DmaAllocAlign        API_VmmDmaAllocAlign
#define Lw_Vmm_DmaFree              API_VmmDmaFree

#define Lw_Vmm_IoRemap              API_VmmIoRemap
#define Lw_Vmm_IoUnmap              API_VmmIoUnmap
#define Lw_Vmm_IoRemapNocache       API_VmmIoRemapNocache

#define Lw_Vmm_Map                  API_VmmMap
#define Lw_Vmm_VirtualToPhysical    API_VmmVirtualToPhysical
#define Lw_Vmm_PhysicalToVirtual    API_VmmPhysicalToVirtual

#define Lw_Vmm_ZoneStatus           API_VmmZoneStatus
#define Lw_Vmm_VirtualStatus        API_VmmVirtualStatus

#endif                                                                  /*  __LW_API_VMM_H              */
/*********************************************************************************************************
    OBJECT
*********************************************************************************************************/

