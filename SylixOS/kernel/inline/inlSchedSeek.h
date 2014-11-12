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
** 文   件   名: inlSchedSeek.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 14 日
**
** 描        述: 这是系统调度器查询有就绪线程的最高优先级内联函数
*********************************************************************************************************/

#ifndef  __INLSCHEDSEEK_H
#define  __INLSCHEDSEEK_H

/*********************************************************************************************************
  调度器查询有就绪线程的最高优先级内联函数
*********************************************************************************************************/

static LW_INLINE PLW_CLASS_PCBBMAP  _SchedSeekPriority (PLW_CLASS_CPU  pcpu, UINT8 *ucPriority)
{
#if LW_CFG_SMP_EN > 0
    UINT8               ucLocal, ucGlobal;
    PLW_CLASS_PCBBMAP   ppcbbmap;
    
    if (_BitmapIsEmpty(LW_CPU_RDY_BMAP(pcpu))) {
        ppcbbmap    = LW_GLOBAL_RDY_PCBBMAP();
        ucGlobal    = _BitmapHigh(LW_GLOBAL_RDY_BMAP());
        *ucPriority = ucGlobal;
    
    } else if (_BitmapIsEmpty(LW_GLOBAL_RDY_BMAP())) {
        ppcbbmap    = LW_CPU_RDY_PCBBMAP(pcpu);
        ucGlobal    = _BitmapHigh(LW_CPU_RDY_BMAP(pcpu));
        *ucPriority = ucGlobal;
        
    } else {
        ucLocal  = _BitmapHigh(LW_CPU_RDY_BMAP(pcpu));
        ucGlobal = _BitmapHigh(LW_GLOBAL_RDY_BMAP());
        if (LW_PRIO_IS_HIGH(ucLocal, ucGlobal)) {
            ppcbbmap    = LW_CPU_RDY_PCBBMAP(pcpu);
            *ucPriority = ucLocal;
        
        } else {
            ppcbbmap    = LW_GLOBAL_RDY_PCBBMAP();
            *ucPriority = ucGlobal;
        }
    }
    
    return  (ppcbbmap);
#else
    *ucPriority = _BitmapHigh(LW_GLOBAL_RDY_BMAP());
    
    return  (LW_GLOBAL_RDY_PCBBMAP());
#endif                                                                  /*  LW_CFG_SMP_EN > 0           */
}

#endif                                                                  /*  __INLSCHEDSEEK_H            */
/*********************************************************************************************************
  END
*********************************************************************************************************/
