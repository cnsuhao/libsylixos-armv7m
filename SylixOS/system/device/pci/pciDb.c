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
** 文   件   名: pciDb.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 09 月 30 日
**
** 描        述: PCI 总线信息数据库.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_PCI_EN > 0)
/*********************************************************************************************************
  总线信息结构
*********************************************************************************************************/
typedef struct {
    UINT8               CP_ucProgIf;
    PCHAR               CS_pcProgIfName;
} PCI_CLASS_PROGIF;

typedef struct {
    UINT16              CS_ucSubClass;                                  /*  high 8 bit is class code    */
    PCHAR               CS_pcSubName;
    PCI_CLASS_PROGIF   *CS_pcpTable;
    UINT16              CS_usTableNum;
} PCI_CLASS_SUB;

typedef struct {
    UINT8               CC_ucClassCode;
    PCHAR               CC_pcClassName;
    PCI_CLASS_SUB      *CC_pcsTable;
    UINT16              CC_usTableNum;
} PCI_CLASS_CODE;
/*********************************************************************************************************
  总线 class 信息
*********************************************************************************************************/
static PCI_CLASS_CODE   _G_ccPciClassTable[] = {
    {PCI_CLASS_NOT_DEFINED,             "Pre 2.0",              LW_NULL,    1},
    {PCI_BASE_CLASS_STORAGE,            "Storage",              LW_NULL,    5},
    {PCI_BASE_CLASS_NETWORK,            "Network",              LW_NULL,    4},
    {PCI_BASE_CLASS_DISPLAY,            "Display",              LW_NULL,    3},
    {PCI_BASE_CLASS_MULTIMEDIA,         "Multimedia",           LW_NULL,    3},
    {PCI_BASE_CLASS_MEMORY,             "Memory",               LW_NULL,    2},
    {PCI_BASE_CLASS_BRIDGE,             "Bridge",               LW_NULL,    9},
    {PCI_BASE_CLASS_COMMUNICATION,      "Communication",        LW_NULL,    4},
    {PCI_BASE_CLASS_SYSTEM,             "System",               LW_NULL,    5},
    {PCI_BASE_CLASS_INPUT,              "Input",                LW_NULL,    5},
    {PCI_BASE_CLASS_DOCKING,            "Docking",              LW_NULL,    1},
    {PCI_BASE_CLASS_PROCESSOR,          "Processor",            LW_NULL,    7},
    {PCI_BASE_CLASS_SERIAL,             "Serial",               LW_NULL,    5},
    {PCI_BASE_CLASS_INTELLIGENT,        "Intelligent",          LW_NULL,    1},
    {PCI_BASE_CLASS_SATELLITE,          "Satellite",            LW_NULL,    4},
    {PCI_BASE_CLASS_CRYPT,              "Crypt",                LW_NULL,    2},
    {PCI_BASE_CLASS_SIGNAL_PROCESSING,  "Signal processing",    LW_NULL,    1},
};
/*********************************************************************************************************
  总线 sub class 信息
*********************************************************************************************************/
static PCI_CLASS_SUB    _G_ccPciSubClassTable[] = {
    {PCI_CLASS_NOT_DEFINED_VGA, "VGA",     LW_NULL, 0},

    {PCI_CLASS_STORAGE_SCSI,    "SCSI",    LW_NULL, 0},
    {PCI_CLASS_STORAGE_IDE,     "IDE",     LW_NULL, 5},
    {PCI_CLASS_STORAGE_FLOPPY,  "FLOPPY",  LW_NULL, 0},
    {PCI_CLASS_STORAGE_IPI,     "IPI",     LW_NULL, 0},
    {PCI_CLASS_STORAGE_RAID,    "RAID",    LW_NULL, 0},


    {PCI_CLASS_NETWORK_ETHERNET,    "ETHERNET",     LW_NULL, 0},
    {PCI_CLASS_NETWORK_TOKEN_RING,  "TOKEN_RING",   LW_NULL, 0},
    {PCI_CLASS_NETWORK_FDDI,        "FDDI",         LW_NULL, 0},
    {PCI_CLASS_NETWORK_ATM,         "ATM",          LW_NULL, 0},


    {PCI_CLASS_DISPLAY_VGA,     "VGA",      LW_NULL, 2},
    {PCI_CLASS_DISPLAY_XGA,     "XGA",      LW_NULL, 0},
    {PCI_CLASS_DISPLAY_3D,      "3D",       LW_NULL, 0},


    {PCI_CLASS_MULTIMEDIA_VIDEO,    "VIDEO",    LW_NULL, 0},
    {PCI_CLASS_MULTIMEDIA_AUDIO,    "AUDIO",    LW_NULL, 0},
    {PCI_CLASS_MULTIMEDIA_PHONE,    "PHONE",    LW_NULL, 0},


    {PCI_CLASS_MEMORY_RAM,      "RAM",      LW_NULL, 0},
    {PCI_CLASS_MEMORY_FLASH,    "FLASH",    LW_NULL, 0},


    {PCI_CLASS_BRIDGE_HOST,     "Host/PCI",             LW_NULL, 0},
    {PCI_CLASS_BRIDGE_ISA,      "PCI/ISA",              LW_NULL, 0},
    {PCI_CLASS_BRIDGE_EISA,     "PCI/EISA",             LW_NULL, 0},
    {PCI_CLASS_BRIDGE_MC,       "PCI/Micro Channel",    LW_NULL, 0},
    {PCI_CLASS_BRIDGE_PCI,      "PCI/PCI",              LW_NULL, 0},
    {PCI_CLASS_BRIDGE_PCMCIA,   "PCI/PCMCIA",           LW_NULL, 0},
    {PCI_CLASS_BRIDGE_NUBUS,    "PCI/NuBus",            LW_NULL, 0},
    {PCI_CLASS_BRIDGE_CARDBUS,  "PCI/CardBus",          LW_NULL, 0},
    {PCI_CLASS_BRIDGE_RACEWAY,  "PCI/RaceWay",          LW_NULL, 0},


    {PCI_CLASS_COMMUNICATION_SERIAL,        "SERIAL",       LW_NULL, 3},
    {PCI_CLASS_COMMUNICATION_PARALLEL,      "PARALLEL",     LW_NULL, 3},
    {PCI_CLASS_COMMUNICATION_MULTISERIAL,   "MULTISERIAL",  LW_NULL, 0},
    {PCI_CLASS_COMMUNICATION_MODEM,         "MODEM",        LW_NULL, 0},


    {PCI_CLASS_SYSTEM_PIC,          "PIC",      LW_NULL, 3},
    {PCI_CLASS_SYSTEM_DMA,          "DMA",      LW_NULL, 3},
    {PCI_CLASS_SYSTEM_TIMER,        "TIMER",    LW_NULL, 3},
    {PCI_CLASS_SYSTEM_RTC,          "RTC",      LW_NULL, 2},
    {PCI_CLASS_SYSTEM_PCI_HOTPLUG,  "HOTPLUG",  LW_NULL, 0},


    {PCI_CLASS_INPUT_KEYBOARD,  "KEYBOARD", LW_NULL, 0},
    {PCI_CLASS_INPUT_PEN,       "PEN",      LW_NULL, 0},
    {PCI_CLASS_INPUT_MOUSE,     "MOUSE",    LW_NULL, 0},
    {PCI_CLASS_INPUT_SCANNER,   "SCANNER",  LW_NULL, 0},
    {PCI_CLASS_INPUT_GAMEPORT,  "GAMEPORT", LW_NULL, 0},


    {PCI_CLASS_DOCKING_GENERIC, "GENERIC",  LW_NULL, 0},


    {PCI_CLASS_PROCESSOR_386,       "386",          LW_NULL, 0},
    {PCI_CLASS_PROCESSOR_486,       "486",          LW_NULL, 0},
    {PCI_CLASS_PROCESSOR_PENTIUM,   "PENTIUM",      LW_NULL, 0},
    {PCI_CLASS_PROCESSOR_ALPHA,     "ALPHA",        LW_NULL, 0},
    {PCI_CLASS_PROCESSOR_POWERPC,   "POWERPC",      LW_NULL, 0},
    {PCI_CLASS_PROCESSOR_MIPS,      "MIPS",         LW_NULL, 0},
    {PCI_CLASS_PROCESSOR_CO,        "Co-Processor", LW_NULL, 0},


    {PCI_CLASS_SERIAL_FIREWIRE, "Firewire (IEEE 1394)",                 LW_NULL, 0},
    {PCI_CLASS_SERIAL_ACCESS,   "ACCESS bus",                           LW_NULL, 0},
    {PCI_CLASS_SERIAL_SSA,      "SSA (Serial Storage Architecture)",    LW_NULL, 0},
    {PCI_CLASS_SERIAL_USB,      "USB",                                  LW_NULL, 0},
    {PCI_CLASS_SERIAL_FIBER,    "FIBER",                                LW_NULL, 0},


    {PCI_CLASS_INTELLIGENT_I2O, "I2O",      LW_NULL, 0},


    {PCI_CLASS_SATELLITE_TV,    "TV",       LW_NULL, 0},
    {PCI_CLASS_SATELLITE_AUDIO, "AUDIO",    LW_NULL, 0},
    {PCI_CLASS_SATELLITE_VOICE, "VOICE",    LW_NULL, 0},
    {PCI_CLASS_SATELLITE_DATA,  "DATA",     LW_NULL, 0},


    {PCI_CLASS_CRYPT_NETWORK,       "NETWORK",          LW_NULL, 0},
    {PCI_CLASS_CRYPT_ENTERTAINMENT, "ENTERTAINMENT",    LW_NULL, 0},


    {PCI_CLASS_SP_DPIO, "SP-DPIO",  LW_NULL, 0},
};
/*********************************************************************************************************
  总线 prog if 信息
*********************************************************************************************************/
static PCI_CLASS_PROGIF _G_ccPciProgIfTable[] = {
    {0,     "Operating mode (primary)"},
    {1,     "Programmable indicator (primary)"},
    {2,     "Operating mode (secondary)"},
    {3,     "Programmable indicator (secondary)"},
    {7,     "Master IDE device"},

    {0,     "VGA compatable controller"},
    {1,     "8514 compatable"},

    {0,     "Generic XT compatable serial controller"},
    {1,     "16450 compatable serial controller"},
    {2,     "16550 compatable serial controller"},

    {0,     "Parallel port"},
    {1,     "Bi-directional parallel port"},
    {2,     "ECP 1.X parallel port"},

    {0,     "Generic 8259 programmable interrupt controller"},
    {1,     "ISA PIC"},
    {2,     "EISA PIC"},

    {0,     "Generic 8237 DMA controller"},
    {1,     "ISA DMA controller"},
    {2,     "EISA DMA controller"},

    {0,     "Generic 8254 timer"},
    {1,     "ISA system timer"},
    {2,     "EISA system timer"},

    {0,     "Generic RTC controller"},
    {1,     "ISA RTC controller"},
};
/*********************************************************************************************************
  PCI 事例类型根节点
*********************************************************************************************************/
static INT  _G_iClassTableNum;
/*********************************************************************************************************
** 函数名称: __pciDbInit
** 功能描述: 初始化 pci 总线信息数据库
** 输　入  : NONE
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID  __pciDbInit (VOID)
{
    INT     i, j;
    INT     iClassMax = sizeof(_G_ccPciClassTable) / sizeof(PCI_CLASS_CODE);
    
    PCI_CLASS_CODE      *pcc   = _G_ccPciClassTable;
    PCI_CLASS_SUB       *pcs   = _G_ccPciSubClassTable;
    PCI_CLASS_PROGIF    *pcpif = _G_ccPciProgIfTable;
    
    for (i = 0; i < iClassMax; i++) {
        if (pcc[i].CC_usTableNum) {
            pcc[i].CC_pcsTable = pcs;
            
            for (j = 0; j < pcc[i].CC_usTableNum; j++) {
                if (pcs[j].CS_usTableNum) {
                    pcs[j].CS_pcpTable = pcpif;
                    
                    pcpif += pcs[j].CS_usTableNum;
                }
            }
            pcs += pcc[i].CC_usTableNum;
        }
    }
    
    _G_iClassTableNum = iClassMax;
}
/*********************************************************************************************************
** 函数名称: __pciDbGetClassStr
** 功能描述: 通过类型查询 PCI 事例信息
** 输　入  : ucClass       类型信息
**           ucSubClass    子类型
**           ucProgIf      prog if
**           pcBuffer      输出缓冲
**           stSize        缓冲区大小
** 输　出  : NONE
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID  __pciDbGetClassStr (UINT8 ucClass, UINT8 ucSubClass, UINT8 ucProgIf, PCHAR pcBuffer, size_t  stSize)
{
    INT                  i, j, k;
    PCHAR                pcClass = "", pcSubClass = "", pcProgIf = "";
    PCI_CLASS_CODE      *pcc;
    PCI_CLASS_SUB       *pcs;
    PCI_CLASS_PROGIF    *pcpif;
    UINT16               usClassSub = (UINT16)((ucClass << 8) + ucSubClass);
    
    for (i = 0; i < _G_iClassTableNum; i++) {
        pcc = &_G_ccPciClassTable[i];
        
        if (pcc->CC_ucClassCode == ucClass) {
            pcClass = pcc->CC_pcClassName;
            
            for (j = 0; j < pcc->CC_usTableNum; j++) {
                pcs = &pcc->CC_pcsTable[j];
                
                if (pcs->CS_ucSubClass == usClassSub) {
                    pcSubClass = pcs->CS_pcSubName;
                    
                    for (k = 0; k < pcs->CS_usTableNum; k++) {
                        pcpif = &pcs->CS_pcpTable[k];
                    
                        if (pcpif->CP_ucProgIf == ucProgIf) {
                            pcProgIf = pcpif->CS_pcProgIfName;
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
    
    bnprintf(pcBuffer, stSize, 0, 
             "Class %d [%s] Sub %d [%s] Prog-if %d [%s]", 
             ucClass, pcClass, ucSubClass, pcSubClass, ucProgIf, pcProgIf);
}
#endif                                                                  /*  (LW_CFG_DEVICE_EN > 0) &&   */
                                                                        /*  (LW_CFG_PCI_EN > 0)         */
/*********************************************************************************************************
  END
*********************************************************************************************************/
