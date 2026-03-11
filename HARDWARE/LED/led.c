/**
 * led.c
 * LED驱动文件
 * 功能：实现LED、蜂鸣器和水泵的初始化
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-08
 */

#include "led.h"

/**
 * LED初始化函数
 * 功能：初始化LED、蜂鸣器和水泵的GPIO引脚
 * @param 无
 * @retval 无
 * @note 初始化PA8(蜂鸣器)、PB11(水泵)、PB15(补光灯)
 */
void LED_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
  
	// 使能GPIOA、GPIOB、GPIOC和AFIO时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
	
	// 禁用JTAG功能，释放相关引脚
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	
	// 初始化PA8 (蜂鸣器)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8; 	
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 // 推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // 速度50MHz
  	GPIO_Init(GPIOA, &GPIO_InitStructure);	  // 初始化GPIOA
 
	// 初始化PB11(水泵)和PB15(补光灯)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_15; 	
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 // 推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // 速度50MHz
  	GPIO_Init(GPIOB, &GPIO_InitStructure);	  // 初始化GPIOB
}






