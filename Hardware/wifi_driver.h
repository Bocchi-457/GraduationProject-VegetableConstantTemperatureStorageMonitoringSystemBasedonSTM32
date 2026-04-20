/**
 * wifi_driver.h
 * ESP8266 WiFi驱动头文件
 * 版本：V1.0
 * MCU：STM32F103C8T6
 */

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

/* 双缓冲区配置 */
#define AT_RING_BUFFER_SIZE        256   // AT响应缓冲区
#define CLOUD_RING_BUFFER_SIZE     512   // 云端数据缓冲区

/* WiFi配置 */
#define WIFI_SSID                  "bocchi457"
#define WIFI_PASSWORD              "2020625663"

/* 巴法云配置 */
#define BEMFA_SERVER_IP            "bemfa.com"          // 域名方式
//#define BEMFA_SERVER_IP          "121.41.35.209"      // IP方式（备选，如果DNS失败）
#define BEMFA_SERVER_PORT          "8344"
#define BEMFA_UID                  "ae6e47ba373f46e79db0e8ce6e50ea3d"
#define BEMFA_TOPIC_DATA           "data"
#define BEMFA_TOPIC_CONTROL        "control"

/**
 * @brief 初始化WiFi模块（双缓冲区架构）
 */
void WiFi_Driver_Init(uint32_t baudrate);

/**
 * @brief 复位ESP8266模块
 */
void WiFi_Module_Reset(void);

/**
 * @brief 发送AT指令（阻塞方式，响应确认后清空缓冲区）
 * @param cmd AT指令字符串
 * @param expected_ack 期望的响应关键字
 * @param timeout_ms 超时时间（毫秒）
 * @return 1=成功, 0=失败
 */
uint8_t WiFi_Send_AT_Command(const char *cmd, const char *expected_ack, uint32_t timeout_ms);

/**
 * @brief 清空AT缓冲区（供初始化阶段使用）
 */
void RingBuffer_AT_Clear(void);

/**
 * @brief 清空云端缓冲区
 */
void RingBuffer_Cloud_Clear(void);

/**
 * @brief 重置解析器状态机（强制回到IDLE状态）
 * @note 用于时间同步前，避免上次遗留的状态影响
 */
void WiFi_Reset_Parse_State(void);

/**
 * @brief 从AT缓冲区读取数据（供外部调用）
 * @param data 输出缓冲区
 * @param max_len 最大读取长度
 * @return 实际读取的字节数
 */
uint16_t RingBuffer_AT_Read(uint8_t *data, uint16_t max_len);

/**
 * @brief 从云端缓冲区读取完整帧（基于\r\n判断）
 * @param data 输出缓冲区
 * @param max_len 最大长度
 * @return 完整帧长度，0表示没有完整帧
 */
uint16_t WiFi_Read_Cloud_Complete_Frame(uint8_t *data, uint16_t max_len);

/**
 * @brief 获取云端缓冲区数据量
 * @return 数据长度
 */
uint16_t WiFi_Get_Cloud_Data_Length(void);

/**
 * @brief 从云端缓冲区读取数据（不依赖\r\n，用于时间同步等特殊场景）
 * @param data 输出缓冲区
 * @param max_len 最大读取长度
 * @return 实际读取的字节数
 * @note 如果缓冲区中有数据但没有\r\n，也会返回数据
 */
uint16_t WiFi_Read_Cloud_Data_NoDelimiter(uint8_t *data, uint16_t max_len);

/**
 * @brief 预览云端缓冲区内容（不移除数据）
 * @param data 输出缓冲区
 * @param max_len 最大读取长度
 * @return 实际读取的字节数
 */
uint16_t WiFi_Peek_Cloud_Data(uint8_t *data, uint16_t max_len);

/**
 * @brief 发送原始数据（用于AT+CIPSEND第二阶段）
 * @param data 数据指针
 * @param len 数据长度
 */
void WiFi_Send_Data(const uint8_t *data, uint16_t len);

#endif /* __WIFI_DRIVER_H__ */
