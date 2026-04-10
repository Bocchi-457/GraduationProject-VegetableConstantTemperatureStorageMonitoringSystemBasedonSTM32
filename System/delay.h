/**
 * delay.h
 * 延时函数头文件
 * 功能：提供微秒、毫秒和秒级别的延时函数
 * 测试硬件：STM32F103RCT6
 */

#ifndef __DELAY_H
#define __DELAY_H

#include "stdint.h"

/**
 * @brief 微秒级延时函数
 * @param us: 延时时间，单位为微秒
 * @return 无
 */
void delay_us(uint32_t us);

/**
 * @brief 毫秒级延时函数
 * @param ms: 延时时间，单位为毫秒
 * @return 无
 */
void delay_ms(uint32_t ms);

/**
 * @brief 秒级延时函数
 * @param s: 延时时间，单位为秒
 * @return 无
 */
void delay_s(uint32_t s);

#endif

/***************************** 结束 *****************************/
