/**
 * SGP30.h
 * SGP30气体传感器驱动头文件
 * 功能：提供SGP30传感器的IIC通信和数据读写函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 * 作者：蔬菜恒温库监控系统开发团队
 * 创建日期：2026-03-08
 * 项目：蔬菜恒温库监控系统
 */

#ifndef __SGP30_H
#define __SGP30_H

#include "sys.h"

/**
 * @brief SGP30 IIC引脚定义
 */
#define  SGP30_SCL   PBout(10)    // SCL引脚（PB10）
#define  SGP30_SDA   PBout(11)    // SDA引脚（PB11）

/**
 * @brief SGP30 GPIO配置宏定义
 */
#define  SGP30_SCL_GPIO_CLK        RCC_APB2Periph_GPIOB  // SCL时钟
#define  SGP30_SCL_GPIO_PORT       GPIOB                 // SCL端口
#define  SGP30_SCL_GPIO_PIN        GPIO_Pin_10           // SCL引脚

#define  SGP30_SDA_GPIO_SDA        RCC_APB2Periph_GPIOB  // SDA时钟
#define  SGP30_SDA_GPIO_PORT       GPIOB                 // SDA端口
#define  SGP30_SDA_GPIO_PIN        GPIO_Pin_11           // SDA引脚

/**
 * @brief 读取SDA引脚状态
 */
#define  SGP30_SDA_READ()           GPIO_ReadInputDataBit(SGP30_SDA_GPIO_PORT, SGP30_SDA_GPIO_PIN)

/**
 * @brief SGP30 IIC地址
 */
#define SGP30_read  0xb1  // SGP30的读地址
#define SGP30_write 0xb0  // SGP30的写地址

/**
 * @brief 发送IIC开始信号
 * @param 无
 * @return 无
 */
void SGP30_IIC_Start(void);			//发送IIC开始信号

/**
 * @brief 发送IIC停止信号
 * @param 无
 * @return 无
 */
void SGP30_IIC_Stop(void);	   		//发送IIC停止信号

/**
 * @brief IIC发送一个字节
 * @param txd: 要发送的字节
 * @return 无
 */
void SGP30_IIC_Send_Byte(u8 txd);		//IIC发送一个字节

/**
 * @brief IIC读取一个字节
 * @param ack: 应答标志，1表示发送ACK，0表示发送NACK
 * @return 读取的字节
 */
u16 SGP30_IIC_Read_Byte(unsigned char ack);//IIC读取一个字节

/**
 * @brief IIC等待ACK信号
 * @param 无
 * @return 0表示成功，1表示失败
 */
u8 SGP30_IIC_Wait_Ack(void); 			//IIC等待ACK信号

/**
 * @brief IIC发送ACK信号
 * @param 无
 * @return 无
 */
void SGP30_IIC_Ack(void);			//IIC发送ACK信号

/**
 * @brief IIC不发送ACK信号
 * @param 无
 * @return 无
 */
void SGP30_IIC_NAck(void);			//IIC不发送ACK信号

/**
 * @brief IIC写入一个字节数据
 * @param daddr: 器件地址
 * @param addr: 寄存器地址
 * @param data: 要写入的数据
 * @return 无
 */
void SGP30_IIC_Write_One_Byte(u8 daddr, u8 addr, u8 data);

/**
 * @brief IIC读取一个字节数据
 * @param daddr: 器件地址
 * @param addr: 寄存器地址
 * @return 读取的数据
 */
u8 SGP30_IIC_Read_One_Byte(u8 daddr, u8 addr);	

/**
 * @brief 初始化SGP30传感器
 * @param 无
 * @return 无
 */
void SGP30_Init(void);		  

/**
 * @brief 向SGP30写入命令
 * @param a: 命令高字节
 * @param b: 命令低字节
 * @return 无
 */
void SGP30_Write(u8 a, u8 b);

/**
 * @brief 从SGP30读取数据
 * @param 无
 * @return 读取的数据（32位）
 */
u32 SGP30_Read(void);

#endif

/***************************** 结束 *****************************/

