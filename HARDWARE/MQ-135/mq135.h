/**
 * mq135.h
 * MQ-135空气质量传感器头文件
 * 功能：定义MQ-135传感器相关的宏和函数声明
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：辰哥单片机设计
 * 日期：2024.8.23
 * 项目：蔬菜恒温库监控系统
 */

#ifndef __MQ135_H
#define	__MQ135_H

#include "stm32f10x.h"
#include "adcx.h"
#include "delay.h"
#include "math.h"

/**
 * MQ-135传感器读取配置
 */
#define MQ135_READ_TIMES	10  // MQ-135传感器ADC循环读取次数

/**
 * 模式选择
 * 1: 模拟AO模式（使用ADC读取）
 * 0: 数字DO模式（使用GPIO读取）
 */
#define	MODE 	1

/**
 * MQ-135 GPIO宏定义
 * 根据选择的模式配置不同的引脚
 */
#if MODE
// 模拟AO模式配置
#define		MQ135_AO_GPIO_CLK					RCC_APB2Periph_GPIOA  // MQ-135模拟输出GPIO时钟
#define  	MQ135_AO_GPIO_PORT				GPIOA               // MQ-135模拟输出GPIO端口
#define  	MQ135_AO_GPIO_PIN				GPIO_Pin_4           // MQ-135模拟输出GPIO引脚
#define   ADC_CHANNEL                		ADC_Channel_4       // ADC通道宏定义

#else
// 数字DO模式配置
#define		MQ135_DO_GPIO_CLK					RCC_APB2Periph_GPIOA  // MQ-135数字输出GPIO时钟
#define  	MQ135_DO_GPIO_PORT				GPIOA               // MQ-135数字输出GPIO端口
#define  	MQ135_DO_GPIO_PIN				GPIO_Pin_1           // MQ-135数字输出GPIO引脚

#endif

/**
 * 函数声明
 */

/**
 * MQ-135传感器初始化函数
 * 功能：初始化MQ-135传感器的GPIO和ADC
 * @param 无
 * @retval 无
 */
void MQ135_Init(void);

/**
 * 获取MQ-135传感器数据
 * 功能：读取MQ-135传感器的原始数据
 * @param 无
 * @retval uint16_t 传感器原始数据
 */
uint16_t MQ135_GetData(void);

/**
 * 获取MQ-135传感器PPM值
 * 功能：将传感器原始数据转换为PPM浓度值
 * @param 无
 * @retval float PPM浓度值
 */
float MQ135_GetData_PPM(void);

#endif /* __MQ135_H */

