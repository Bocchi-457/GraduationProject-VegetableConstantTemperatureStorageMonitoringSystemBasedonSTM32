/**
 * led.h
 * LED驱动头文件
 * 功能：定义LED、蜂鸣器和水泵的控制引脚
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-08
 */

#ifndef __LED_H
#define __LED_H

#include "sys.h"

/**
 * 外设引脚定义
 */
#define BEEP         PAout(8)   	// 蜂鸣器 (PA8)
#define Water_pump   PBout(11)   // 水泵 (PB11)
#define LED          PBout(15)   // 补光灯 (PB15)

/**
 * LED初始化函数
 * 功能：初始化LED、蜂鸣器和水泵的GPIO引脚
 * @param 无
 * @retval 无
 */
void LED_Init(void);

#endif

