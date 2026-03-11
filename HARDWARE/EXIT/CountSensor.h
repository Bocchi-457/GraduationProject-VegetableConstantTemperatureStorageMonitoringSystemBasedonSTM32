/**
 * CountSensor.h
 * 计数传感器头文件
 * 功能：声明计数传感器相关的函数
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 */

#ifndef __COUNT_SENSOR_H
#define __COUNT_SENSOR_H

/**
 * 计数传感器初始化函数
 * 功能：初始化计数传感器的GPIO、外部中断和NVIC配置
 * @param 无
 * @retval 无
 */
void CountSensor_Init(void);

/**
 * 获取计数传感器计数值函数
 * 功能：获取计数传感器的当前计数值
 * @param 无
 * @retval uint16_t 计数值
 */
uint16_t CountSensor_Get(void);

#endif
