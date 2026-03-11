/**
 * AD.h
 * ADC模数转换头文件
 * 功能：声明ADC相关的函数
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-07
 */

#ifndef __AD_H
#define __AD_H

/**
 * ADC初始化函数
 * 功能：初始化ADC1和相关GPIO
 * @param 无
 * @retval 无
 * @note 使用GPIOA的4、5、6引脚作为ADC输入通道
 */
void AD_Init(void);

/**
 * 获取ADC转换值函数
 * 功能：获取指定通道的ADC转换值
 * @param ADC_Channel ADC通道号（0-15）
 * @retval uint16_t ADC转换值（0-4095）
 */
uint16_t AD_GetValue(uint8_t ADC_Channel);

#endif