/**
 * control.h
 * 控制模块头文件
 * 功能：提供各种控制设备的初始化函数和宏定义
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 */

#ifndef	__CONTROL_H
#define	__CONTROL_H

#include "stm32f10x.h"                  // Device header

/**
 * @brief 蜂鸣器初始化函数
 * @param 无
 * @return 无
 * @note 初始化蜂鸣器控制引脚
 */
void beep_init(void);

/**
 * @brief 蜂鸣器控制宏定义
 * @note PA8引脚控制蜂鸣器
 */
#define beep PAout(8) // PA8

/**
 * @brief 加热模块初始化函数
 * @param 无
 * @return 无
 * @note 初始化加热模块控制引脚
 */
void jiare_init(void);

/**
 * @brief 加热模块控制宏定义
 * @note PB10引脚控制加热模块
 */
#define jiare PBout(10) // PB10

/**
 * @brief 制冷模块初始化函数
 * @param 无
 * @return 无
 * @note 初始化制冷模块控制引脚
 */
void zhileng_init(void);

/**
 * @brief 制冷模块控制宏定义
 * @note PB11引脚控制制冷模块
 */
#define zhileng PBout(11) // PB11

/**
 * @brief 除湿模块初始化函数
 * @param 无
 * @return 无
 * @note 初始化除湿模块控制引脚
 */
void chushi_init(void); 

/**
 * @brief 除湿模块控制宏定义
 * @note PB1引脚控制除湿模块
 */
#define chushi PBout(1) // PB1

/**
 * @brief 加湿器初始化函数
 * @param 无
 * @return 无
 * @note 初始化加湿器控制引脚
 */
void jiashi_init(void);
/**
 * @brief 加湿器控制宏定义
 * @note PB0引脚控制加湿器
 */
#define jiashi PBout(0) // PB0

#endif

/***************************** 结束 *****************************/