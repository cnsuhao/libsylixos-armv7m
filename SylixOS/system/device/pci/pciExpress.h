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
** 文   件   名: pciExpress.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 09 月 28 日
**
** 描        述: PCIE 相关定义.
*********************************************************************************************************/

#ifndef __PCI_EXPRESS_H
#define __PCI_EXPRESS_H

/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_PCI_EN > 0)

/*********************************************************************************************************
  PCI Express Capability Register
*********************************************************************************************************/

#define PCI_EXP_CAP_REG                 0x02            /* PCI Express Cap. Reg. Offset                 */

#define PCI_EXP_CAP_VER                 0x000f          /* PCI Express Capability Version               */
#   define PCI_EXP_VER_1                0x1             /* PCI Express Version - must be 0x1            */
#   define PCI_EXP_CAP_PORT_TYPE        0x00f0          /* Device Port type                             */
#   define PCI_EXP_TYPE_ENDPOINT        0x0             /* Express Endpoint                             */
#   define PCI_EXP_TYPE_LEG_END         0x1             /* Legacy Endpoint                              */
#   define PCI_EXP_TYPE_ROOT_PORT       0x4             /* Root Port                                    */
#   define PCI_EXP_TYPE_UPSTREAM        0x5             /* Upstream Port                                */
#   define PCI_EXP_TYPE_DOWNSTREAM      0x6             /* Downstream Port                              */
#   define PCI_EXP_TYPE_EXP2PCI         0x7             /* Express-to-PCI/PCI-X Bridge                  */
#   define PCI_EXP_TYPE_PCI2EXP         0x8             /* PCI/PCI-X Bridge-to-Express                  */
#define PCI_EXP_CAP_SLOT_ADDON          0x100           /* Slot implemented                             */
#define PCI_EXP_CAP_IRQ                 0x3e00          /* Interrupt message no.                        */

/*********************************************************************************************************
  PCI Express Device Capabilities Register
*********************************************************************************************************/

#define PCI_EXP_DEVCAP_REG              0x04            /* PCI Express Dev. Cap. Reg. Offset            */

#define PCI_EXP_DEVCAP_PAYLOAD          0x07            /* Max_Payload_Size                             */
#   define PCI_EXP_PAYLOAD_128          0x0             /* 128 bytes max                                */
#   define PCI_EXP_PAYLOAD_256          0x1             /* 256 bytes max                                */
#   define PCI_EXP_PAYLOAD_512          0x2             /* 512 bytes max                                */
#   define PCI_EXP_PAYLOAD_1KB          0x3             /* 1K bytes max                                 */
#   define PCI_EXP_PAYLOAD_2KB          0x4             /* 2K bytes max                                 */
#   define PCI_EXP_PAYLOAD_4KB          0x5             /* 4K bytes max                                 */
#define PCI_EXP_DEVCAP_PHANTOM          0x18            /* Phantom functions                            */
#   define PCI_EXP_PHANTOM_NOTAVAIL     0x0             /* Phantom function not available               */
#   define PCI_EXP_PHANTOM_MSB          0x1             /* Most Sig Bit of function no.                 */
#   define PCI_EXP_PHANTOM_2MSB         0x2             /* 2 Most Sig Bit of function no.               */
#   define PCI_EXP_PHANTOM_3MSB         0x3             /* 3 Most Sig Bit of function no.               */
#define PCI_EXP_DEVCAP_EXT_TAG          0x20            /* Extended tags                                */
#   define PCI_EXP_EXT_TAG_5BIT         0x0             /* 5 bit Tag                                    */
#   define PCI_EXP_EXT_TAG_8BIT         0x1             /* 8 bit Tag                                    */
#define PCI_EXP_DEVCAP_L0S              0x1c0           /* EndPt L0s Acceptable Latency                 */
#   define PCI_EXP_L0S_LT_64NS          0x0             /* Less than 64ns                               */
#   define PCI_EXP_L0S_64NS_to_128NS    0x1             /* 64ns to less than 128ns                      */
#   define PCI_EXP_L0S_128NS_to_256NS   0x2             /* 128ns to less than 256ns                     */
#   define PCI_EXP_L0S_256NS_to_512NS   0x3             /* 256ns to less than 512ns                     */
#   define PCI_EXP_L0S_512NS_to_1US     0x4             /* 512ns to less than 1us                       */
#   define PCI_EXP_L0S_1US_to_2US       0x5             /* 1us to less than 2us                         */
#   define PCI_EXP_L0S_2US_to_4US       0x6             /* 2us to less than 4us                         */
#   define PCI_EXP_L0S_GT_4US           0x7             /* Greater than 4us                             */
#define PCI_EXP_DEVCAP_L1               0xe00           /* EndPt L1 Acceptable Latency                  */
#   define PCI_EXP_L1_LT_1US            0x0             /* Less than 1us                                */
#   define PCI_EXP_L1_1US_to_2US        0x1             /* 1us to less than 2us                         */
#   define PCI_EXP_L1_2US_to_4US        0x2             /* 2us to less than 4us                         */
#   define PCI_EXP_L1_4US_to_8US        0x3             /* 4us to less than 8us                         */
#   define PCI_EXP_L1_8US_to_16US       0x4             /* 8us to less than 16us                        */
#   define PCI_EXP_L1_16US_to_32US      0x5             /* 16us to less than 32us                       */
#   define PCI_EXP_L1_32US_to_64US      0x6             /* 32us to less than 64us                       */
#   define PCI_EXP_L1_GT_64US           0x7             /* Greater than 64us                            */
#define PCI_EXP_DEVCAP_ATTN_BUTTON      0x1000          /* Attention Button Present                     */
#define PCI_EXP_DEVCAP_ATTN_IND         0x2000          /* Attention Indicator Present                  */
#define PCI_EXP_DEVCAP_PWR_IND          0x4000          /* Power Indicator Present                      */
#define PCI_EXP_DEVCAP_PWR_VAL_LIMIT    0x3fc0000       /* Captured Slot Power Limit Value              */
#define PCI_EXP_DEVCAP_PWR_SCL_LIMIT    0xc000000       /* Captured Slot Power Limit Scale              */
#   define PCI_EXP_PWR_SCL_LIMIT_ONE    0x0             /* 1.0 times                                    */
#   define PCI_EXP_PWR_SCL_LIMIT_10TH   0x1             /* 0.1 times                                    */
#   define PCI_EXP_PWR_SCL_LIMIT_100TH  0x2             /* 0.01 times                                   */
#   define PCI_EXP_PWR_SCL_LIMIT_1000TH 0x3             /* 0.001 times                                  */

/*********************************************************************************************************
  The Slot Power Limit and Slot Power Limit Scale values are either automatically set
  by a "Set Slot Power Limit Message" rec'd from port on downstream end, or
  hardwired to zero.
  
  To calculate Power Limit:
     Pwr Limit (watts) = value of Slot Power Limit x value of Slot Power Limit Scale
*********************************************************************************************************/

/*********************************************************************************************************
  PCI Express Device Control Register
*********************************************************************************************************/

#define PCI_EXP_DEVCTL_REG              0x08            /* PCI Express Dev. Ctrl Reg. Offset            */

#define PCI_EXP_DEVCTL_CERR_ENB         0x0001          /* Correctable Error Reporting En.              */
#define PCI_EXP_DEVCTL_NFERR_ENB        0x0002          /* Non-Fatal Error Reporting Enable             */
#define PCI_EXP_DEVCTL_FERR_ENB         0x0004          /* Fatal Error Reporting Enable                 */
#define PCI_EXP_DEVCTL_URREP_ENB        0x0008          /* Unsupported Request Reporting En.            */
#define PCI_EXP_DEVCTL_RLX_ORD_ENB      0x0010          /* Enable relaxed ordering                      */
#define PCI_EXP_DEVCTL_PAYLOAD          0x00e0          /* Max Payload Size                             */
    /* 
     * See Device Capabilities Register for Max Payload bit defines 
     */
#define PCI_EXP_DEVCTL_EXT_TAG          0x0100          /* Extended Tag Field Enable                    */
    /* 
     * See Device Capabilities Register for Tag Field bit defines 
     */
#define PCI_EXP_DEVCTL_PHANTOM          0x0200          /* Phantom Functions Enable                     */
    /* 
     * See Device Capabilities Register for Phantom Function bit defines 
     */
#define PCI_EXP_DEVCTL_AUX_PM_ENB       0x0400          /* Auxiliary Power PM Enable                    */
#define PCI_EXP_DEVCTL_NOSNOOP_ENB      0x0800          /* Enable No Snoop                              */
#define PCI_EXP_DEVCTL_READ_REQ         0x7000          /* Max Read Request Size                        */
#   define PCI_EXP_READ_REQ_128         0x0             /* 128 bytes max                                */
#   define PCI_EXP_READ_REQ_256         0x1             /* 256 bytes max                                */
#   define PCI_EXP_READ_REQ_512         0x2             /* 512 bytes max                                */
#   define PCI_EXP_READ_REQ_1KB         0x3             /* 1K bytes max                                 */
#   define PCI_EXP_READ_REQ_2KB         0x4             /* 2K bytes max                                 */
#   define PCI_EXP_READ_REQ_4KB         0x5             /* 4K bytes max                                 */

/*********************************************************************************************************
  PCI Express Device Status Register
*********************************************************************************************************/

#define PCI_EXP_DEVSTA_REG              0x0A            /* PCI Express Dev. Status Reg. Offset          */

#define PCI_EXP_DEVSTA_CERR_DET         0x1             /* Correctable Error Detected RW1C              */
#define PCI_EXP_DEVSTA_NFERR_DET        0x2             /* Non-Fatal Error Detected RW1C                */
#define PCI_EXP_DEVSTA_FERR_DET         0x4             /* Fatal Error Detected RW1C                    */
#define PCI_EXP_DEVSTA_UNREQ_DET        0x8             /* Unsupported Request Detected                 */
#define PCI_EXP_DEVSTA_AUXPWR_DET       0x10            /* AUX Power Detected RO                        */
#define PCI_EXP_DEVSTA_TRAN_PEND        0x20            /* Transactions Pending RO                      */

/*********************************************************************************************************
  PCI Express Link Capabilities Register
*********************************************************************************************************/

#define PCI_EXP_LNKCAP_REG              0x0C            /* PCI Express Link Capab. Reg. Offset          */

#define PCI_EXP_LNKCAP_LNK_SPD          0xF             /* Max Link Speed                               */
#   define PCI_EXP_LNK_SPD_2PT5GB       0x1             /* 2.5 Gb/s                                     */
#define PCI_EXP_LNKCAP_LNK_WDTH         0x3F0           /* Max Link Width                               */
#   define PCI_EXP_LNK_WDTH_1           0x1             /* x1                                           */
#   define PCI_EXP_LNK_WDTH_2           0x2             /* x2                                           */
#   define PCI_EXP_LNK_WDTH_4           0x4             /* x4                                           */
#   define PCI_EXP_LNK_WDTH_8           0x8             /* x8                                           */
#   define PCI_EXP_LNK_WDTH_12          0xC             /* x12                                          */
#   define PCI_EXP_LNK_WDTH_16          0x10            /* x16                                          */
#   define PCI_EXP_LNK_WDTH_32          0x20            /* x32                                          */
#define PCI_EXP_LNKCAP_ASPM             0xC00           /* Active State Mgmt Support                    */
#   define PCI_EXP_ASPM_L0              0x1             /* L0s Entry Supported                          */
#   define PCI_EXP_ASPM_L0_L1           0x3             /* L0s & L1s Supported                          */
#define PCI_EXP_LNKCAP_L0S_LAT          0x7000          /* L0s Exit Latency                             */
    /* 
     * See Device Capabilities Register for L0 Latency bit defines 
     */
#define PCI_EXP_LNKCAP_L1_LAT           0x38000         /* L1s Exit Latency                             */
    /* 
     * See Device Capabilities Register for L1 Latency bit defines 
     */
#define PCI_EXP_LNKCAP_PORTNO           0xFF000000      /* Port Number                                  */
    /* 
     * Port Number is assigned in HW 
     */
    
/*********************************************************************************************************
  PCI Express Link Control Register
*********************************************************************************************************/

#define PCI_EXP_LNKCTL_REG              0x10            /* PCI Express Link Ctl. Reg. Offset            */

#define PCI_EXP_LNKCTL_ASPM             0x3             /* Active State Mgmt Ctrl                       */
#define PCI_EXP_LNKCTL_RCB              0x8             /* Read Completion Boundary                     */
#define PCI_EXP_LNKCTL_LNK_DIS          0x10            /* Link Disable                                 */
#define PCI_EXP_LNKCTL_LNK_TRAIN        0x20            /* Link Retrain; Reads 0                        */
#define PCI_EXP_LNKCTL_CCC              0x40            /* Common Clk Config                            */
#define PCI_EXP_LNKCTL_EXT_SYNC         0x80            /* Extended Sync                                */

/*********************************************************************************************************
  PCI Express Link Status Register
*********************************************************************************************************/

#define PCI_EXP_LNKSTA_REG              0x12            /* PCI Express Link Status Reg. Offset          */

#define PCI_EXP_LNKSTA_LNK_SPD          0xF             /* Link Speed                                   */
    /* 
     * See Link Capabilities Register for Link Speed bit defines 
     */
#define PCI_EXP_LNKSTA_LNK_WDTH         0x3F0           /* Negotiead Link Width                         */
    /* 
     * See Link Capabilities Register for Link Width bit defines 
     */
#define PCI_EXP_LNKSTA_LNK_TERR         0x400           /* Link Training Error                          */
#define PCI_EXP_LNKSTA_LNK_TRAIN        0x800           /* Link Training                                */
#define PCI_EXP_LNKSTA_LNK_CLKCFG       0x1000          /* Slot Clk Config.                             */

/*********************************************************************************************************
  PCI Express Slot Capabilities Register
*********************************************************************************************************/

#define PCI_EXP_SLTCAP_REG              0x12            /* PCI Express Slot Capab. Reg. Offset          */

#define PCI_EXP_SLTCAP_ATTN_BUT         0x1             /* Attention Button Present                     */
#define PCI_EXP_SLTCAP_PWR_CTL          0x2             /* Power Controller Present                     */
#define PCI_EXP_SLTCAP_MRL              0x4             /* MRL Sensor Present                           */
#define PCI_EXP_SLTCAP_ATTN_IND         0x8             /* Attention Indicator Present                  */
#define PCI_EXP_SLTCAP_PWR_IND          0x10            /* Power Indicator Present                      */
#define PCI_EXP_SLTCAP_HPL_SUP          0x20            /* Hot Plug Surprise                            */
#define PCI_EXP_SLTCAP_HPL_CAP          0x40            /* Hot Plug Capable                             */
#define PCI_EXP_SLTCAP_PWR_VAL_LIMIT    0x7F80          /* Slot Pwr Limit Value                         */
#define PCI_EXP_SLTCAP_PWR_SCL_LIMIT    0x18000         /* Slot Pwr Limit Scale                         */
    /* 
     * See Device Capabilities Register for Slot Power Scale bit defines 
     */
#define PCI_EXP_SLTCAP_PHYS_SLOT        0xFFF80000      /* Physical Slot No.                            */

/*********************************************************************************************************
  PCI Express Slot Control Register
*********************************************************************************************************/

#define PCI_EXP_SLTCTL_REG              0x18            /* PCI Express Slot Status Reg. Offset          */

#define PCI_EXP_SLTCTL_ATTN_BUT_ENB     0x1             /* Attention Button Pressed Enabled             */
#define PCI_EXP_SLTCTL_PWRFLT_DET_ENB   0x2             /* Power Fault Detected Enabled                 */
#define PCI_EXP_SLTCTL_MRLSNSR_ENB      0x4             /* MRL Sensor Changed Enabled                   */
#define PCI_EXP_SLTCTL_PRES_DET_ENB     0x8             /* Presence Detect Changed Enabled              */
#define PCI_EXP_SLTCTL_CCMPLT_ENB       0x10            /* Command Complete Interrupt Enabled           */
#define PCI_EXP_SLTCTL_HPLINT_ENB       0x20            /* Hot Plug Interrupt Enabled                   */
#define PCI_EXP_SLTCTL_ATTN_INDCTL      0xC0            /* Attention Indicator Control                  */
#   define PCI_EXP_ATTN_INDCTL_ON       0x1             /* Attention Indicator Control                  */
#   define PCI_EXP_ATTN_INDCTL_BLINK    0x2             /* Attention Indicator Control                  */
#   define PCI_EXP_ATTN_INDCTL_OFF      0x3             /* Attention Indicator Control                  */
#define PCI_EXP_SLTCTL_PWR_INDCTL       0x30            /* Power Indicator Control                      */
#   define PCI_EXP_PWR_INDCTL_ON        0x1             /* Power Indicator Control                      */
#   define PCI_EXP_PWR_INDCTL_BLINK     0x2             /* Power Indicator Control                      */
#   define PCI_EXP_PWR_INDCTL_OFF       0x3             /* Power Indicator Control                      */
#define PCI_EXP_SLTCTL_PWR_CTLRCTL      0x400           /* Power Controller Control                     */
#   define PCI_EXP_PWR_CTLRCTL_ON       0x1             /* Power Controller Control                     */
#   define PCI_EXP_PWR_CTLRCTL_OFF      0x0             /* Power Controller Control                     */

/*********************************************************************************************************
  PCI Express Slot Status Register
*********************************************************************************************************/

#define PCI_EXP_SLTSTA_REG              0x1A            /* PCI Express Slot Status Reg. Offset          */

#define PCI_EXP_SLTSTA_ATTN_BUT         0x1             /* Attention Button Pressed                     */
#define PCI_EXP_SLTSTA_PWRFLT_DET       0x2             /* Power Fault Detected                         */
#define PCI_EXP_SLTSTA_MRLSNSR          0x4             /* MRL Sensor Changed                           */
#define PCI_EXP_SLTSTA_PRES_DET         0x8             /* Presence Detect Changed                      */
#define PCI_EXP_SLTSTA_CCMPLT           0x10            /* Command Completed                            */
#define PCI_EXP_SLTSTA_MRLSNSR_STAT     0x20            /* MRL Sensor Status                            */
#   define PCI_EXP_MRLSNSR_STAT_CLSD    0x0             /* MRL Sensor Status                            */
#   define PCI_EXP_MRLSNSR_STAT_OPEN    0x1             /* MRL Sensor Status                            */
#define PCI_EXP_SLTSTA_PRES_DET_STAT    0x40            /* Presence Detect Status                       */
#   define PCI_EXP_PRES_DET_STAT_EMPTY  0x0             /* Presence Detect Status                       */
#   define PCI_EXP_PRES_DET_STAT_PRES   0x1             /* Presence Detect Status                       */

/*********************************************************************************************************
  PCI Express Root Control Register
*********************************************************************************************************/

#define PCI_EXP_RTCTL_REG               0x1C            /* Root Control Register                        */

#define PCI_EXP_RTCTL_SECE_ENB          0x01            /* System Error on Correctable Error            */
#define PCI_EXP_RTCTL_SENFE_ENB         0x02            /* System Error on Non-Fatal Error              */
#define PCI_EXP_RTCTL_SEFE_ENB          0x04            /* System Error on Fatal Error                  */
#define PCI_EXP_RTCTL_PMEI_ENB          0x08            /* PME Interrupt Enable                         */

/*********************************************************************************************************
  PCI Express Root Status Register
*********************************************************************************************************/

#define PCI_EXP_RTSTA                   32              /* Root Status                                  */

/*********************************************************************************************************
  Extended Capabilities (PCI-X 2.0 and Express)
*********************************************************************************************************/

#define PCI_EXT_CAP_ID(header)          (header & 0x0000ffff)
#define PCI_EXT_CAP_VER(header)         ((header >> 16) & 0xf)
#define PCI_EXT_CAP_NEXT(header)        ((header >> 20) & 0xffc)

#define PCI_EXT_CAP_ID_ERR              1
#define PCI_EXT_CAP_ID_VC               2
#define PCI_EXT_CAP_ID_DSN              3
#define PCI_EXT_CAP_ID_PWR              4

#endif                                                                  /*  (LW_CFG_DEVICE_EN > 0) &&   */
                                                                        /*  (LW_CFG_PCI_EN > 0)         */
#endif                                                                  /*  __PCI_EXPRESS_H             */
/*********************************************************************************************************
  END
*********************************************************************************************************/
