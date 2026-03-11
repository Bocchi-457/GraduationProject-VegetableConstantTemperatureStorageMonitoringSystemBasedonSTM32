#include "stm32f10x.h"                  // Device header
#include "CountSensor.h"
#include "stm32f10x_exti.h"
uint16_t CountSensor_Count;

/**
 * 计数传感器初始化函数
 * 功能：初始化计数传感器的GPIO、外部中断和NVIC配置
 * @param 无
 * @retval 无
 * @note 使用GPIOB的11和14引脚作为计数传感器输入，上升沿触发中断
 */
void CountSensor_Init(void)
{
	// 使能GPIOB和AFIO时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	
	// 初始化GPIO
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;          // 上拉输入模式
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11|GPIO_Pin_14;  // 使用PB11和PB14引脚
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       //  GPIO速度50MHz
	GPIO_Init(GPIOB, &GPIO_InitStructure);
  
	// 配置外部中断线路
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource11);  // 配置PB11为外部中断源
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource14);  // 配置PB14为外部中断源
  
	// 初始化外部中断
	EXTI_InitTypeDef EXTI_InitStructure;
	EXTI_InitStructure.EXTI_Line = EXTI_Line11|EXTI_Line14;        // 外部中断线路11和14
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;                      // 启用外部中断
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;            // 中断模式
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;         // 上升沿触发
	EXTI_Init(&EXTI_InitStructure);
	
	// 初始化NVIC
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;           // EXTI15_10中断通道
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                // 启用中断通道
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;      // 抢占优先级0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;             // 子优先级1
	NVIC_Init(&NVIC_InitStructure);
}


