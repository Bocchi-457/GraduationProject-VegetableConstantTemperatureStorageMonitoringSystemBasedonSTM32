/**
 * adcx.h
 * ADC驱动头文件
 * 功能：提供ADC初始化和数据采集功能
 * 测试硬件：STM32F103RCT6
 */

#ifndef _ADCX_H_
#define _ADCX_H_

#include "stm32f10x.h"                  // Device header

/**
 * @brief ADC 编号选择
 * @note 可以是 ADC1/2/3
 */
#define    ADCx                          ADC1  // 选择ADC1
#define    ADC_CLK                       RCC_APB2Periph_ADC1  // ADC1时钟

/**
 * @brief ADC初始化函数
 * @param 无
 * @return 无
 * @note 初始化ADC，配置为独立模式，单次转换，非扫描模式
 */
void ADCx_Init(void);

/**
 * @brief 获取ADC转换后的数据
 * @param ADC_Channel: 选择需要采集的ADC通道
 * @param ADC_SampleTime: 选择需要采样时间
 * @return 返回转换后的模拟信号数值
 */
u16 ADC_GetValue(uint8_t ADC_Channel,uint8_t ADC_SampleTime);

#endif

/***************************** 结束 *****************************/

