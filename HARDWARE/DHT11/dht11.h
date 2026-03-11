/**
 * dht11.h
 * DHT11温湿度传感器头文件
 * 功能：定义DHT11相关的宏、结构体和函数声明
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-07
 */

#ifndef __DHT11_H
#define	__DHT11_H

#include "stm32f10x.h"
#include "sys.h" 
#include "delay.h"

/**
 * 逻辑电平定义
 */
#define HIGH  1
#define LOW   0

/**
 * 温湿度数据结构体
 */
typedef struct
{
	uint8_t  humi_int; 		// 湿度的整数部分
	uint8_t  humi_deci; 		// 湿度的小数部分
	uint8_t  temp_int; 		// 温度的整数部分
	uint8_t  temp_deci; 		// 温度的小数部分
	uint8_t  check_sum; 		// 校验和
		                 
}DHT11_Data_TypeDef;

/**
 * IO方向设置
 */
#define DHT11_IO_IN()  {GPIOB->CRH&=0XFFF0FFFF;GPIOB->CRH|=8<<16;}  // PB12设置为输入
#define DHT11_IO_OUT() {GPIOB->CRH&=0XFFF0FFFF;GPIOB->CRH|=3<<16;} // PB12设置为输出

/**
 * IO操作函数
 */
#define	DHT11_DQ_OUT PBout(12) // 数据端口 PB12
#define	DHT11_DQ_IN  PBin(12)  // 数据端口 PB12

// ErrorStatus 已在 stm32f10x.h 中定义，此处不再重复定义

/**
 * 函数声明
 */

/**
 * 初始化DHT11
 * 功能：配置GPIO并检测DHT11是否存在
 * @param 无
 * @retval 1: DHT11不存在
 * @retval 0: DHT11存在
 */
u8 DHT11_Init(void);

/**
 * 复位DHT11
 * 功能：发送复位信号，使DHT11进入起始状态
 * @param 无
 * @retval 无
 */
void DHT11_Rst(void);

/**
 * 读出一个字节
 * 功能：从DHT11读取8位数据
 * @param 无
 * @retval u8 读到的数据
 */
u8 DHT11_Read_Byte(void);

/**
 * 读出一个位
 * 功能：从DHT11读取单个位数据
 * @param 无
 * @retval 1: 读取到逻辑1
 * @retval 0: 读取到逻辑0
 */
u8 DHT11_Read_Bit(void);

/**
 * 检测是否存在DHT11
 * 功能：检测DHT11是否存在并响应
 * @param 无
 * @retval 1: 未检测到DHT11的存在
 * @retval 0: DHT11存在
 */
u8 DHT11_Check(void);

/**
 * 读取DHT11温湿度数据
 * 功能：从DHT11读取温湿度数据并存储到结构体中
 * @param DHT11_Data 温湿度数据结构体指针
 * @retval SUCCESS: 读取成功
 * @retval ERROR: 读取失败
 */
uint8_t Read_DHT11(DHT11_Data_TypeDef *DHT11_Data);

#endif /* __DHT11_H */