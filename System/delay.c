/**
 * delay.c
 * 延时函数库
 * 功能：提供微秒、毫秒和秒级的延时功能
 * 基于STM32的SysTick定时器实现
 */

#include "stm32f10x.h"
#include "delay.h"

/**
  * @brief  微秒级延时函数
  * @param  xus 延时时长，单位：微秒，范围：0~233015
  * @retval 无
  * @note   基于SysTick定时器实现，时钟频率为72MHz
  *         计算公式：LOAD = 时钟频率(Hz) * 延时时间(s) = 72000000 * (xus/1000000) = 72 * xus
  */
void delay_us(uint32_t xus)
{
	SysTick->LOAD = 72 * xus;                //设置定时器重装值
	SysTick->VAL = 0x00;                     //清空当前计数值
	SysTick->CTRL = 0x00000005;              //设置时钟源为HCLK，启动定时器
	while(!(SysTick->CTRL & 0x00010000));    //等待计数到0
	SysTick->CTRL = 0x00000004;              //关闭定时器
}

/**
  * @brief  毫秒级延时函数
  * @param  xms 延时时长，单位：毫秒，范围：0~4294967295
  * @retval 无
  * @note   通过循环调用微秒延时函数实现
  */
void delay_ms(uint32_t xms)
{
	while(xms--)
	{
		delay_us(1000);  //每次循环延时1000微秒（1毫秒）
	}
}
 
/**
  * @brief  秒级延时函数
  * @param  xs 延时时长，单位：秒，范围：0~4294967295
  * @retval 无
  * @note   通过循环调用毫秒延时函数实现
  */
void delay_s(uint32_t xs)
{
	while(xs--)
	{
		delay_ms(1000);  //每次循环延时1000毫秒（1秒）
	}
}
