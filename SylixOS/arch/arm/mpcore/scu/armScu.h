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
** 文   件   名: armScu.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2014 年 01 月 03 日
**
** 描        述: ARM SNOOP CONTROL UNIT.
*********************************************************************************************************/

#ifndef __ARMSCU_H
#define __ARMSCU_H

/*********************************************************************************************************
  寄存器定义
*********************************************************************************************************/

#define SCU_CTL                 0x00                                    /* SCU Control Register         */
#define SCU_CFG                 0x04                                    /* SCU Configuration Register   */
#define SCU_CPU_PWR_STS         0x08                                    /* SCU CPU Power Status         */
#define SCU_INV_ALL_REG         0x0c                                    /* SCU Invalidate All Registers */
                                                                        /* in Secure State              */
#define SCU_FILTER_START        0x40                                    /* Filtering Start Address      */
#define SCU_FILTER_END          0x44                                    /* Filtering End Address        */
#define SCU_ACCESS_CONTROL      0x50                                    /* SCU Access Control           */
#define SCU_NS_ACCESS_CONTROL   0x54                                    /* SCU Non-Secure Access Control*/

#endif                                                                  /*  __ARMSCU_H                  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
