/**
 * sys.h
 * 系统配置头文件
 * 功能：提供系统级别的配置和IO口操作宏定义
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 * 作者：蔬菜恒温库监控系统开发团队
 * 创建日期：2026-03-09
 * 项目：蔬菜恒温库监控系统
 */

#ifndef __SYS_H
#define __SYS_H	
#include "stm32f10x.h" 

/**
 * @brief UCOS支持配置
 * @note 0: 不支持UCOS，1: 支持UCOS
 */
#define SYSTEM_SUPPORT_UCOS		0		// 定义系统文件夹是否支持UCOS
														    
	 
/**
 * @brief 位带操作宏定义
 * @note 实现51类似的GPIO控制功能，参考<<CM3权威指南>>第五章(87页~92页)
 */
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) // 位带地址计算
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) // 内存地址访问
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) // 位地址访问

/**
 * @brief IO口地址映射
 * @note 定义各GPIO端口的输出数据寄存器(ODR)和输入数据寄存器(IDR)地址
 */
// 输出数据寄存器(ODR)地址
#define GPIOA_ODR_Addr    (GPIOA_BASE+12) // 0x4001080C 
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) // 0x40010C0C 
#define GPIOC_ODR_Addr    (GPIOC_BASE+12) // 0x4001100C 
#define GPIOD_ODR_Addr    (GPIOD_BASE+12) // 0x4001140C 
#define GPIOE_ODR_Addr    (GPIOE_BASE+12) // 0x4001180C 
#define GPIOF_ODR_Addr    (GPIOF_BASE+12) // 0x40011A0C    
#define GPIOG_ODR_Addr    (GPIOG_BASE+12) // 0x40011E0C    

// 输入数据寄存器(IDR)地址
#define GPIOA_IDR_Addr    (GPIOA_BASE+8) // 0x40010808 
#define GPIOB_IDR_Addr    (GPIOB_BASE+8) // 0x40010C08 
#define GPIOC_IDR_Addr    (GPIOC_BASE+8) // 0x40011008 
#define GPIOD_IDR_Addr    (GPIOD_BASE+8) // 0x40011408 
#define GPIOE_IDR_Addr    (GPIOE_BASE+8) // 0x40011808 
#define GPIOF_IDR_Addr    (GPIOF_BASE+8) // 0x40011A08 
#define GPIOG_IDR_Addr    (GPIOG_BASE+8) // 0x40011E08 

/**
 * @brief IO口操作宏定义
 * @note 只对单一的IO口操作，确保n的值小于16
 */
// PA端口操作
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  // 输出 
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  // 输入 

// PB端口操作
#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  // 输出 
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  // 输入 

// PC端口操作
#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  // 输出 
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  // 输入 

// PD端口操作
#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  // 输出 
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  // 输入 

// PE端口操作
#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)  // 输出 
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  // 输入

// PF端口操作
#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)  // 输出 
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  // 输入

// PG端口操作
#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)  // 输出 
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  // 输入

/**
 * @brief NVIC配置函数
 * @param 无
 * @return 无
 * @note 配置嵌套向量中断控制器
 */
void NVIC_Configuration(void);

#endif

/***************************** 结束 *****************************/
