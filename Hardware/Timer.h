/**
 * Timer.h
 * 定时器驱动头文件
 * 功能：提供定时器初始化函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 */

#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f10x_tim.h"
#include "stm32f10x_rcc.h"

extern char TIMER_IT;     //定时器中断标志

/**
 * @brief 初始化定时器
 * @param 无
 * @return 无
 * @note 初始化TIM2定时器，配置为2秒中断一次
 */
void Timer_Init(void);
void TIM2_IRQHandler(void);

#endif

/***************************** 结束 *****************************/
