/**
 * AD.c
 * ADC模数转换驱动文件
 * 功能：实现ADC初始化和ADC值读取
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-07
 */

#include "stm32f10x.h"                  // Device header
#include "stm32f10x_adc.h"

/**
 * ADC初始化函数
 * 功能：初始化ADC1和相关GPIO
 * @param 无
 * @retval 无
 * @note 使用GPIOA的4、5、6引脚作为ADC输入通道
 */
void AD_Init(void)
{
  	GPIO_InitTypeDef GPIO_InitStructure;
  	ADC_InitTypeDef ADC_InitStructure;
   
	// 使能ADC1和GPIOA时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	// 配置ADC时钟为PCLK2的1/6，即12MHz
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);
	
	// 初始化GPIO为模拟输入模式
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;          // 模拟输入模式
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;  // PA4、PA5、PA6
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // GPIO速度50MHz
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// 初始化ADC1
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;               // 独立模式
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;           // 数据右对齐
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;  // 无外部触发
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;              // 单次转换模式
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;                    // 非扫描模式
	ADC_InitStructure.ADC_NbrOfChannel = 1;                          // 1个通道
	ADC_Init(ADC1, &ADC_InitStructure);
	
	// 启用ADC1
	ADC_Cmd(ADC1, ENABLE);
	
	// 校准ADC
	ADC_ResetCalibration(ADC1);                          // 复位校准
	while (ADC_GetResetCalibrationStatus(ADC1) == SET);  // 等待复位校准完成
	ADC_StartCalibration(ADC1);                          // 开始校准
	while (ADC_GetCalibrationStatus(ADC1) == SET);       // 等待校准完成
}

/**
 * 获取ADC转换值函数
 * 功能：获取指定通道的ADC转换值
 * @param ADC_Channel ADC通道号（0-15）
 * @retval uint16_t ADC转换值（0-4095）
 */
uint16_t AD_GetValue(uint8_t ADC_Channel)
{
	// 配置规则通道
	ADC_RegularChannelConfig(ADC1, ADC_Channel, 1, ADC_SampleTime_55Cycles5);
	// 软件启动转换
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	// 等待转换完成
	while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
	// 返回转换值
	return ADC_GetConversionValue(ADC1);
}