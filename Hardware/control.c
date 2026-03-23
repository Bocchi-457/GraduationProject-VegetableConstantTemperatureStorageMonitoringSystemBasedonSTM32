/**
 * control.c
 * 控制模块驱动文件
 * 功能：实现蜂鸣器、加热器、制冷器和除湿器的GPIO初始化
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 */

#include "control.h"
#include "delay.h"

/**
 * 蜂鸣器初始化函数
 * 功能：初始化蜂鸣器控制引脚
 * @param 无
 * @retval 无
 * @note 蜂鸣器连接到PA8引脚
 */
void beep_init(void) 
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 1. 开启GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 2. 配置PA8引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        // 推挽输出模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // 高速模式
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 设置初始状态为关闭（高电平）
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
}

/**
 * 加热器初始化函数
 * 功能：初始化加热器控制引脚
 * @param 无
 * @retval 无
 * @note 加热器连接到PB10引脚
 */
void jiare_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 1. 开启GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 2. 配置PB10引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        // 推挽输出模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // 高速模式
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
 * 制冷器初始化函数
 * 功能：初始化制冷器控制引脚
 * @param 无
 * @retval 无
 * @note 制冷器连接到PB11引脚
 */
void zhileng_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 1. 开启GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 2. 配置PB11引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        // 推挽输出模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // 高速模式
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
 * 除湿器初始化函数
 * 功能：初始化除湿器控制引脚
 * @param 无
 * @retval 无
 * @note 除湿器连接到PB1引脚
 */
void chushi_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 1. 开启GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 2. 配置PB1引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        // 推挽输出模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // 高速模式
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
 * 加湿器初始化函数
 * 功能：初始化加湿器控制引脚
 * @param 无
 * @retval 无
 * @note 加湿器连接到PB0引脚
 */
void jiashi_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 1. 开启GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 2. 配置PB0引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        // 推挽输出模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // 高速模式
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}