/**
 * Light.h
 * 光照传感器驱动头文件
 * 功能：定义光照传感器相关的宏和函数声明
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-08
 */

#ifndef __LIGHT_H
#define __LIGHT_H

#include "stm32f10x.h"                  // Device header

/**
 * 光照传感器 ADC 引脚定义 (PA0)
 */
#define		Light_AO_GPIO_CLK			RCC_APB2Periph_GPIOA  // 光照传感器GPIO时钟
#define  	Light_AO_GPIO_PORT		    GPIOA               // 光照传感器GPIO端口
#define  	Light_AO_GPIO_PIN			GPIO_Pin_0           // 光照传感器GPIO引脚
#define     Light_ADC_CHANNEL           ADC_Channel_0       // ADC 通道宏定义

/**
 * 函数声明
 */

/**
 * 光照传感器初始化函数
 * 功能：初始化光照传感器的GPIO和ADC
 * @param 无
 * @retval 无
 */
void Light_Init(void);

/**
 * 光照传感器ADC值获取函数
 * 功能：获取光照传感器的ADC转换值
 * @param ADC_Channel ADC通道号
 * @retval uint16_t ADC转换值
 */
uint16_t Light_AD_GetValue(uint8_t ADC_Channel);

/**
 * ADC值获取函数
 * 功能：获取指定ADC通道的转换值
 * @param ADC_Channel ADC通道号
 * @retval uint16_t ADC转换值
 */
uint16_t AD_GetValue(uint8_t ADC_Channel);

/**
 * 光照值转换函数
 * 功能：将ADC值转换为实际光照值
 * @param 无
 * @retval float 光照值
 */
float Light_Value_Conversion(void);

#endif
