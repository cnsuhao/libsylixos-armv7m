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
** 文   件   名: ioPowerM.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 08 月 31 日
**
** 描        述: 系统 IO 设备电源管理操作.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  加入裁剪支持
*********************************************************************************************************/
#if LW_CFG_DEVICE_EN > 0
#if (LW_CFG_POWERM_EN > 0) && (LW_CFG_MAX_POWERM_NODES > 0)
/*********************************************************************************************************
** 函数名称: _IosDevPowerMCallback
** 功能描述: 设备电源管理节点回调函数
** 输　入  : pdevpm                       设备电源管理节点
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  _IosDevPowerMCallback (PLW_DEV_PM      pdevpm)
{
    if (pdevpm) {
        if (pdevpm->DEVPM_pfuncPowerOff) {
            pdevpm->DEVPM_pfuncPowerOff(pdevpm->DEVPM_pvArgPowerOff);   /*  调用节能方法                */
        }
    }
}
/*********************************************************************************************************
** 函数名称: API_IosDevPowerMAdd
** 功能描述: 向系统中一个设备添加一个电源管理节点
** 输　入  : pcName                       设备名
**           ulMaxIdleTime                设备最长空闲时间 (ticks) (最后一次操作后, 经历这么长时间, 系统
**                                        就会将此设备进入低功耗模式)
**           pfuncPowerOff                设备进入低功耗模式回调
**           pvArgPowerOff                低功耗模式回调参数
**           pfuncPowerSignal             激活设备进入工作状态 (注意: 每一次 IO 操作都会被调用)
**           pvArgPowerSignal             激活设备进入工作状态参数
**           pfuncRemove                  系统移除此设备时调用的回调
**           pvArgRemove                  系统移除此设备回调参数
**           ppvPowerM                    电源管理点控制句柄(返回)
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_IosDevPowerMAdd (CPCHAR        pcName,
                          ULONG         ulMaxIdleTime,
                          FUNCPTR       pfuncPowerOff,
                          PVOID         pvArgPowerOff,
                          FUNCPTR       pfuncPowerSignal,
                          PVOID         pvArgPowerSignal,
                          FUNCPTR       pfuncRemove,
                          PVOID         pvArgRemove,
                          PVOID        *ppvPowerM)
{
    REGISTER PLW_DEV_HDR     pdevhdr;
             PCHAR           pcTail;
             PLW_DEV_PM      pdevpm;
             CHAR            cPowerMName[LW_CFG_OBJECT_NAME_SIZE];
    
    pdevhdr = iosDevFind(pcName, &pcTail);
    if (pdevhdr) {
        if (pcTail && (pcTail[0] == PX_EOS)) {
            pdevpm = (PLW_DEV_PM)__SHEAP_ALLOC(sizeof(LW_DEV_PM));
            if (pdevpm == LW_NULL) {
                _ErrorHandle(ERROR_SYSTEM_LOW_MEMORY);
                return  (PX_ERROR);
            }
            
            snprintf(cPowerMName, LW_CFG_OBJECT_NAME_SIZE, 
                     "%s_powerm", pcName);                              /*  确定节点名称                */
            pdevpm->DEVPM_ulPowerM = API_PowerMCreate(cPowerMName, 
                                        LW_OPTION_OBJECT_GLOBAL,
                                        LW_NULL);                       /*  创建电源管理节点            */
            if (pdevpm->DEVPM_ulPowerM == LW_OBJECT_HANDLE_INVALID) {
                __SHEAP_FREE(pdevpm);
                return  (PX_ERROR);
            }
            
            pdevpm->DEVPM_pfuncPowerOff    = pfuncPowerOff;
            pdevpm->DEVPM_pvArgPowerOff    = pvArgPowerOff;
            pdevpm->DEVPM_pfuncPowerSignal = pfuncPowerSignal;
            pdevpm->DEVPM_pvArgPowerSignal = pvArgPowerSignal;
            pdevpm->DEVPM_pfuncRemove      = pfuncRemove;
            pdevpm->DEVPM_pvArgRemove      = pvArgRemove;
            
            _IosLock();                                                 /*  进入 IO 临界区              */
            _List_Line_Add_Ahead(&pdevpm->DEVPM_lineManage,
                                 &pdevhdr->DEVHDR_plinePowerMHeader);
            _IosUnlock();                                               /*  退出 IO 临界区              */
            
            API_PowerMStart(pdevpm->DEVPM_ulPowerM, ulMaxIdleTime, 
                            _IosDevPowerMCallback, (PVOID)pdevpm);      /*  启动电源管理定时器          */
            
            if (ppvPowerM) {
                *ppvPowerM = (PVOID)pdevpm;                             /*  保存控制句柄                */
            }
            
            return  (ERROR_NONE);
        }
    }
    
    _ErrorHandle(ERROR_IOS_DEVICE_NOT_FOUND);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_IosDevPowerMDeleteAll
** 功能描述: 从系统中一个设备删除所有电源管理节点
** 输　入  : pcName                       设备名
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_IosDevPowerMDeleteAll (CPCHAR  pcName)
{
    REGISTER PLW_DEV_HDR     pdevhdr;
             PCHAR           pcTail;
             PLW_DEV_PM      pdevpm;
    
    pdevhdr = iosDevFind(pcName, &pcTail);
    if (pdevhdr) {
        if (pcTail && (pcTail[0] == PX_EOS)) {
            
            _IosLock();                                                 /*  进入 IO 临界区              */
            while (pdevhdr->DEVHDR_plinePowerMHeader) {
                pdevpm = _LIST_ENTRY(pdevhdr->DEVHDR_plinePowerMHeader, 
                                     LW_DEV_PM, 
                                     DEVPM_lineManage);
                _List_Line_Del(&pdevpm->DEVPM_lineManage,
                               &pdevhdr->DEVHDR_plinePowerMHeader);     /*  从链表中删除                */
                
                _IosUnlock();                                           /*  退出 IO 临界区              */
                API_PowerMDelete(&pdevpm->DEVPM_ulPowerM);              /*  删除电源管理节点            */
                if (pdevpm->DEVPM_pfuncRemove) {                        /*  调用移除函数                */
                    pdevpm->DEVPM_pfuncRemove(pdevpm->DEVPM_pvArgRemove);
                }
                __SHEAP_FREE(pdevpm);                                   /*  释放节点内存                */
                _IosLock();                                             /*  进入 IO 临界区              */
            }
            _IosUnlock();                                               /*  退出 IO 临界区              */
            
            return  (ERROR_NONE);
        }
    }
    
    _ErrorHandle(ERROR_IOS_DEVICE_NOT_FOUND);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_IosDevPowerMMaxIdleTimeSet
** 功能描述: 设置设备电源管理节点最长空闲周期
** 输　入  : pvPowerM                     电源管理节点
**           ulMaxIdleTime                设备最长空闲时间 (ticks) (最后一次操作后, 经历这么长时间, 系统
**                                        就会将此设备进入低功耗模式)
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_IosDevPowerMMaxIdleTimeSet (PVOID  pvPowerM, ULONG  ulMaxIdleTime)
{
    PLW_DEV_PM      pdevpm = (PLW_DEV_PM)pvPowerM;

    if (pdevpm) {
        API_PowerMCancel(pdevpm->DEVPM_ulPowerM);
        API_PowerMStart(pdevpm->DEVPM_ulPowerM, ulMaxIdleTime, 
                        _IosDevPowerMCallback, (PVOID)pdevpm);          /*  重新启动电源管理定时器      */
        return  (ERROR_NONE);
    }
    
    _ErrorHandle(EINVAL);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_IosDevPowerMMaxIdleTimeGet
** 功能描述: 获取设备电源管理节点最长空闲周期
** 输　入  : pvPowerM                     电源管理节点
**           pulMaxIdleTime               设备最长空闲时间 (ticks) (最后一次操作后, 经历这么长时间, 系统
**                                        就会将此设备进入低功耗模式)
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_IosDevPowerMMaxIdleTimeGet (PVOID  pvPowerM, ULONG  *pulMaxIdleTime)
{
    PLW_DEV_PM      pdevpm = (PLW_DEV_PM)pvPowerM;
    
    if (pdevpm && pulMaxIdleTime) {
        API_PowerMStatus(pdevpm->DEVPM_ulPowerM, LW_NULL,
                         pulMaxIdleTime,
                         LW_NULL, LW_NULL);
        return  (ERROR_NONE);
    }
    
    _ErrorHandle(EINVAL);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_IosDevPowerMCancel
** 功能描述: 停止设备电源管理节点
** 输　入  : pvPowerM                     电源管理节点
** 输　出  : ERROR CODE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
INT  API_IosDevPowerMCancel (PVOID  pvPowerM)
{
    PLW_DEV_PM      pdevpm = (PLW_DEV_PM)pvPowerM;

    if (pdevpm) {
        API_PowerMCancel(pdevpm->DEVPM_ulPowerM);
        return  (ERROR_NONE);
    }
    
    _ErrorHandle(EINVAL);
    return  (PX_ERROR);
}
/*********************************************************************************************************
** 函数名称: API_IosDevPowerMSignal
** 功能描述: 激活设备电源管理节点(表示设备正在运行, 复位电源管理定时器)
** 输　入  : pvPowerM                     电源管理节点
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
VOID  API_IosDevPowerMSignal (PVOID  pvPowerM)
{
    PLW_DEV_PM      pdevpm = (PLW_DEV_PM)pvPowerM;
    
    if (pdevpm) {
        if (pdevpm->DEVPM_ulPowerM) {
            API_PowerMSignalFast(pdevpm->DEVPM_ulPowerM);
        }
    }
}
#endif                                                                  /*  LW_CFG_POWERM_EN            */
                                                                        /*  LW_CFG_MAX_POWERM_NODES     */
#endif                                                                  /*  LW_CFG_DEVICE_EN            */
/*********************************************************************************************************
  END
*********************************************************************************************************/
