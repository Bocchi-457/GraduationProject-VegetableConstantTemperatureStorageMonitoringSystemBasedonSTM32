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
 * 【新增】ESP8266连接状态枚举
 */
typedef enum {
  CONN_STATE_IDLE = 0,      // 空闲状态
  CONN_STATE_WIFI_CONNECTING,   // WiFi连接中
  CONN_STATE_TCP_CONNECTING,    // TCP连接中
  CONN_STATE_CONNECTED,         // 已连接（正常）
  CONN_STATE_TCP_LOST,          // TCP断开
  CONN_STATE_WIFI_LOST,         // WiFi断开
  CONN_STATE_MODULE_ERROR,      // 模块异常
  CONN_STATE_RECONNECTING       // 重连中
} ConnState_t;

/**
 * 【新增】网络诊断结果枚举
 */
typedef enum {
  DIAG_OK = 0,              // 诊断通过，连接正常
  DIAG_NETWORK_CONGESTED,   // 网络拥塞
  DIAG_TCP_DISCONNECTED,    // TCP连接断开
  DIAG_WIFI_DISCONNECTED,   // WiFi断开
  DIAG_MODULE_ERROR         // 模块异常
} DiagResult_t;

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
 * 【新增】全局连接状态和诊断变量
 */
extern volatile ConnState_t g_conn_state;     // 当前连接状态
extern uint8_t g_send_fail_count;             // 连续发送失败计数
extern uint32_t g_last_reconnect_time;        // 上次重连时间戳（用于防抖）

/**
 * ESP8266函数声明
 */
void ESP8266_Clear(void); // 清空ESP8266接收缓冲区

_Bool ESP8266_WaitRecive(void); // 等待ESP8266接收数据

_Bool ESP8266_SendCmd(char *cmd, char *res); // 发送AT指令并等待响应

void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str,
                      unsigned short len); // 串口发送字符串

uint8_t ESP8266_SendData(unsigned char *data); // 发送数据到巴法云

void ESP8266_Init(unsigned int bound); // ESP8266初始化（仅上电时调用）

void USART2_IRQHandler(void); // USART2中断处理函数

/**
 * 【新增】连接状态诊断与重连函数声明
 */
DiagResult_t ESP8266_DiagnoseConnection(void);  // 诊断连接状态
uint8_t ESP8266_TCP_Reconnect(void);            // TCP层重连（非阻塞，需多次调用）
uint8_t ESP8266_WIFI_Reconnect(void);           // WiFi层重连（非阻塞，需多次调用）
uint8_t ESP8266_Module_Reset(void);             // 模块复位重连（非阻塞，需多次调用）
ConnState_t ESP8266_GetConnState(void);         // 获取当前连接状态
void ESP8266_UpdateConnState(ConnState_t state); // 更新连接状态

/**
 * AT指令非阻塞执行状态枚举
 */
typedef enum {
    AT_STATE_IDLE = 0,      // 空闲
    AT_STATE_SENDING_CMD,   // 发送AT指令中
    AT_STATE_WAITING_ACK,   // 等待响应
    AT_STATE_SENDING_DATA,  // 发送数据阶段（CIPSEND两阶段）
    AT_STATE_DONE,          // 完成
    AT_STATE_TIMEOUT,       // 超时
    AT_STATE_ERROR          // 错误
} AT_State_t;

/**
 * AT指令执行结果
 */
typedef enum {
    AT_RESULT_OK = 0,       // 成功
    AT_RESULT_BUSY,         // 忙（正在执行其他AT指令）
    AT_RESULT_TIMEOUT,      // 超时
    AT_RESULT_ERROR,        // 错误
    AT_RESULT_IN_PROGRESS   // 进行中
} AT_Result_t;

/**
 * @brief 初始化AT指令非阻塞执行器
 * @note 在ESP8266_Init之后调用
 */
void AT_Executor_Init(void);

/**
 * @brief 非阻塞执行AT指令（无数据阶段）
 * @param cmd: AT指令字符串（如"AT\r\n"）
 * @param expected_ack: 期望的响应关键字（如"OK"）
 * @param timeout_ms: 超时时间（毫秒）
 * @return AT_Result_t
 * @note 需周期性调用直到返回非AT_RESULT_IN_PROGRESS
 */
AT_Result_t AT_Execute_NonBlocking(const char *cmd, const char *expected_ack, uint32_t timeout_ms);

/**
 * @brief 非阻塞执行AT+CIPSEND两阶段发送
 * @param data: 要发送的数据
 * @param len: 数据长度
 * @param timeout_ms: 超时时间（毫秒）
 * @return AT_Result_t
 * @note 阶段1: 发送"AT+CIPSEND=len"，等待">"
 *       阶段2: 发送数据，等待"SEND OK"
 */
AT_Result_t AT_SendData_NonBlocking(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 获取当前AT执行状态
 */
AT_State_t AT_GetState(void);

/**
 * @brief 强制重置AT执行器状态
 * @note 用于异常情况下的状态恢复
 */
void AT_Reset(void);

/**
 * @brief 获取AT执行器忙标志
 * @return 1=忙（正在执行AT指令），0=空闲
 * @note 用于防止并发调用AT指令导致状态机冲突
 */
uint8_t AT_IsBusy(void);

/**
 * @brief 获取AT执行器调用计数器（用于诊断日志时序）
 * @return 当前调用次数
 */
uint32_t AT_GetCallCount(void);

#endif // _ESP8266_H_
