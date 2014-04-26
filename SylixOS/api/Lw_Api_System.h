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
** 文   件   名: Lw_Api_System.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 14 日
**
** 描        述: 这是系统提供给 C / C++ 用户的系统应用程序接口层。
                 为了适应不同语言习惯的人，这里使用了很多重复功能.
*********************************************************************************************************/

#ifndef __LW_API_SYSTEM_H
#define __LW_API_SYSTEM_H

/*********************************************************************************************************
  设备驱动程序部分
*********************************************************************************************************/

#define Lw_Ios_DrvInstall                        API_IosDrvInstall
#define Lw_Ios_DrvInstallEx                      API_IosDrvInstallEx
#define Lw_Ios_DrvRemove                         API_IosDrvRemove

#define Lw_Ios_DevAdd                            API_IosDevAdd
#define Lw_Ios_DevDelete                         API_IosDevDelete

#define Lw_Ios_DevFind                           API_IosDevFind
#define Lw_Ios_DevMatch                          API_IosDevMatch
#define Lw_Ios_NextDevGet                        API_IosNextDevGet

/*********************************************************************************************************
  IO API
*********************************************************************************************************/

#define Lw_Io_PrivateEnv                         API_IoPrivateEnv

#define Lw_Io_DefPathCat                         API_IoDefPathCat
#define Lw_Io_DefDevGet                          API_IoDefDevGet
#define Lw_Io_DefDirGet                          API_IoDefDirGet

#define Lw_Io_GlobalStdSet                       API_IoGlobalStdSet
#define Lw_Io_GlobalStdGet                       API_IoGlobalStdGet

#define Lw_Io_TaskStdSet                         API_IoTaskStdSet
#define Lw_Io_TaskStdGet                         API_IoTaskStdGet

/*********************************************************************************************************
  FILE API
*********************************************************************************************************/

#define Lw_File_FullNameGet                      API_IoFullFileNameGet
#define Lw_File_PathCondense                     API_IoPathCondense

#define Lw_File_DefPathSet                       API_IoDefPathSet
#define Lw_File_DefPathGet                       API_IoDefPathGet

/*********************************************************************************************************
  VxWorks SHOW
*********************************************************************************************************/

#define Lw_Io_DrvShow                            API_IoDrvShow
#define Lw_Io_DevShow                            API_IoDevShow
#define Lw_Io_FdShow                             API_IoFdShow
#define Lw_Io_FdentryShow                        API_IoFdentryShow
#define Lw_Io_DrvLicenseShow                     API_IoDrvLicenseShow

/*********************************************************************************************************
  Linux2.6 Driver License
*********************************************************************************************************/

#define Lw_Io_GetDrvLicense                      API_IoGetDrvLicense
#define Lw_Io_GetDrvAuthor                       API_IoGetDrvAuthor
#define Lw_Io_GetDrvDescription                  API_IoGetDrvDescription

/*********************************************************************************************************
  system device API
*********************************************************************************************************/

#define Lw_Tty_DrvInstall                        API_TtyDrvInstall
#define Lw_Tty_DevCreate                         API_TtyDevCreate

#define Lw_Ty_AbortFuncSet                       API_TyAbortFuncSet
#define Lw_Ty_AbortSet                           API_TyAbortSet
#define Lw_Ty_BackspaceSet                       API_TyBackspaceSet
#define Lw_Ty_DeleteLineSet                      API_TyDeleteLineSet
#define Lw_Ty_EOFSet                             API_TyEOFSet
#define Lw_Ty_MonitorTrapSet                     API_TyMonitorTrapSet

#define Lw_Pty_DrvInstall                        API_PtyDrvInstall
#define Lw_Pty_DevCreate                         API_PtyDevCreate
#define Lw_Pty_DevRemove                         API_PtyDevRemove

#define Lw_Spipe_DrvInstall                      API_SpipeDrvInstall
#define Lw_Spipe_DevCreate                       API_SpipeDevCreate
#define Lw_Spipe_DevDelete                       API_SpipeDevDelete

#define Lw_Pipe_DrvInstall                       API_PipeDrvInstall
#define Lw_Pipe_DevCreate                        API_PipeDevCreate
#define Lw_Pipe_DevDelete                        API_PipeDevDelete

#define Lw_RamDisk_Create                        API_RamDiskCreate
#define Lw_RamDisk_Delete                        API_RamDiskDelete

/*********************************************************************************************************
  graph device interface
*********************************************************************************************************/

#define Lw_GMem_DevAdd                           API_GMemDevAdd
#define Lw_GMem_Get2D                            API_GMemGet2D
#define Lw_GMem_SetPalette                       API_GMemSetPalette
#define Lw_GMem_GetPalette                       API_GMemGetPalette
#define Lw_GMem_SetPixel                         API_GMemSetPixel
#define Lw_GMem_GetPixel                         API_GMemGetPixel
#define Lw_GMem_SetColor                         API_GMemSetColor
#define Lw_GMem_SetAlpha                         API_GMemSetAlpha
#define Lw_GMem_DrawHLine                        API_GMemDrawHLine
#define Lw_GMem_DrawVLine                        API_GMemDrawVLine
#define Lw_GMem_FillRect                         API_GMemFillRect

/*********************************************************************************************************
  THREAD POOL
*********************************************************************************************************/

#define Lw_ThreadPool_Create                     API_ThreadPoolCreate
#define Lw_ThreadPool_Delete                     API_ThreadPoolDelete

#define Lw_ThreadPool_AddThread                  API_ThreadPoolAddThread
#define Lw_ThreadPool_DeleteThread               API_ThreadPoolDeleteThread

#define Lw_ThreadPool_SetAttr                    API_ThreadPoolSetAttr
#define Lw_ThreadPool_GetAttr                    API_ThreadPoolGetAttr

#define Lw_ThreadPool_Status                     API_ThreadPoolStatus
#define Lw_ThreadPool_Info                       API_ThreadPoolStatus

/*********************************************************************************************************
  POWER MANAGEMENT
*********************************************************************************************************/

#define Lw_PowerM_Create                         API_PowerMCreate
#define Lw_PowerM_Delete                         API_PowerMDelete

#define Lw_PowerM_Start                          API_PowerMStart
#define Lw_PowerM_Cancel                         API_PowerMCancel

#define Lw_PowerM_Connect                        API_PowerMConnect
#define Lw_PowerM_Signal                         API_PowerMSignal
#define Lw_PowerM_SignalFast                     API_PowerMSignalFast

#define Lw_PowerM_Status                         API_PowerMStatus
#define Lw_PowerM_Show                           API_PowerMShow

/*********************************************************************************************************
  HWRTC system
*********************************************************************************************************/

#define Lw_Rtc_DrvInstall                        API_RtcDrvInstall
#define Lw_Rtc_DevCreate                         API_RtcDevCreate
#define Lw_Rtc_Set                               API_RtcSet
#define Lw_Rtc_Get                               API_RtcGet
#define Lw_Rtc_SysToRtc                          API_SysToRtc
#define Lw_Rtc_RtcToSys                          API_RtcToSys
#define Lw_Rtc_RtcToRoot                         API_RtcToRoot

/*********************************************************************************************************
  Log system
*********************************************************************************************************/

#define Lw_Log_FdSet                             API_LogFdSet
#define Lw_Log_FdGet                             API_LogFdGet
#define Lw_Log_Printk                            API_LogPrintk
#define Lw_Log_Msg                               API_LogMsg

/*********************************************************************************************************
  BUS
*********************************************************************************************************/

#define Lw_Bus_Show                              API_BusShow

/*********************************************************************************************************
  hotplug
*********************************************************************************************************/

#define Lw_Hotplug_GetLost                       API_HotplugGetLost
#define Lw_Hotplug_Context                       API_HotplugContext

#endif                                                                  /*  __LW_API_SYSTEM_H           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
