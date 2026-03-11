/**
 * mq135.c
 * MQ-135空气质量传感器驱动文件
 * 功能：实现MQ-135传感器的初始化和数据读取
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：辰哥单片机设计
 * 日期：2024.8.23
 * 项目：蔬菜恒温库监控系统
 */

#include "mq135.h"

/**
 * MQ135传感器初始化函数
 * 功能：根据工作模式初始化相应的硬件接口
 * @param 无
 * @retval 无
 * @note 根据MODE宏定义选择初始化方式：
 *       MODE=1: 初始化模拟输出引脚（AO）和ADC
 *       MODE=0: 初始化数字输出引脚（DO）
 */
void MQ135_Init(void)
{
	#if MODE
	{
		GPIO_InitTypeDef GPIO_InitStructure;
		
		// 打开ADC IO端口时钟
		RCC_APB2PeriphClockCmd(MQ135_AO_GPIO_CLK, ENABLE);
		
		// 配置ADC IO引脚模式
		GPIO_InitStructure.GPIO_Pin = MQ135_AO_GPIO_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; // 设置为模拟输入
		
		// 初始化ADC IO
		GPIO_Init(MQ135_AO_GPIO_PORT, &GPIO_InitStructure);

		// 初始化ADC
		ADCx_Init();
	}
	#else
	{
		GPIO_InitTypeDef GPIO_InitStructure;
		
		// 打开连接传感器DO的单片机引脚端口时钟
		RCC_APB2PeriphClockCmd(MQ135_DO_GPIO_CLK, ENABLE);
		
		// 配置连接传感器DO的单片机引脚模式
		GPIO_InitStructure.GPIO_Pin = MQ135_DO_GPIO_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 设置为上拉输入
		
		// 初始化GPIO
		GPIO_Init(MQ135_DO_GPIO_PORT, &GPIO_InitStructure);
	}
	#endif
}

#if MODE
/**
 * MQ135 ADC读取函数
 * 功能：读取MQ135传感器的ADC值
 * @param 无
 * @retval uint16_t ADC转换值
 */
uint16_t MQ135_ADC_Read(void)
{
	// 设置指定ADC的规则组通道，采样时间
	return ADC_GetValue(ADC_CHANNEL, ADC_SampleTime_55Cycles5);
}
#endif

/**
 * 获取MQ135传感器数据
 * 功能：读取MQ135传感器的原始数据
 * @param 无
 * @retval uint16_t 传感器原始数据
 * @note 根据MODE宏定义选择读取方式：
 *       MODE=1: 读取ADC值并取平均值
 *       MODE=0: 读取数字引脚状态
 */
uint16_t MQ135_GetData(void)
{
	#if MODE
	uint32_t tempData = 0;
	
	// 多次采样取平均值以提高精度
	for (uint8_t i = 0; i < MQ135_READ_TIMES; i++)
	{
		tempData += MQ135_ADC_Read();
		delay_ms(5); // 短暂延时
	}

	tempData /= MQ135_READ_TIMES; // 计算平均值
	return tempData;
	
	#else
	// 读取数字输出引脚状态
	uint16_t tempData;
	tempData = !GPIO_ReadInputDataBit(MQ135_DO_GPIO_PORT, MQ135_DO_GPIO_PIN);
	return tempData;
	#endif
}

/**
 * 获取MQ135传感器PPM值
 * 功能：将传感器原始数据转换为PPM浓度值
 * @param 无
 * @retval float PPM浓度值
 * @note 仅在MODE=1时有效
 */
float MQ135_GetData_PPM(void)
{
	#if MODE
	float tempData = 0;
	
	// 多次采样取平均值以提高精度
	for (uint8_t i = 0; i < MQ135_READ_TIMES; i++)
	{
		tempData += MQ135_ADC_Read();
		delay_ms(5); // 短暂延时
	}
	tempData /= MQ135_READ_TIMES; // 计算平均值
	
	// 计算电压值（假设参考电压为5V）
	float Vol = (tempData * 5.0) / 4096.0;
	
	// 计算传感器电阻值
	// RS = (Vc - Vout) / (Vout * RL)
	// 其中RL为负载电阻，这里假设为0.5KΩ
	float RS = (5.0 - Vol) / (Vol * 0.5);
	
	// 传感器在洁净空气中的电阻值（需要校准）
	float R0 = 6.64;
	
	// 根据MQ-135传感器特性曲线计算PPM值
	// ppm = pow((R0/RS) * 11.5428, 0.6549)
	float ppm = pow(11.5428 * R0 / RS, 0.6549f);
	
	return ppm;
	#endif
}
