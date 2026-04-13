#ifndef __WIFI_DRIVER_H__
#define __WIFI_DRIVER_H__

#include "stm32f10x.h"
#include <stdint.h>
#include <string.h>

/* ESP8266硬件配置 */
#define ESP8266_USART              USART2
#define ESP8266_USART_CLK          RCC_APB1Periph_USART2
#define ESP8266_RX_PIN             GPIO_Pin_3
#define ESP8266_TX_PIN             GPIO_Pin_2
#define ESP8266_GPIO_PORT          GPIOA
#define ESP8266_GPIO_CLK           RCC_APB2Periph_GPIOA

#define ESP8266_RST_PIN            GPIO_Pin_1
#define ESP8266_RST_PORT           GPIOA
#define ESP8266_RST_CLK            RCC_APB2Periph_GPIOA

/* DMA配置 */
#define ESP8266_DMA_CHANNEL        DMA1_Channel6
#define ESP8266_DMA_CLK            RCC_AHBPeriph_DMA1
#define DMA_BUFFER_SIZE            512

/* WiFi配置 */
#define WIFI_SSID                  "TheWorld"
#define WIFI_PASSWORD              "2020625663"

/* 巴法云配置 */
#define BEMFA_SERVER_IP            "bemfa.com"          // 域名方式
//#define BEMFA_SERVER_IP          "121.41.35.209"      // IP方式（备选，如果DNS失败）
#define BEMFA_SERVER_PORT          "8344"
#define BEMFA_UID                  "ae6e47ba373f46e79db0e8ce6e50ea3d"
#define BEMFA_TOPIC_DATA           "data"
#define BEMFA_TOPIC_CONTROL        "control"

/**
 * @brief 初始化WiFi模块（DMA+IDLE中断）
 */
void WiFi_Driver_Init(uint32_t baudrate);

/**
 * @brief 复位ESP8266模块
 */
void WiFi_Module_Reset(void);

/**
 * @brief 发送AT指令（阻塞方式，用于初始化阶段）
 * @param cmd AT指令字符串
 * @param expected_ack 期望的响应关键字
 * @param timeout_ms 超时时间（毫秒）
 * @return 1=成功, 0=失败
 */
uint8_t WiFi_Send_AT_Command(const char *cmd, const char *expected_ack, uint32_t timeout_ms);

/**
 * @brief 从DMA缓冲区读取数据
 * @param data 输出缓冲区
 * @param max_len 最大读取长度
 * @return 实际读取的字节数
 */
uint16_t WiFi_Read_Data(uint8_t *data, uint16_t max_len);

/**
 * @brief 清空接收缓冲区
 */
void WiFi_Clear_Buffer(void);

/**
 * @brief 获取接收缓冲区中的数据长度
 * @return 数据长度
 */
uint16_t WiFi_Get_Data_Length(void);

/**
 * @brief 发送原始数据（非阻塞）
 * @param data 数据指针
 * @param len 数据长度
 */
void WiFi_Send_Data(const uint8_t *data, uint16_t len);

#endif /* __WIFI_DRIVER_H__ */
