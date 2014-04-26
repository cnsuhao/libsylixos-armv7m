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
** 文   件   名: sdhci.h
**
** 创   建   人: Zeng.Bo (曾波)
**
** 文件创建日期: 2011 年 01 月 14 日
**
** 描        述: sd标准主控制器驱动头文件(SD Host Controller Simplified Specification Version 2.00).
**               注意所有寄存器定义都为相对偏移地址.
**               对于使用标准控制器的平台而言,使用该模块完全屏蔽了sd bus层的操作,移植工作更简单.

** BUG:
** 2011.03.02  增加主控传输模式查看\设置函数.允许动态改变传输模式(主控上不存在设备的情况下).
** 2011.04.12  主控属性里增加最大时钟频率这一项.
** 2011.05.10  考虑到SD控制器在不同平台上其寄存器可能在IO空间,也可能在内存空间,所以读写寄存器的6个函数申明
**             为外部函数,由具体平台的驱动实现.
** 2011.06.01  访问寄存器的与硬件平台相关的函数采用回调方式,由驱动实现.
**             今天是六一儿童节,回想起咱的童年,哎,一去不复返.祝福天下的小朋友们茁壮成长,健康快乐.
*********************************************************************************************************/

#ifndef SDHCI_H
#define SDHCI_H

/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_SDCARD_EN > 0)

/*********************************************************************************************************
   系统SDMA地址寄存器.
*********************************************************************************************************/

#define SDHCI_SYS_SDMA                  0x00                            /*  DMA传输时指向一个内存地址   */

/*********************************************************************************************************
    块大小寄存器.该寄存器用于描述DMA传输(bit 12 - 14)中的地址边界和一般传输中的块大小(bit 0 - 11)
*********************************************************************************************************/

#define SDHCI_BLOCK_SIZE                0x04
#define SDHCI_MAKE_BLKSZ(dma, blksz)    (((dma & 0x7) << 12) | (blksz & 0xfff))

/*********************************************************************************************************
    块计数寄存器.
*********************************************************************************************************/

#define SDHCI_BLOCK_COUNT               0x06

/*********************************************************************************************************
    参数寄存器.
*********************************************************************************************************/

#define SDHCI_ARGUMENT                  0x08

/*********************************************************************************************************
    传输模式寄存器.
*********************************************************************************************************/

#define SDHCI_TRANSFER_MODE             0x0c
#define SDHCI_TRNS_DMA                  0x01
#define SDHCI_TRNS_BLK_CNT_EN           0x02
#define SDHCI_TRNS_ACMD12               0x04
#define SDHCI_TRNS_READ                 0x10
#define SDHCI_TRNS_MULTI                0x20

/*********************************************************************************************************
    命令寄存器.
*********************************************************************************************************/

#define SDHCI_COMMAND                   0x0e
#define SDHCI_CMD_CRC_CHK               0x08
#define SDHCI_CMD_INDEX_CHK             0x10
#define SDHCI_CMD_DATA                  0x20
#define SDHCI_CMD_RESP_TYPE_MASK        0x03
#define SDHCI_CMD_RESP_TYPE_NONE        0x00
#define SDHCI_CMD_RESP_TYPE_LONG        0x01
#define SDHCI_CMD_RESP_TYPE_SHORT       0x02
#define SDHCI_CMD_RESP_TYPE_SHORT_BUSY  0x03

#define SDHCI_MAKE_CMD(cmd, flg)        (((cmd & 0xff) << 8) | (flg & 0xff))

/*********************************************************************************************************
    响应寄存器.
*********************************************************************************************************/

#define SDHCI_RESPONSE0                 0x10
#define SDHCI_RESPONSE1                 0x14
#define SDHCI_RESPONSE2                 0x18
#define SDHCI_RESPONSE3                 0x1c

/*********************************************************************************************************
    数据缓冲寄存器.
*********************************************************************************************************/

#define SDHCI_BUFFER                    0x20

/*********************************************************************************************************
    当前状态寄存器.
*********************************************************************************************************/

#define SDHCI_PRESENT_STATE             0x24
#define SDHCI_PSTA_CMD_INHIBIT          0x00000001
#define SDHCI_PSTA_DATA_INHIBIT         0x00000002
#define SDHCI_PSTA_DATA_ACTIVE          0x00000004
#define SDHCI_PSTA_DOING_WRITE          0x00000100
#define SDHCI_PSTA_DOING_READ           0x00000200
#define SDHCI_PSTA_SPACE_AVAILABLE      0x00000400
#define SDHCI_PSTA_DATA_AVAILABLE       0x00000800
#define SDHCI_PSTA_CARD_PRESENT         0x00010000
#define SDHCI_PSTA_WRITE_PROTECT        0x00080000

/*********************************************************************************************************
    主控器控制寄存器.
*********************************************************************************************************/

#define SDHCI_HOST_CONTROL              0x28
#define SDHCI_HCTRL_LED                 0x01
#define SDHCI_HCTRL_4BITBUS             0x02
#define SDHCI_HCTRL_HISPD               0x04
#define SDHCI_HCTRL_DMA_MASK            0x18
#define SDHCI_HCTRL_SDMA                0x00
#define SDHCI_HCTRL_ADMA1               0x08
#define SDHCI_HCTRL_ADMA32              0x10
#define SDHCI_HCTRL_ADMA64              0x18
#define SDHCI_HCTRL_MMC_BITS8           0x20

/*********************************************************************************************************
    电源控制寄存器.
*********************************************************************************************************/

#define SDHCI_POWER_CONTROL             0x29
#define SDHCI_POWCTL_ON                 0x01
#define SDHCI_POWCTL_180                0x0a
#define SDHCI_POWCTL_300                0x0c
#define SDHCI_POWCTL_330                0x0e

/*********************************************************************************************************
    块间隔寄存器.
*********************************************************************************************************/

#define SDHCI_BLOCK_GAP_CONTROL         0x2a
#define SDHCI_BLKGAP_STOP               0x01
#define SDHCI_BLKGAP_RESTART            0x02
#define SDHCI_BLKGAP_RWCTL_EN           0x04
#define SDHCI_BLKGAP_INT_EN             0x08

/*********************************************************************************************************
    唤醒控制寄存器.
*********************************************************************************************************/

#define SDHCI_WAKE_UP_CONTROL           0x2b
#define SDHCI_WAKUP_INT_EN              0x01
#define SDHCI_WAKUP_INSERT_EN           0x02
#define SDHCI_WAKUP_REMOVE_EN           0x04

/*********************************************************************************************************
    时钟控制寄存器.
*********************************************************************************************************/

#define SDHCI_CLOCK_CONTROL             0x2c
#define SDHCI_CLKCTL_DIVIDER_SHIFT      8
#define SDHCI_CLKCTL_CLOCK_EN           0x0004
#define SDHCI_CLKCTL_INTER_STABLE       0x0002
#define SDHCI_CLKCTL_INTER_EN           0x0001

/*********************************************************************************************************
   超时计数控制寄存器.
*********************************************************************************************************/

#define SDHCI_TIMEOUT_CONTROL           0x2e

/*********************************************************************************************************
    软件复位寄存器.
*********************************************************************************************************/

#define SDHCI_SOFTWARE_RESET            0x2f
#define SDHCI_SFRST_ALL                 0x01
#define SDHCI_SFRST_CMD                 0x02
#define SDHCI_SFRST_DATA                0x04

/*********************************************************************************************************
    一般中断状态寄存器.
*********************************************************************************************************/

#define SDHCI_INT_STATUS                0x30
#define SDHCI_INTSTA_CMD_END            0x0001
#define SDHCI_INTSTA_DATA_END           0x0002
#define SDHCI_INTSTA_BLKGAP_END         0x0004
#define SDHCI_INTSTA_DMA                0x0008
#define SDHCI_INTSTA_WRTBUF_RDY         0x0010
#define SDHCI_INTSTA_RDBUF_RDY          0x0020
#define SDHCI_INTSTA_CARD_INSERT        0x0040
#define SDHCI_INTSTA_CARD_REMOVE        0x0080
#define SDHCI_INTSTA_CARD_INT           0x0100
#define SDHCI_INTSTA_CCS_INT            0x0200
#define SDHCI_INTSTA_RDWAIT_INT         0x0400
#define SDHCI_INTSTA_FIFA0_INT          0x0800
#define SDHCI_INTSTA_FIFA1_INT          0x1000
#define SDHCI_INTSTA_FIFA2_INT          0x2000
#define SDHCI_INTSTA_FIFA3_INT          0x4000
#define SDHCI_INTSTA_INT_ERROR          0x8000

/*********************************************************************************************************
    错误中断状态寄存器.
*********************************************************************************************************/

#define SDHCI_ERRINT_STATUS             0x32
#define SDHCI_EINTSTA_CMD_TIMOUT        0x0001
#define SDHCI_EINTSTA_CMD_CRC           0x0002
#define SDHCI_EINTSTA_CMD_ENDBIT        0x0004
#define SDHCI_EINTSTA_CMD_INDEX         0x0008
#define SDHCI_EINTSTA_DATA_TIMOUT       0x0010
#define SDHCI_EINTSTA_DATA_CRC          0x0020
#define SDHCI_EINTSTA_DATA_ENDBIT       0x0040
#define SDHCI_EINTSTA_CMD_CURRLIMIT     0x0080
#define SDHCI_EINTSTA_CMD_ACMD12        0x0100
#define SDHCI_EINTSTA_CMD_ADMA          0x0200


/*********************************************************************************************************
   一般中断状态使能寄存器.
*********************************************************************************************************/

#define SDHCI_INTSTA_ENABLE             0x34
#define SDHCI_INTSTAEN_CMD              0x0001
#define SDHCI_INTSTAEN_DATA             0x0002
#define SDHCI_INTSTAEN_BLKGAP           0x0004
#define SDHCI_INTSTAEN_DMA              0x0008
#define SDHCI_INTSTAEN_WRTBUF           0x0010
#define SDHCI_INTSTAEN_RDBUF            0x0020
#define SDHCI_INTSTAEN_CARD_INSERT      0x0040
#define SDHCI_INTSTAEN_CARD_REMOVE      0x0080
#define SDHCI_INTSTAEN_CARD_INT         0x0100
#define SDHCI_INTSTAEN_CCS_INT          0x0200
#define SDHCI_INTSTAEN_RDWAIT           0x0400
#define SDHCI_INTSTAEN_FIFA0            0x0800
#define SDHCI_INTSTAEN_FIFA1            0x1000
#define SDHCI_INTSTAEN_FIFA2            0x2000
#define SDHCI_INTSTAEN_FIFA3            0x4000

/*********************************************************************************************************
   错误中断状态使能寄存器.
*********************************************************************************************************/

#define SDHCI_EINTSTA_ENABLE            0x36
#define SDHCI_EINTSTAEN_CMD_TIMEOUT     0x0001
#define SDHCI_EINTSTAEN_CMD_CRC         0x0002
#define SDHCI_EINTSTAEN_CMD_ENDBIT      0x0004
#define SDHCI_EINTSTAEN_CMD_INDEX       0x0008
#define SDHCI_EINTSTAEN_DATA_TIMEOUT    0x0010
#define SDHCI_EINTSTAEN_DATA_CRC        0x0020
#define SDHCI_EINTSTAEN_DATA_ENDBIT     0x0040
#define SDHCI_EINTSTAEN_CURR_LIMIT      0x0080
#define SDHCI_EINTSTAEN_ACMD12          0x0100
#define SDHCI_EINTSTAEN_ADMA            0x0200

/*********************************************************************************************************
  一般中断信号使能寄存器.
*********************************************************************************************************/

#define SDHCI_SIGNAL_ENABLE             0x38
#define SDHCI_SIGEN_CMD_END             0x0001
#define SDHCI_SIGEN_DATA_END            0x0002
#define SDHCI_SIGEN_BLKGAP              0x0004
#define SDHCI_SIGEN_DMA                 0x0008
#define SDHCI_SIGEN_WRTBUF              0x0010
#define SDHCI_SIGEN_RDBUF               0x0020
#define SDHCI_SIGEN_CARD_INSERT         0x0040
#define SDHCI_SIGEN_CARD_REMOVE         0x0080
#define SDHCI_SIGEN_CARD_INT            0x0100

/*********************************************************************************************************
  错误中断信号使能寄存器.
*********************************************************************************************************/

#define SDHCI_ERRSIGNAL_ENABLE          0x3a
#define SDHCI_ESIGEN_CMD_TIMEOUT        0x0001
#define SDHCI_ESIGEN_CMD_CRC            0x0002
#define SDHCI_ESIGEN_CMD_ENDBIT         0x0004
#define SDHCI_ESIGEN_CMD_INDEX          0x0008
#define SDHCI_ESIGEN_DATA_TIMEOUT       0x0010
#define SDHCI_ESIGEN_DATA_CRC           0x0020
#define SDHCI_ESIGEN_DATA_ENDBIT        0x0040
#define SDHCI_ESIGEN_CURR_LIMIT         0x0080
#define SDHCI_ESIGEN_ACMD12             0x0100
#define SDHCI_ESIGEN_ADMA               0x0200

/*********************************************************************************************************
  following is for the case of 32-bit registers operation.
*********************************************************************************************************/

#define SDHCI_INT_RESPONSE              0x00000001
#define SDHCI_INT_DATA_END              0x00000002
#define SDHCI_INT_DMA_END               0x00000008
#define SDHCI_INT_SPACE_AVAIL           0x00000010
#define SDHCI_INT_DATA_AVAIL            0x00000020
#define SDHCI_INT_CARD_INSERT           0x00000040
#define SDHCI_INT_CARD_REMOVE           0x00000080
#define SDHCI_INT_CARD_INT              0x00000100
#define SDHCI_INT_ERROR                 0x00008000                      /*  normal                      */

#define SDHCI_INT_TIMEOUT               0x00010000
#define SDHCI_INT_CRC                   0x00020000
#define SDHCI_INT_END_BIT               0x00040000
#define SDHCI_INT_INDEX                 0x00080000
#define SDHCI_INT_DATA_TIMEOUT          0x00100000
#define SDHCI_INT_DATA_CRC              0x00200000
#define SDHCI_INT_DATA_END_BIT          0x00400000
#define SDHCI_INT_BUS_POWER             0x00800000
#define SDHCI_INT_ACMD12ERR             0x01000000
#define SDHCI_INT_ADMA_ERROR            0x02000000                      /*  error                       */

#define SDHCI_INT_NORMAL_MASK           0x00007fff
#define SDHCI_INT_ERROR_MASK            0xffff8000

#define SDHCI_INT_CMD_MASK              (SDHCI_INT_RESPONSE     | \
                                         SDHCI_INT_TIMEOUT      | \
                                         SDHCI_INT_CRC          | \
                                         SDHCI_INT_END_BIT      | \
                                         SDHCI_INT_INDEX)
#define SDHCI_INT_DATA_MASK             (SDHCI_INT_DATA_END     | \
                                         SDHCI_INT_DMA_END      | \
                                         SDHCI_INT_DATA_AVAIL   | \
                                         SDHCI_INT_SPACE_AVAIL  | \
                                         SDHCI_INT_DATA_TIMEOUT | \
                                         SDHCI_INT_DATA_CRC     | \
                                         SDHCI_INT_DATA_END_BIT | \
                                         SDHCI_INT_ADMA_ERROR)
#define SDHCI_INT_ALL_MASK              ((unsigned int)-1)

/*********************************************************************************************************
    自动CMD12错误状态寄存器.
*********************************************************************************************************/

#define SDHCI_ACMD12_ERR                0x3c
#define SDHCI_EACMD12_EXE               0x0001
#define SDHCI_EACMD12_TIMEOUT           0x0002
#define SDHCI_EACMD12_CRC               0x0004
#define SDHCI_EACMD12_ENDBIT            0x0008
#define SDHCI_EACMD12_INDEX             0x0010
#define SDHCI_EACMD12_CMDISSUE          0x0080

/*********************************************************************************************************
    主控功能寄存器.
*********************************************************************************************************/

#define SDHCI_CAPABILITIES              0x40
#define SDHCI_CAP_TIMEOUT_CLK_MASK      0x0000003f
#define SDHCI_CAP_TIMEOUT_CLK_SHIFT     0
#define SDHCI_CAP_TIMEOUT_CLK_UNIT      0x00000080
#define SDHCI_CAP_BASECLK_MASK          0x00003f00
#define SDHCI_CAP_BASECLK_SHIFT         8
#define SDHCI_CAP_MAXBLK_MASK           0x00030000
#define SDHCI_CAP_MAXBLK_SHIFT          16
#define SDHCI_CAP_CAN_DO_ADMA           0x00080000
#define SDHCI_CAP_CAN_DO_HISPD          0x00200000
#define SDHCI_CAP_CAN_DO_SDMA           0x00400000
#define SDHCI_CAP_CAN_DO_SUSRES         0x00800000
#define SDHCI_CAP_CAN_VDD_330           0x01000000
#define SDHCI_CAP_CAN_VDD_300           0x02000000
#define SDHCI_CAP_CAN_VDD_180           0x04000000
#define SDHCI_CAP_CAN_64BIT             0x10000000

/*********************************************************************************************************
    Maximum current capability register.
*********************************************************************************************************/

#define SDHCI_MAX_CURRENT               0x48                            /*  4c-4f reserved for more     */
                                                                        /*  for more max current        */

/*********************************************************************************************************
    设置(force set, 就是主动产生中断)ACMD12错误寄存器.
*********************************************************************************************************/

#define SDHCI_SET_ACMD12_ERROR          0x50

/*********************************************************************************************************
    设置(force set, 即主动产生中断)中断错误寄存器.
*********************************************************************************************************/

#define SDHCI_SET_INT_ERROR             0x52

/*********************************************************************************************************
    ADMA错误状态寄存器.
*********************************************************************************************************/

#define SDHCI_ADMA_ERROR                0x54

/*********************************************************************************************************
   ADMA地址寄存器.
*********************************************************************************************************/

#define SDHCI_ADMA_ADDRESS              0x58

/*********************************************************************************************************
    插槽中断状态寄存器.
*********************************************************************************************************/

#define SDHCI_SLOT_INT_STATUS           0xfc
#define SDHCI_SLOTINT_MASK              0x00ff
#define SDHCI_CHK_SLOTINT(slotsta, n)   ((slotsta & 0x00ff) | (1 << (n)))

/*********************************************************************************************************
    主控制器版本寄存器.
*********************************************************************************************************/

#define SDHCI_HOST_VERSION              0xfe
#define SDHCI_HVER_VENDOR_VER_MASK      0xff00
#define SDHCI_HVER_VENDOR_VER_SHIFT     8
#define SDHCI_HVER_SPEC_VER_MASK        0x00ff
#define SDHCI_HVER_SPEC_VER_SHIFT       0
#define SDHCI_HVER_SPEC_100             0x0000
#define SDHCI_HVER_SPEC_200             0x0001

/*********************************************************************************************************
    其他宏定义.
*********************************************************************************************************/

#define SDHCI_DELAYMS(ms)                                   \
        do {                                                \
            ULONG   ulTimeout = LW_MSECOND_TO_TICK_1(ms);   \
            API_TimeSleep(ulTimeout);                       \
        } while (0)

/*********************************************************************************************************
    主控传输模式设置使用参数.
*********************************************************************************************************/

#define SDHCIHOST_TMOD_SET_NORMAL       0
#define SDHCIHOST_TMOD_SET_SDMA         1
#define SDHCIHOST_TMOD_SET_ADMA         2

/*********************************************************************************************************
    主控支持的传输模式组合位标定义.
*********************************************************************************************************/

#define SDHCIHOST_TMOD_CAN_NORMAL       (1 << 0)
#define SDHCIHOST_TMOD_CAN_SDMA         (1 << 1)
#define SDHCIHOST_TMOD_CAN_ADMA         (1 << 2)

/*********************************************************************************************************
  SD标准主控制器属性结构
*********************************************************************************************************/

struct _sdhci_drv_funcs;                                                /*  标准主控驱动函数声明        */
typedef struct _sdhci_drv_funcs SDHCI_DRV_FUNCS;

typedef struct lw_sdhci_host_attr {
    SDHCI_DRV_FUNCS *SDHCIHOST_pDrvFuncs;                               /*  标准主控驱动函数结构指针    */
    LONG             SDHCIHOST_lBasePoint;                              /*  槽基地址指针.该域的设置必须 */
                                                                        /*  特别注意,因为内部无法检测其 */
                                                                        /*  合法性.                     */
    UINT32           SDHCIHOST_uiMaxClock;                              /*  如果控制器没有内部时钟,用户 */
                                                                        /*  需要提供时钟源              */
} LW_SDHCI_HOST_ATTR, *PLW_SDHCI_HOST_ATTR;

/*********************************************************************************************************
  SD标准主控驱动结构体
*********************************************************************************************************/

struct _sdhci_drv_funcs {
    UINT32        (*sdhciReadL)
                  (
                  PLW_SDHCI_HOST_ATTR  phostattr,
                  LONG                 lReg
                  );
    UINT16        (*sdhciReadW)
                  (
                  PLW_SDHCI_HOST_ATTR  phostattr,
                  LONG                 lReg
                  );
    UINT8         (*sdhciReadB)
                  (
                  PLW_SDHCI_HOST_ATTR  phostattr,
                  LONG                 lReg
                  );
    VOID          (*sdhciWriteL)
                  (
                  PLW_SDHCI_HOST_ATTR  phostattr,
                  LONG                 lReg,
                  UINT32               uiLword
                  );
    VOID          (*sdhciWriteW)
                  (
                  PLW_SDHCI_HOST_ATTR  phostattr,
                  LONG                 lReg,
                  UINT16               usWord
                  );
    VOID          (*sdhciWriteB)
                  (
                  PLW_SDHCI_HOST_ATTR  phostattr,
                  LONG                 lReg,
                  UINT8                ucByte
                  );
};

#define SDHCI_READL(phostattr, lReg)           \
        ((phostattr)->SDHCIHOST_pDrvFuncs->sdhciReadL)(phostattr, lReg)
#define SDHCI_READW(phostattr, lReg)           \
        ((phostattr)->SDHCIHOST_pDrvFuncs->sdhciReadW)(phostattr, lReg)
#define SDHCI_READB(phostattr, lReg)           \
        ((phostattr)->SDHCIHOST_pDrvFuncs->sdhciReadB)(phostattr, lReg)

#define SDHCI_WRITEL(phostattr, lReg, uiLword) \
        ((phostattr)->SDHCIHOST_pDrvFuncs->sdhciWriteL)(phostattr, lReg, uiLword)
#define SDHCI_WRITEW(phostattr, lReg, usWord)  \
        ((phostattr)->SDHCIHOST_pDrvFuncs->sdhciWriteW)(phostattr, lReg, usWord)
#define SDHCI_WRITEB(phostattr, lReg, ucByte)  \
        ((phostattr)->SDHCIHOST_pDrvFuncs->sdhciWriteB)(phostattr, lReg, ucByte)
/*********************************************************************************************************
  SD标准主控制器操作
*********************************************************************************************************/

LW_API PVOID      API_SdhciHostCreate(CPCHAR               pcAdapterName,
                                      PLW_SDHCI_HOST_ATTR  psdhcihostattr);
LW_API INT        API_SdhciHostDelete(PVOID            pvHost);

LW_API INT        API_SdhciHostTransferModGet(PVOID    pvHost);
LW_API INT        API_SdhciHostTransferModSet(PVOID    pvHost, INT   iTmod);

/*********************************************************************************************************
  SD标准主控制器设备操作
*********************************************************************************************************/

LW_API PVOID      API_SdhciDeviceAdd(PVOID            pvHost,
                                     CPCHAR           pcDeviceName);
LW_API INT        API_SdhciDeviceRemove(PVOID         pvDevice);

LW_API INT        API_SdhciDeviceUsageInc(PVOID       pvDevice);
LW_API INT        API_SdhciDeviceUsageDec(PVOID       pvDevice);
LW_API INT        API_SdhciDeviceUsageGet(PVOID       pvDevice);

#endif                                                                  /*  (LW_CFG_DEVICE_EN > 0)      */
                                                                        /*  (LW_CFG_SDCARD_EN > 0)      */
#endif                                                                  /*  SDHCI_H                     */
/*********************************************************************************************************
  END
*********************************************************************************************************/
