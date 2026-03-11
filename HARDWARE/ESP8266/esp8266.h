/**
 * esp8266.h
 * ESP8266 WiFi模块驱动头文件
 * 功能：提供ESP8266 WiFi模块与巴法云平台通信的接口定义
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 */

#ifndef _ESP8266_H_
#define _ESP8266_H_

// 单片机头文件
#include "stm32f10x.h"

/**
 * ESP8266接收状态定义
 */
#define REV_OK		0	// 接收完成标志
#define REV_WAIT	1	// 接收未完成标志
#define buf_len    256  // 串口接收缓冲区总长度

/**
 * 串口选择定义
 */
#define Bamfa_USART1		        0  // 使用USART1
#define Bamfa_USART2		        1  // 使用USART2
#define Bamfa_USART3		        0  // 使用USART3

#define Bamfa_USART		            USART2  // 当前使用的串口

/**
 * ESP-01S复位引脚定义
 * 注意：根据实际硬件连接修改这些定义
 */
#define ESP01S_RST_RCC_CLK	RCC_APB2Periph_GPIOA  // 复位引脚时钟
#define ESP01S_RST_PROT		    GPIOA              // 复位引脚端口
#define ESP01S_RST_PIN		    GPIO_Pin_1          // 复位引脚编号

/**
 * WiFi连接配置
 * 注意：根据实际WiFi网络修改这些定义
 */
#define ESP8266_WIFI_INFO		"AT+CWJAP=\"The world\",\"2020625663\"\r\n"  // WiFi名称和密码

/**
 * 巴法云平台连接配置
 * 注意：巴法云网络端口通常不需要修改
 */
#define ESP8266_ONENET_INFO	"AT+CIPSTART=\"TCP\",\"bemfa.com\",8344\r\n"  // TCP连接指令

/**
 * 巴法云平台用户配置
 * 注意：需要修改为实际的巴法云用户秘钥
 */
#define BEMFA_ID  "ae6e47ba373f46e79db0e8ce6e50ea3d"  // 巴法云用户秘钥

/**
 * 巴法云主题定义
 */
#define DATA_TOPIC "data"  // 数据上传主题

/**
 * 巴法云订阅主题指令
 * 注意：需要修改为实际的巴法云用户秘钥
 */
#define ESP8266_TOPIC   "cmd=1&uid=ae6e47ba373f46e79db0e8ce6e50ea3d&topic=data,control,online\r\n"  // 订阅主题指令
                         
/**
 * 返回时间指令
 * 注意：需要修改为实际的巴法云用户秘钥
 */
#define Return_Time	"cmd=7&uid=ae6e47ba373f46e79db0e8ce6e50ea3d&type=1\r\n"  // 返回时间指令

/**
 * 全局变量声明
 */
extern unsigned char Secret_Key[];  // 加密密钥

/**
 * ESP8266函数声明
 */
void ESP8266_Clear(void);  // 清空ESP8266接收缓冲区

_Bool ESP8266_WaitRecive(void);  // 等待ESP8266接收数据

_Bool ESP8266_SendCmd(char *cmd, char *res);  // 发送AT指令并等待响应

void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len);  // 串口发送字符串

void ESP8266_SendData(unsigned char *data);  // 发送数据到巴法云

void ESP8266_Init(unsigned int bound);  // ESP8266初始化

void USART2_IRQHandler(void);  // USART2中断处理函数

void mode_choice(void);  // 模式选择函数

/**
 * 工具函数声明
 */
// 提取小时和分钟的函数
void extractHourAndMinute(const char *input, int *hour, int *minute);

#endif // _ESP8266_H_
