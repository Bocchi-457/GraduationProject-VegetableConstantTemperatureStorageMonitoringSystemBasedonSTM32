/**
 * esp8266.h
 * ESP8266 WiFi模块驱动头文件
 * 功能：提供ESP8266 WiFi模块与巴法云平台通信的接口定义
 * 版本：V1.0
 * MCU：STM32F103C8T6
 */

#ifndef _ESP8266_H_
#define _ESP8266_H_

// 单片机头文件
#include "stm32f10x.h"

/**
 * ESP8266接收状态定义
 */
#define REV_OK 0    // 接收完成标志
#define REV_WAIT 1  // 接收未完成标志
#define buf_len 256 // 串口接收缓冲区总长度

/**
 * 串口选择定义
 */
#define Bemfa_USART1 0 // 使用USART1
#define Bemfa_USART2 1 // 使用USART2
#define Bemfa_USART3 0 // 使用USART3

#define Bemfa_USART USART2 // 当前使用的串口

/**
 * ESP-01S复位引脚定义
 * 注意：根据实际硬件连接修改这些定义
 */
#define ESP01S_RST_RCC_CLK RCC_APB2Periph_GPIOA // 复位引脚时钟
#define ESP01S_RST_PROT GPIOA                   // 复位引脚端口
#define ESP01S_RST_PIN GPIO_Pin_1               // 复位引脚编号

/**
 * WiFi连接配置
 * 注意：根据实际WiFi网络修改这些定义
 */
#define ESP8266_WIFI_INFO                                                      \
  "AT+CWJAP=\"bocchi457\",\"2020625663\"\r\n" // WiFi名称和密码

/**
 * 巴法云平台连接配置
 * 注意：巴法云网络端口通常不需要修改
 */
#define ESP8266_ONENET_INFO                                                    \
  "AT+CIPSTART=\"TCP\",\"bemfa.com\",8344\r\n" // TCP连接指令

/**
 * 巴法云平台用户配置
 * 注意：需要修改为实际的巴法云用户秘钥
 */
#define BEMFA_ID "ae6e47ba373f46e79db0e8ce6e50ea3d" // 巴法云用户秘钥

/**
 * 巴法云主题定义
 */
#define DATA_TOPIC "data" // 数据上传主题

/**
 * 巴法云订阅主题指令
 * 注意：需要修改为实际的巴法云用户秘钥
 */
#define ESP8266_TOPIC                                                          \
  "cmd=1&uid=ae6e47ba373f46e79db0e8ce6e50ea3d&topic=data,control,online\r\n" // 订阅主题指令

/**
 * 返回时间指令
 */
#define Return_Time                                                            \
  "cmd=7&uid=ae6e47ba373f46e79db0e8ce6e50ea3d&type=1\r\n" // 返回时间指令

/**
 * 全局变量声明
 */
// 【优化】双缓冲区职责明确声明
extern unsigned char
    esp8266_buf[buf_len]; // 应用层接收缓冲区：专用于解析云端下发的异步指令
extern unsigned short esp8266_cnt;        // 应用层缓冲区计数
extern unsigned char esp8266_recive_flag; // 应用层接收完成标志

extern uint8_t
    ESP8266_RecvBuf[buf_len];    // 驱动层接收缓冲区：专用于处理AT指令的同步响应
extern uint16_t ESP8266_RecvLen; // 驱动层缓冲区数据长度

/**
 * ESP8266函数声明
 */
void ESP8266_Clear(void); // 清空ESP8266接收缓冲区

_Bool ESP8266_WaitRecive(void); // 等待ESP8266接收数据

_Bool ESP8266_SendCmd(char *cmd, char *res); // 发送AT指令并等待响应

void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str,
                      unsigned short len); // 串口发送字符串

uint8_t ESP8266_SendData(unsigned char *data); // 发送数据到巴法云

void ESP8266_Init(unsigned int bound); // ESP8266初始化

void USART2_IRQHandler(void); // USART2中断处理函数

#endif // _ESP8266_H_