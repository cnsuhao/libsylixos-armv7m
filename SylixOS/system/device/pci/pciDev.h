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
** 文   件   名: pciDev.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 09 月 28 日
**
** 描        述: PCI 总线驱动模型, 设备头定义.
*********************************************************************************************************/

#ifndef __PCI_DEV_H
#define __PCI_DEV_H

#include "pciBus.h"
#include "pciVendor.h"
#include "pciExpress.h"

/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_PCI_EN > 0)

/*********************************************************************************************************
   PCI 硬件连接结构例图
   
                      +-------+
                      |       |
                      |  CPU  |
                      |       |
                      +-------+
                          |
       Host bus           |
    ----------------------+-------------------------- ...
                          |
                 +------------------+
                 |     Bridge 0     |
                 |  (host adapter)  |
                 +------------------+
                          |
       PCI bus segment 0  |  (primary bus segment)
    ----------------------+-------------------------- ...
             |            |    |                  |
           dev 0          |   dev 1              dev 2
                          |
                 +------------------+
                 |     Bridge 1     |
                 |   (PCI to PCI)   |
                 +------------------+
                          |
       PCI bus segment 1  |  (secondary bus segment)
    ----------------------+-------------------------- ...
             |            |    |                  |
           dev 0          |   dev 1              dev 2
                          |
                 +------------------+
                 |     Bridge 2     |
                 |   (PCI to PCI)   |
                 +------------------+
                          |
       PCI bus segment 2  |  (tertiary bus segment)
    ----------------------+-------------------------- ...
             |                 |                  |
           dev 0              dev 1              dev 2
*********************************************************************************************************/
/*********************************************************************************************************
  PCI 设备头
*********************************************************************************************************/

typedef struct {
    UINT16          PCID_usVendorId;                                    /* vendor ID                    */
    UINT16          PCID_usDeviceId;                                    /* device ID                    */
    UINT16          PCID_usCommand;                                     /* command register             */
    UINT16          PCID_usStatus;                                      /* status register              */
    
    UINT8           PCID_ucRevisionId;                                  /* revision ID                  */
    UINT8           PCID_ucProgIf;                                      /* programming interface        */
    UINT8           PCID_ucSubClass;                                    /* sub class code               */
    UINT8           PCID_ucClassCode;                                   /* class code                   */
    
    UINT8           PCID_ucCacheLine;                                   /* cache line                   */
    UINT8           PCID_ucLatency;                                     /* latency time                 */
    UINT8           PCID_ucHeaderType;                                  /* header type                  */
    UINT8           PCID_ucBist;                                        /* BIST                         */
    
    UINT32          PCID_uiBase0;                                       /* base address 0               */
    UINT32          PCID_uiBase1;                                       /* base address 1               */
    UINT32          PCID_uiBase2;                                       /* base address 2               */
    UINT32          PCID_uiBase3;                                       /* base address 3               */
    UINT32          PCID_uiBase4;                                       /* base address 4               */
    UINT32          PCID_uiBase5;                                       /* base address 5               */
    
    UINT32          PCID_uiCis;                                         /* cardBus CIS pointer          */
    UINT16          PCID_usSubVendorId;                                 /* sub system vendor ID         */
    UINT16          PCID_usSubSystemId;                                 /* sub system ID                */
    UINT32          PCID_uiRomBase;                                     /* expansion ROM base address   */

    UINT32          PCID_uiReserved0;                                   /* reserved                     */
    UINT32          PCID_uiReserved1;                                   /* reserved                     */

    UINT8           PCID_ucIntLine;                                     /* interrupt line               */
    UINT8           PCID_ucIntPin;                                      /* interrupt pin                */
    
    UINT8           PCID_ucMinGrant;                                    /* min Grant                    */
    UINT8           PCID_ucMaxLatency;                                  /* max Latency                  */
} PCI_DEV_HDR;

/*********************************************************************************************************
  PCI 桥设备头
*********************************************************************************************************/

typedef struct {
    UINT16          PCIB_usVendorId;                                    /* vendor ID                    */
    UINT16          PCIB_usDeviceId;                                    /* device ID                    */
    UINT16          PCIB_usCommand;                                     /* command register             */
    UINT16          PCIB_usStatus;                                      /* status register              */

    UINT8           PCIB_ucRevisionId;                                  /* revision ID                  */
    UINT8           PCIB_ucProgIf;                                      /* programming interface        */
    UINT8           PCIB_ucSubClass;                                    /* sub class code               */
    UINT8           PCIB_ucClassCode;                                   /* class code                   */
    
    UINT8           PCIB_ucCacheLine;                                   /* cache line                   */
    UINT8           PCIB_ucLatency;                                     /* latency time                 */
    UINT8           PCIB_ucHeaderType;                                  /* header type                  */
    UINT8           PCIB_ucBist;                                        /* BIST                         */

    UINT32          PCIB_uiBase0;                                       /* base address 0               */
    UINT32          PCIB_uiBase1;                                       /* base address 1               */

    UINT8           PCIB_ucPriBus;                                      /* primary bus number           */
    UINT8           PCIB_ucSecBus;                                      /* secondary bus number         */
    UINT8           PCIB_ucSubBus;                                      /* subordinate bus number       */
    UINT8           PCIB_ucSecLatency;                                  /* secondary latency timer      */
    UINT8           PCIB_ucIoBase;                                      /* IO base                      */
    UINT8           PCIB_ucIoLimit;                                     /* IO limit                     */

    UINT16          PCIB_usSecStatus;                                   /* secondary status             */

    UINT16          PCIB_usMemBase;                                     /* memory base                  */
    UINT16          PCIB_usMemLimit;                                    /* memory limit                 */
    UINT16          PCIB_usPreBase;                                     /* prefetchable memory base     */
    UINT16          PCIB_usPreLimit;                                    /* prefetchable memory limit    */

    UINT32          PCIB_uiPreBaseUpper;                                /* prefetchable memory base     */
                                                                        /* upper 32 bits                */
    UINT32          PCIB_uiPreLimitUpper;                               /* prefetchable memory limit    */
                                                                        /* upper 32 bits                */

    UINT16          PCIB_usIoBaseUpper;                                 /* IO base upper 16 bits        */
    UINT16          PCIB_usIoLimitUpper;                                /* IO limit upper 16 bits       */

    UINT32          PCIB_uiReserved;                                    /* reserved                     */
    UINT32          PCIB_uiRomBase;                                     /* expansion ROM base address   */

    UINT8           PCIB_ucIntLine;                                     /* interrupt line               */
    UINT8           PCIB_ucIntPin;                                      /* interrupt pin                */

    UINT16          PCIB_usControl;                                     /* bridge control               */
} PCI_BRG_HDR;

/*********************************************************************************************************
  PCI 卡桥设备头
*********************************************************************************************************/

typedef struct {
    UINT16          PCICB_usVendorId;                                   /* vendor ID                    */
    UINT16          PCICB_usDeviceId;                                   /* device ID                    */
    UINT16          PCICB_usCommand;                                    /* command register             */
    UINT16          PCICB_usStatus;                                     /* status register              */

    UINT8           PCICB_ucRevisionId;                                 /* revision ID                  */
    UINT8           PCICB_ucProgIf;                                     /* programming interface        */
    UINT8           PCICB_ucSubClass;                                   /* sub class code               */
    UINT8           PCICB_ucClassCode;                                  /* class code                   */
    
    UINT8           PCICB_ucCacheLine;                                  /* cache line                   */
    UINT8           PCICB_ucLatency;                                    /* latency time                 */
    UINT8           PCICB_ucHeaderType;                                 /* header type                  */
    UINT8           PCICB_ucBist;                                       /* BIST                         */
    
    UINT32          PCICB_uiBase0;                                      /* base address 0               */

    UINT8           PCICB_ucCapPtr;                                     /* capabilities pointer         */
    UINT8           PCICB_ucReserved;                                   /* reserved                     */

    UINT16          PCICB_usSecStatus;                                  /* secondary status             */

    UINT8           PCICB_ucPriBus;                                     /* primary bus number           */
    UINT8           PCICB_ucSecBus;                                     /* secondary bus number         */
    UINT8           PCICB_ucSubBus;                                     /* subordinate bus number       */
    UINT8           PCICB_ucSecLatency;                                 /* secondary latency timer      */

    UINT32          PCICB_uiMemBase0;                                   /* memory base 0                */
    UINT32          PCICB_uiMemLimit0;                                  /* memory limit 0               */
    UINT32          PCICB_uiMemBase1;                                   /* memory base 1                */
    UINT32          PCICB_uiMemLimit1;                                  /* memory limit 1               */

    UINT32          PCICB_uiIoBase0;                                    /* IO base 0                    */
    UINT32          PCICB_uiIoLimit0;                                   /* IO limit 0                   */
    UINT32          PCICB_uiIoBase1;                                    /* IO base 1                    */
    UINT32          PCICB_uiIoLimit1;                                   /* IO limit 1                   */

    UINT8           PCICB_ucIntLine;                                    /* interrupt line               */
    UINT8           PCICB_ucIntPin;                                     /* interrupt pin                */

    UINT16          PCICB_usControl;                                    /* bridge control               */
    UINT16          PCICB_usSubVendorId;                                /* sub system vendor ID         */
    UINT16          PCICB_usSubSystemId;                                /* sub system ID                */

    UINT32          PCICB_uiLegacyBase;                                 /* pccard 16bit legacy mode base*/
} PCI_CBUS_HDR;

/*********************************************************************************************************
  PCI 标准设备头
*********************************************************************************************************/

typedef struct {
    UINT8               PCIH_ucType;                                    /*  PCI 类型                    */
    /*
     * PCI_HEADER_TYPE_NORMAL
     * PCI_HEADER_TYPE_BRIDGE
     * PCI_HEADER_TYPE_CARDBUS
     */
    union {
        PCI_DEV_HDR     PCIHH_pcidHdr;
        PCI_BRG_HDR     PCIHH_pcibHdr;
        PCI_CBUS_HDR    PCIHH_pcicbHdr;
    } hdr;
#define PCIH_pcidHdr    hdr.PCIHH_pcidHdr
#define PCIH_pcibHdr    hdr.PCIHH_pcibHdr
#define PCIH_pcicbHdr   hdr.PCIHH_pcicbHdr
} PCI_HDR;

/*********************************************************************************************************
  PCI I/O Drv
*********************************************************************************************************/

typedef struct pci_drv_funcs {
    UINT8   (*ioInByte)(addr_t ulAddr);
    UINT16  (*ioInWord)(addr_t ulAddr);
    UINT32  (*ioInDword)(addr_t ulAddr);
    VOID    (*ioOutByte)(addr_t ulAddr, UINT8 ucValue);
    VOID    (*ioOutWord)(addr_t ulAddr, UINT16 usValue);
    VOID    (*ioOutDword)(addr_t ulAddr, UINT32 uiValue);
} PCI_DRV_FUNCS;

/*********************************************************************************************************
  The PCI interface treats multi-function devices as independent
  devices. The slot/function address of each device is encoded
  in a single byte as follows:
  7:3 = slot
  2:0 = function
*********************************************************************************************************/

#define PCI_SLOTFN(slot, func)  ((((slot) & 0x1f) << 3) | ((func) & 0x07))
#define PCI_SLOT(slotfn)        (((slotfn) >> 3) & 0x1f)
#define PCI_FUNC(slotfn)        ((slotfn) & 0x07)

/*********************************************************************************************************
  pack parameters for the PCI Configuration Address Register
*********************************************************************************************************/

#define PCI_PACKET(bus, slot, func) ((((bus) << 16) & 0x00ff0000) | \
                                    (((slot) << 11) & 0x0000f800) | \
                                    (((func) << 8)  & 0x00000700))

/*********************************************************************************************************
  T. Straumann, 7/31/2001: increased to 32 - PMC slots are not
  scanned on mvme2306 otherwise
*********************************************************************************************************/

#define PCI_MAX_BUS             256
#define PCI_MAX_SLOTS           32
#define PCI_MAX_FUNCTIONS       8

/*********************************************************************************************************
  Configuration I/O addresses for mechanism 1
*********************************************************************************************************/

#define PCI_CONFIG_ADDR         0x0cf8                                  /* write 32 bits to set address */
#define PCI_CONFIG_DATA         0x0cfc                                  /* 8, 16, or 32 bit accesses    */

/*********************************************************************************************************
  Configuration I/O addresses for mechanism 2
*********************************************************************************************************/

#define PCI_CONFIG_CSE          0x0cf8                                  /* CSE register                 */
#define PCI_CONFIG_FORWARD      0x0cfa                                  /* forward register             */
#define PCI_CONFIG_BASE         0xc000                                  /* base register                */

/*********************************************************************************************************
  PCI access config
*********************************************************************************************************/

typedef struct {
    PCI_DRV_FUNCS              *PCIC_pDrvFuncs;
    
#define PCI_MECHANISM_1         1
#define PCI_MECHANISM_2         2
    UINT8                       PCIC_ucMechanism;
    
    addr_t                      PCIC_ulConfigAddr;
    addr_t                      PCIC_ulConfigData;
    addr_t                      PCIC_ulConfigBase;                      /* only for PCI_MECHANISM_2     */
    
    LW_OBJECT_HANDLE            PCIC_ulLock;
} PCI_CONFIG;

/*********************************************************************************************************
  API
  API_PciConfigInit() 必须在 BSP 初始化总线系统时被调用, 而且必须保证是第一个被正确调用的 PCI 系统函数.
*********************************************************************************************************/

LW_API INT      API_PciConfigInit(PCI_CONFIG *p_pcicfg);
LW_API VOID     API_PciConfigReset(INT  iRebootType);

LW_API INT      API_PciLock(VOID);
LW_API INT      API_PciUnlock(VOID);

LW_API INT      API_PciConfigInByte(INT iBus, INT iSlot, INT iFunc, INT iOft, UINT8 *pucValue);
LW_API INT      API_PciConfigInWord(INT iBus, INT iSlot, INT iFunc, INT iOft, UINT16 *pusValue);
LW_API INT      API_PciConfigInDword(INT iBus, INT iSlot, INT iFunc, INT iOft, UINT32 *puiValue);

LW_API INT      API_PciConfigOutByte(INT iBus, INT iSlot, INT iFunc, INT iOft, UINT8 ucValue);
LW_API INT      API_PciConfigOutWord(INT iBus, INT iSlot, INT iFunc, INT iOft, UINT16 usValue);
LW_API INT      API_PciConfigOutDword(INT iBus, INT iSlot, INT iFunc, INT iOft, UINT32 uiValue);

LW_API INT      API_PciConfigModifyByte(INT iBus, INT iSlot, INT iFunc, INT iOft, 
                                        UINT8 ucMask, UINT8 ucValue);
LW_API INT      API_PciConfigModifyWord(INT iBus, INT iSlot, INT iFunc, INT iOft, 
                                        UINT16 usMask, UINT16 usValue);
LW_API INT      API_PciConfigModifyDword(INT iBus, INT iSlot, INT iFunc, INT iOft, 
                                         UINT32 uiMask, UINT32 uiValue);
                                         
LW_API INT      API_PciSpecialCycle(INT iBus, UINT32 uiMsg);

LW_API INT      API_PciFindDev(UINT16 usVendorId, UINT16 usDeviceId, INT  iInstance,
                               INT *piBus, INT *piSlot, INT *piFunc);
LW_API INT      API_PciFindClass(UINT16  usClassCode, INT  iInstance,
                                 INT *piBus, INT *piSlot, INT *piFunc);
                                 
LW_API INT      API_PciTraversal(INT (*pfuncCall)(), PVOID pvArg, INT iMaxBusNum);
LW_API INT      API_PciTraversalDev(INT iBusStart, BOOL bSubBus, INT (*pfuncCall)(), PVOID pvArg);
                                 
LW_API INT      API_PciConfigDev(INT iBus, INT iSlot, INT iFunc, 
                                 UINT32 uiIoBase, UINT32 uiMemBase, UINT8 ucLatency,UINT32 uiCommand);
LW_API INT      API_PciFuncDisable(INT iBus, INT iSlot, INT iFunc);
                                 
LW_API INT      API_PciInterConnect(ULONG ulVector, PINT_SVR_ROUTINE pfuncIsr, 
                                    PVOID pvArg, CPCHAR pcName);
LW_API INT      API_PciInterDisconnect(ULONG ulVector, PINT_SVR_ROUTINE pfuncIsr, 
                                       PVOID pvArg);
                                       
LW_API INT      API_PciGetHeader(INT iBus, INT iSlot, INT iFunc, PCI_HDR *p_pcihdr);
                                       
#define pciConfigInit           API_PciConfigInit
#define pciConfigReset          API_PciConfigReset

#define pciLock                 API_PciLock
#define pciUnlock               API_PciUnlock

#define pciConfigInByte         API_PciConfigInByte
#define pciConfigInWord         API_PciConfigInWord
#define pciConfigInDword        API_PciConfigInDword

#define pciConfigOutByte        API_PciConfigOutByte
#define pciConfigOutWord        API_PciConfigOutWord
#define pciConfigOutDword       API_PciConfigOutDword

#define pciConfigModifyByte     API_PciConfigModifyByte
#define pciConfigModifyWord     API_PciConfigModifyWord
#define pciConfigModifyDword    API_PciConfigModifyDword

#define pciSpecialCycle         API_PciSpecialCycle

#define pciFindDev              API_PciFindDev
#define pciFindClass            API_PciFindClass

#define pciTraversal            API_PciTraversal
#define pciTraversalDev         API_PciTraversalDev

#define pciConfigDev            API_PciConfigDev
#define pciFuncDisable          API_PciFuncDisable

#define pciInterConnect         API_PciInterConnect
#define pciInterDisconnect      API_PciInterDisconnect

#define pciGetHeader            API_PciGetHeader

#endif                                                                  /*  (LW_CFG_DEVICE_EN > 0) &&   */
                                                                        /*  (LW_CFG_PCI_EN > 0)         */
#endif                                                                  /*  __PCI_DEV_H                 */
/*********************************************************************************************************
  END
*********************************************************************************************************/
