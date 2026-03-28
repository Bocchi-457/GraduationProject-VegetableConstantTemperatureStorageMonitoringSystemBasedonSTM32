/**
 * Key.c
 * 按键驱动文件
 * 功能：实现按键的初始化和扫描
 * 版本：V1.0
 * MCU：STM32F103C8T6
 */

#include "stm32f10x.h"                  // Device header
#include "delay.h"
#include "Key.h"

/**
 * 按键初始化函数
 * 功能：初始化按键对应的GPIO引脚
 * @param 无
 * @retval 无
 * @note 使用GPIOB的4、5、6、7引脚作为按键输入
 */
void Key_Init(void)
{
  	GPIO_InitTypeDef GPIO_InitStructure;
  
	// 使能GPIOB时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  
	// 禁用JTAG功能，释放PB3、PB4、PA15引脚
  GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
  
	// 配置GPIO为上拉输入模式
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;  // PB4-PB7
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
 * 按键扫描函数
 * 功能：扫描按键状态，返回按键值
 * @param mode 扫描模式：0-不支持连续按，1-支持连续按
 * @retval 0-无按键按下
 * @retval KEY1_PRES-KEY1按键按下
 * @retval KEY2_PRES-KEY2按键按下
 * @retval KEY3_PRES-KEY3按键按下
 * @retval KEY4_PRES-KEY4按键按下
 * @note 按键优先级：KEY1 > KEY2 > KEY3 > KEY4
 */
u8 KEY_Scan(u8 mode)
{ 
	static u8 key_up = 1;  // 按键松开标志
	
	if(mode) key_up = 1;  // 支持连续按
  	
	// 检测按键是否按下
	if(key_up && (KEY1 == 0 || KEY2 == 0 || KEY3 == 0 || KEY4 == 0))
	{
		delay_ms(10);  // 消抖
		key_up = 0;
		
		// 检测具体是哪个按键按下
		if(KEY1 == 0) return KEY1_PRES;
		else if(KEY2 == 0) return KEY2_PRES;
		else if(KEY3 == 0) return KEY3_PRES;
		else if(KEY4 == 0) return KEY4_PRES;
	} 
	// 检测按键是否松开
	else if(KEY1 == 1 && KEY2 == 1 && KEY3 == 1 && KEY4 == 1) 
		key_up = 1; 	    
  	return 0;  // 无按键按下
}
