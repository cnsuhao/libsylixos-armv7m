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
** 文   件   名: KernelObject.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 14 日
**
** 描        述: 内核对象.

** BUG:
2012.03.30  将所有内核对象操作合并在这个文件.
2012.12.07  API_ObjectIsGlobal() 使用资源管理器判断.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
** 函数名称: API_ObjectGetClass
** 功能描述: 获得对象类型
** 输　入  : 
** 输　出  : CLASS
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
UINT8  API_ObjectGetClass (LW_OBJECT_HANDLE  ulId)
{
    REGISTER UINT8    ucClass;

#if LW_CFG_ARG_CHK_EN > 0
    if (!ulId) {
        _ErrorHandle(ERROR_KERNEL_OBJECT_NULL);
        return  (0);
    }
#endif
    
    ucClass = (UINT8)_ObjectGetClass(ulId);
    
    _ErrorHandle(ERROR_NONE);
    return  (ucClass);
}
/*********************************************************************************************************
** 函数名称: API_ObjectIsGlobal
** 功能描述: 获得对象是否为全局对象
** 输　入  : 
** 输　出  : 为全局对象，返回：LW_TRUE  否则，返回：LW_FALSE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
BOOL  API_ObjectIsGlobal (LW_OBJECT_HANDLE  ulId)
{
#if LW_CFG_MODULELOADER_EN > 0
    return  (__resHandleIsGlobal(ulId));
#else
    return  (LW_TRUE);
#endif                                                                  /*  LW_CFG_MODULELOADER_EN > 0  */
}
/*********************************************************************************************************
** 函数名称: API_ObjectGetNode
** 功能描述: 获得对象所在的处理器号
** 输　入  : 
** 输　出  : 处理器号
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_ObjectGetNode (LW_OBJECT_HANDLE  ulId)
{
    REGISTER ULONG    ulNode;
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!ulId) {
        _ErrorHandle(ERROR_KERNEL_OBJECT_NULL);
        return  ((unsigned)(PX_ERROR));
    }
#endif

    ulNode = _ObjectGetNode(ulId);
    
    _ErrorHandle(ERROR_NONE);
    return  (ulNode);
}
/*********************************************************************************************************
** 函数名称: API_ObjectGetIndex
** 功能描述: 获得对象缓冲区内地址
** 输　入  : 
** 输　出  : 获得对象缓冲区内地址
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
ULONG  API_ObjectGetIndex (LW_OBJECT_HANDLE  ulId)
{
    REGISTER ULONG    ulIndex;
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!ulId) {
        _ErrorHandle(ERROR_KERNEL_OBJECT_NULL);
        return  ((unsigned)(PX_ERROR));
    }
#endif

    ulIndex = _ObjectGetIndex(ulId);
    
    _ErrorHandle(ERROR_NONE);
    return  (ulIndex);
}
/*********************************************************************************************************
  END
*********************************************************************************************************/


