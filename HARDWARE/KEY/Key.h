/**
 * Key.h
 * 按键驱动头文件
 * 功能：定义按键相关的宏和函数声明
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-08
 */

#ifndef __KEY_H
#define __KEY_H

#include "sys.h"

/**
 * 按键初始化函数
 * 功能：初始化按键对应的GPIO引脚
 * @param 无
 * @retval 无
 * @note 使用GPIOB的4、5、6、7引脚作为按键输入
 */
void Key_Init(void);

/**
 * 按键扫描函数
 * 功能：扫描按键状态，返回按键值
 * @param mode 扫描模式：0-不支持连续按，1-支持连续按
 * @retval 0-无按键按下
 * @retval KEY1_PRES-KEY1按键按下
 * @retval KEY2_PRES-KEY2按键按下
 * @retval KEY3_PRES-KEY3按键按下
 * @retval KEY4_PRES-KEY4按键按下
 * @note 按键优先级：KEY1 > KEY2 > KEY3 > KEY4
 */
u8 KEY_Scan(u8 mode);   	// 按键扫描函数		

/**
 * 按键引脚定义
 */
#define KEY1  PBin(4)  // 读取按键1 (PB4)
#define KEY2  PBin(5)  // 读取按键2 (PB5)
#define KEY3  PBin(6)  // 读取按键3 (PB6)
#define KEY4  PBin(7)  // 读取按键4 (PB7)

/**
 * 按键返回值定义
 */
#define KEY1_PRES 	1	// KEY1按下
#define KEY2_PRES 	2	// KEY2按下
#define KEY3_PRES 	3	// KEY3按下
#define KEY4_PRES 	4	// KEY4按下
		  
#endif
