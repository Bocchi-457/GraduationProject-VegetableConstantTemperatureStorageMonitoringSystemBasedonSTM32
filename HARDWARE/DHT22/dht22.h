/**
 * dht22.h
 * DHT22温湿度传感器驱动文件
 * 功能：定义DHT22相关的宏、结构体和函数声明
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-07
 */

#ifndef __DHT22_H
#define	__DHT22_H

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
	uint8_t  humi_int; 	    // 湿度整数部分（DHT22：0-100）
	uint8_t  humi_deci; 	// 湿度小数部分（DHT22：0-9，精度0.1%）
	int8_t   temp_int; 	    // 温度整数部分（DHT22：-40~80），使用有符号整数
	uint8_t  temp_deci; 	// 温度小数部分（DHT22：0-9，精度0.1℃）
	uint8_t  check_sum; 	// 校验和
}DHT22_Data_TypeDef;

/**
 * IO方向设置（PB12）
 */
#define DHT22_IO_IN()  {GPIOB->CRH&=0XFFF0FFFF;GPIOB->CRH|=8<<16;}  // PB12输入模式
#define DHT22_IO_OUT() {GPIOB->CRH&=0XFFF0FFFF;GPIOB->CRH|=3<<16;} // PB12输出模式

/**
 * IO操作函数
 */
#define	DHT22_DQ_OUT PBout(12) // 数据端口 PB12输出
#define	DHT22_DQ_IN  PBin(12)  // 数据端口 PB12输入

// 状态定义
#define SUCCESS 0
#define ERROR   1

/**
 * 函数声明
 */
u8 DHT22_Init(void);
void DHT22_Rst(void);
u8 DHT22_Read_Bit(void);
u8 DHT22_Read_Byte(void);
u8 DHT22_Check(void);
uint8_t Read_DHT22(DHT22_Data_TypeDef *DHT22_Data);

#endif /* __DHT22_H */