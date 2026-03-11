/**
 * PWM.h
 * PWM驱动头文件
 * 功能：提供PWM初始化和占空比设置函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 * 作者：蔬菜恒温库监控系统开发团队
 * 创建日期：2026-03-08
 * 项目：蔬菜恒温库监控系统
 */

#ifndef __PWM_H
#define __PWM_H

#include "stm32f10x.h"                  // Device header

/**
 * @brief 初始化PWM
 * @param 无
 * @return 无
 * @note 使用TIM3的通道1和通道2，对应PA6和PA7引脚
 */
void PWM_Init(void);

/**
 * @brief 设置TIM3通道1的比较值（占空比）
 * @param Compare: 比较值，范围0-99
 * @return 无
 */
void PWM_SetCompare1(uint16_t Compare);

/**
 * @brief 设置TIM3通道2的比较值（占空比）
 * @param Compare: 比较值，范围0-99
 * @return 无
 */
void PWM_SetCompare2(uint16_t Compare);

#endif

/***************************** 结束 *****************************/
