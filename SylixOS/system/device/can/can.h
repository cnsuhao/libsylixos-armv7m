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
** 文   件   名: can.h
**
** 创   建   人: Wang.Feng (王锋)
**
** 文件创建日期: 2010 年 02 月 01 日
**
** 描        述: CAN 设备库.

** BUG
2010.02.01  初始版本
2010.05.13  增加几种异常总线状态的宏定义
*********************************************************************************************************/

#ifndef __CAN_H
#define __CAN_H

/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_CAN_EN > 0)
/*********************************************************************************************************
  CAN 帧结构宏定义
*********************************************************************************************************/
#define   CAN_MAX_DATA                             8                    /*  CAN 帧数据最大长度          */
/*********************************************************************************************************
  定义驱动安装回调函数时的命令
*********************************************************************************************************/
#define   CAN_CALLBACK_GET_TX_DATA                 1                    /*  安装发送数据时的回调        */
#define   CAN_CALLBACK_PUT_RCV_DATA                2                    /*  安装接收数据时的回调        */
#define   CAN_CALLBACK_PUT_BUS_STATE               3                    /*  安装总线状态改变时的回调    */
/*********************************************************************************************************
  总线状态宏定义
*********************************************************************************************************/
#define   CAN_DEV_BUS_ERROR_NONE                   0X0000               /*  正常状态                    */
#define   CAN_DEV_BUS_OVERRUN                      0X0001               /*  接收溢出                    */
#define   CAN_DEV_BUS_OFF                          0X0002               /*  总线关闭                    */
#define   CAN_DEV_BUS_LIMIT                        0X0004               /*  限定警告                    */
#define   CAN_DEV_BUS_PASSIVE                      0x0008               /*  错误被动                    */
#define   CAN_DEV_BUS_RXBUFF_OVERRUN               0X0010               /*  接收缓冲溢出                */
/*********************************************************************************************************
  CAN 设备 ioctl 驱动应当实现命令定义
*********************************************************************************************************/
#define   CAN_DEV_OPEN                             201                  /*  CAN 设备打开命令            */
#define   CAN_DEV_CLOSE                            202                  /*  CAN 设备关闭命令            */
#define   CAN_DEV_GET_BUS_STATE                    203                  /*  获取 CAN 控制器状态         */
#define   CAN_DEV_REST_CONTROLLER                  205                  /*  复位 CAN 控制器             */
#define   CAN_DEV_SET_BAUD                         206                  /*  设置 CAN 波特率             */
#define   CAN_DEV_SET_FLITER                       207                  /*  设置 CAN 验收滤波器         */
#define   CAN_DEV_STARTUP                          208                  /*  启动 CAN 控制器             */
/*********************************************************************************************************
  注意: CAN 设备的 read() 与 write() 操作, 第三个参数与返回值为字节数, 不是 CAN 帧的个数.
        ioctl() FIONREAD 与 FIONWRITE 结果的单位都是字节.
  例如:
        CAN_FRAME   canframe[10];
        ssize_t     size;
        ssize_t     frame_num;
        ...
        size      = read(can_fd, canframe, 10 * sizeof(CAN_FRAME));
        frame_num = size / sizeof(CAN_FRAME);
        ...
        size      = write(can_fd, canframe, 10 * sizeof(CAN_FRAME));
        frame_num = size / sizeof(CAN_FRAME);
*********************************************************************************************************/
/*********************************************************************************************************
  CAN 帧结构定义
*********************************************************************************************************/
typedef struct {
    UINT            CAN_uiId;                                           /*  标识码                      */
    UINT            CAN_uiChannel;                                      /*  通道号                      */
    BOOL            CAN_bExtId;                                         /*  是否是扩展帧                */
    BOOL            CAN_bRtr;                                           /*  是否是远程帧                */
    UCHAR           CAN_ucLen;                                          /*  数据长度                    */
    UCHAR           CAN_ucData[CAN_MAX_DATA];                           /*  帧数据                      */
} CAN_FRAME;
typedef CAN_FRAME  *PCAN_FRAME;                                         /*  CAN帧指针类型               */
/*********************************************************************************************************
  CAN 驱动函数结构
*********************************************************************************************************/
#ifdef  __SYLIXOS_KERNEL

#ifdef  __cplusplus
typedef INT     (*CAN_CALLBACK)(...);
#else
typedef INT     (*CAN_CALLBACK)();
#endif

typedef struct __can_drv_funcs                       CAN_DRV_FUNCS;

typedef struct __can_chan {
    CAN_DRV_FUNCS    *pDrvFuncs;
} CAN_CHAN;                                                             /*  CAN 驱动结构体              */

struct __can_drv_funcs {
    INT               (*ioctl)
                      (
                      CAN_CHAN    *pcanchan,
                      INT          cmd,
                      PVOID        arg
                      );

    INT               (*txStartup)
                      (
                      CAN_CHAN    *pcanchan
                      );

    INT               (*callbackInstall)
                      (
                      CAN_CHAN          *pcanchan,
                      INT                callbackType,
                      CAN_CALLBACK       callback,
                      PVOID              callbackArg
                      );
};
/*********************************************************************************************************
  API
*********************************************************************************************************/

LW_API INT  API_CanDrvInstall(VOID);
LW_API INT  API_CanDevCreate(PCHAR     pcName,
                             CAN_CHAN *pcanchan,
                             UINT      uiRdFrameSize,
                             UINT      uiWrtFrameSize);
LW_API INT  API_CanDevRemove(PCHAR     pcName, BOOL  bForce);

#define canDrv          API_CanDrvInstall
#define canDevCreate    API_CanDevCreate
#define canDevRemove    API_CanDevRemove

#endif                                                                  /*  __SYLIXOS_KERNEL            */
#endif                                                                  /*  (LW_CFG_DEVICE_EN > 0)      */
                                                                        /*  (LW_CFG_CAN_EN > 0)         */
#endif                                                                  /*  __CAN_H                     */
/*********************************************************************************************************
  END
*********************************************************************************************************/
