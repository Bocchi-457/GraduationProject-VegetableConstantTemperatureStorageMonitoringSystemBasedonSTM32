/**
 * dht22.h
 * DHT22温湿度传感器驱动文件
 * 功能：定义DHT22相关的宏、结构体和函数声明
 * 版本：V1.0
 * MCU：STM32F103C8T6
 */

#ifndef __DHT22_H
#define	__DHT22_H

#include "stm32f10x.h"
#include "sys.h" 
#include "delay.h"

// 状态定义
#define SUCCESS 0
#define ERROR   1

typedef struct DHT22_Data_Struct
{
    uint16_t humidity;    // 湿度值，放大10倍，例如 805 表示 80.5%
    int16_t  temperature; // 温度值，放大10倍，例如 -5 表示 -0.5℃
    uint8_t  check_sum;   // 校验和
} DHT22_Data_TypeDef;

/**
 * GPIO配置宏 (PB12)
 * 严格遵循STM32寄存器规范：CNF[1:0]在高位，MODE[1:0]在低位
 */
// PB12开漏输出模式：CNF=01(通用开漏), MODE=11(50MHz) → 0x07
#define DHT22_IO_OUT() {GPIOB->CRH &= 0XFFF0FFFF; GPIOB->CRH |= 0x07 << 16;} 
// PB12浮空输入模式：CNF=01(浮空输入), MODE=00(输入) → 0x04
#define DHT22_IO_IN()  {GPIOB->CRH &= 0XFFF0FFFF; GPIOB->CRH |= 0x04 << 16;}

/**
 * IO操作宏
 */
#define	DHT22_DQ_OUT  PBout(12) // PB12输出
#define	DHT22_DQ_IN   PBin(12)  // PB12输入

/**
 * 函数声明
 */
u8      DHT22_Init(void);       // DHT22初始化
void    DHT22_Rst(void);        // 发送复位/起始信号
uint8_t Read_DHT22(DHT22_Data_TypeDef *data); // 读取温湿度数据
uint8_t DHT22_CheckSum(DHT22_Data_TypeDef *data); // 校验和验证

#endif