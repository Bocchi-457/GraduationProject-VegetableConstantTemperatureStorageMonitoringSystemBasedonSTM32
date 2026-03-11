/**
 * PWM.c
 * PWM驱动实现文件
 * 功能：实现PWM初始化和占空比设置函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 * 作者：蔬菜恒温库监控系统开发团队
 * 创建日期：2026-03-08
 * 项目：蔬菜恒温库监控系统
 */

#include "PWM.h" 

/**
 * @brief 初始化PWM
 * @param 无
 * @return 无
 * @note 使用TIM3的通道1和通道2，对应PA6和PA7引脚
 *       PWM频率：72MHz / 720 / 100 = 1kHz
 */
void PWM_Init(void)
{
	// 使能TIM3时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	// 使能GPIOA时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	// 以下为引脚重映射代码，当前未使用
	// RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	// GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM3, ENABLE);
	// GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	
	// 配置PA6和PA7为复用推挽输出
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;      // 复用推挽输出
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;    // PA6和PA7
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;    // 速度50MHz
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// 使用TIM3内部时钟
	TIM_InternalClockConfig(TIM3);
	
	// 初始化TIM3时基
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;   // 时钟分频
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;  // 向上计数模式
	TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;           // 自动重装载值(ARR)
	TIM_TimeBaseInitStructure.TIM_Prescaler = 720 - 1;         // 预分频器(PSC)
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;       // 重复计数器
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
	
	// 初始化TIM3输出比较
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);                  // 初始化结构体
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;        // PWM模式1
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High; // 输出极性高
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; // 输出使能
	TIM_OCInitStructure.TIM_Pulse = 0;                       // 初始比较值(CCR)
    
	// 初始化通道1
	TIM_OC1Init(TIM3, &TIM_OCInitStructure);
	// 初始化通道2
	TIM_OC2Init(TIM3, &TIM_OCInitStructure);
	
	// 使能TIM3
	TIM_Cmd(TIM3, ENABLE);
}

/**
 * @brief 设置TIM3通道1的比较值（占空比）
 * @param Compare: 比较值，范围0-99
 * @return 无
 * @note 占空比 = Compare / 100
 */
void PWM_SetCompare1(uint16_t Compare)
{
	// 设置通道1的比较值
	TIM_SetCompare1(TIM3, Compare);
}

/**
 * @brief 设置TIM3通道2的比较值（占空比）
 * @param Compare: 比较值，范围0-99
 * @return 无
 * @note 占空比 = Compare / 100
 */
void PWM_SetCompare2(uint16_t Compare)
{
	// 设置通道2的比较值
	TIM_SetCompare2(TIM3, Compare);
}

/***************************** 结束 *****************************/

