/**
 * Usart.h
 * 串口驱动头文件
 * 功能：提供串口初始化和数据收发功能
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 */


#include "stdio.h"
#include "sys.h"

#ifndef __USART_H
#define __USART_H

/**
 * 【全局日志开关】
 * 1 = 调试模式（启用所有Serial_Printf日志）
 * 0 = 生产模式（禁用所有日志，零性能开销）
 */
#define DEBUG_LOG  1

#if DEBUG_LOG
    // 调试模式：Serial_Printf 映射到实际函数
    void Serial_Printf_1(char *format, ...);
    #define Serial_Printf(fmt, ...) Serial_Printf_1(fmt, ##__VA_ARGS__)
#else
    // 生产模式：Serial_Printf 为空宏，不产生任何代码
    #define Serial_Printf(fmt, ...)
#endif

/**
 * @brief 数据包长度
 */
#define Packet_Len 8

/**
 * @brief 串口使能宏定义
 * @note 1: 使能，0: 禁用
 */
#define USART1_ENABLE 1 // 使能USART1
#define USART2_ENABLE 0 // 禁用USART2
#define USART3_ENABLE 0 // 禁用USART3

/**
 * @brief 发送串口选择
 */
#define SEND_USART USART1

/**
 * @brief 全局变量声明
 */
extern unsigned short data_cnt;   // 数据计数器
extern uint8_t Seria1_TxPacket[]; // 发送数据包
extern char Serial_RxPacket[];    // 接收数据包
extern uint8_t Serial_RxFlag;     // 接收标志位

/**
 * @brief 串口初始化函数
 * @param bound: 波特率
 * @return 无
 */
void Serial_Iint(u32 bound);

/**
 * @brief 发送单个字节
 * @param Byte: 要发送的字节
 * @return 无
 */
void Serial_SendByte(uint8_t Byte);

/**
 * @brief 发送数组
 * @param Array: 要发送的数组指针
 * @param Length: 数组长度
 * @return 无
 */
void Serial_SendArray(uint8_t *Array, uint16_t Length);

/**
 * @brief 发送字符串
 * @param String: 要发送的字符串指针
 * @return 无
 */
void Serial_SendString(char *String);

/**
 * @brief 发送数字
 * @param Number: 要发送的数字
 * @param Length: 数字长度
 * @return 无
 */
void Serial_SendNumber(uint32_t Number, uint8_t Length);

/**
 * @brief 发送数据包
 * @param 无
 * @return 无
 */
void Serial_SendPacket(void);

/**
 * @brief 获取接收标志位
 * @param 无
 * @return 接收标志位状态
 */
uint8_t Serial_GetRxFlag(void);

#endif

/***************************** 结束 *****************************/
