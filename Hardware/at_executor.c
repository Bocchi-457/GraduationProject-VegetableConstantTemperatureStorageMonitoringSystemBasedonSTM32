/**
 * at_executor.c
 * AT指令非阻塞执行器实现
 * 功能：基于状态机的非阻塞AT指令执行，避免delay_ms阻塞
 * 版本：V1.0
 */

#include "esp8266.h"
#include "Timer.h"  // sys_tick_ms
#include "Usart.h"  // Serial_Printf（可选日志）
#include <string.h>
#include <stdio.h>

/**
 * 【调试开关】AT执行器日志控制
 */
#ifndef DEBUG_AT_EXECUTOR
  #define DEBUG_AT_EXECUTOR  0  // 生产环境建议关闭
#endif

#if DEBUG_AT_EXECUTOR
  #define AT_LOG(fmt, ...) Serial_Printf("[AT] " fmt, ##__VA_ARGS__)
#else
  #define AT_LOG(fmt, ...)
#endif

/**
 * 内部状态变量
 */
static AT_State_t g_at_state = AT_STATE_IDLE;
static uint32_t g_at_start_time = 0;
static uint32_t g_at_timeout = 0;
static const char *g_expected_ack = NULL;
static uint8_t g_at_retry_count = 0;

// CIPSEND两阶段发送相关
static const uint8_t *g_send_data = NULL;
static uint16_t g_send_len = 0;

// AT执行器忙标志（防止并发调用）
volatile uint8_t g_at_busy = 0;

/**
 * @brief 初始化AT指令非阻塞执行器
 */
void AT_Executor_Init(void) {
    g_at_state = AT_STATE_IDLE;
    g_at_start_time = 0;
    g_at_timeout = 0;
    g_expected_ack = NULL;
    g_at_retry_count = 0;
    g_send_data = NULL;
    g_send_len = 0;
    g_at_busy = 0;  // ✅ 初始化为空闲
}

/**
 * @brief 获取当前AT执行状态
 */
AT_State_t AT_GetState(void) {
    return g_at_state;
}

/**
 * @brief 强制重置AT执行器状态
 */
void AT_Reset(void) {
    g_at_state = AT_STATE_IDLE;
    g_at_start_time = 0;
    g_expected_ack = NULL;
    g_send_data = NULL;
    g_at_busy = 0;  // ✅ 重置时清除忙标志
    AT_LOG("[WARN] AT executor reset\r\n");
}

/**
 * @brief 获取AT执行器忙标志
 * @return 1=忙（正在执行AT指令），0=空闲
 */
uint8_t AT_IsBusy(void) {
    return g_at_busy;
}

/**
 * @brief 检查缓冲区中是否包含期望的响应
 */
static uint8_t Check_Ack(const char *expected) {
    if (expected == NULL || esp8266_cnt == 0) {
        return 0;
    }
    
    // 在接收缓冲区中查找关键字
    if (strstr((char *)esp8266_buf, expected) != NULL) {
        return 1;
    }
    
    return 0;
}

/**
 * @brief 非阻塞执行AT指令（无数据阶段）
 */
AT_Result_t AT_Execute_NonBlocking(const char *cmd, const char *expected_ack, uint32_t timeout_ms) {
    // ✅ 检查忙标志，防止并发调用
    if (g_at_busy && g_at_state != AT_STATE_IDLE) {
        return AT_RESULT_BUSY;
    }
    
    // 状态机逻辑
    switch (g_at_state) {
        case AT_STATE_IDLE:
            // 启动新的AT指令执行
            if (cmd == NULL || expected_ack == NULL) {
                return AT_RESULT_ERROR;
            }
            
            // ✅ 设置忙标志
            g_at_busy = 1;
            
            // 清空AT响应缓冲区
            memset(ESP8266_RecvBuf, 0, sizeof(ESP8266_RecvBuf));
            ESP8266_RecvLen = 0;
            
            // 发送AT指令（使用现有的阻塞发送，因为AT指令本身很短）
            Usart_SendString(Bemfa_USART, (unsigned char *)cmd, strlen(cmd));
            
            g_at_state = AT_STATE_WAITING_ACK;
            g_at_start_time = sys_tick_ms;
            g_at_timeout = timeout_ms;
            g_expected_ack = expected_ack;
            g_at_retry_count = 0;
            
            AT_LOG("[INFO] Executing: %s\r\n", cmd);
            return AT_RESULT_IN_PROGRESS;
            
        case AT_STATE_WAITING_ACK:
            // ✅ 新增：每次进入都输出状态信息，方便诊断超时失效问题
            {
                static uint32_t last_debug_time = 0;
                uint32_t elapsed = sys_tick_ms - g_at_start_time;
                
                // 每500ms输出一次调试信息，避免日志风暴
                if (sys_tick_ms - last_debug_time > 500) {
                    AT_LOG("[DBG] Waiting ACK: elapsed=%lu ms, timeout=%lu ms, recv_len=%d\r\n", 
                           elapsed, g_at_timeout, ESP8266_RecvLen);
                    last_debug_time = sys_tick_ms;
                }
            }
            
            // 检查超时
            if (sys_tick_ms - g_at_start_time > g_at_timeout) {
                g_at_state = AT_STATE_TIMEOUT;
                g_at_busy = 0;  // ✅ 超时时清除忙标志
                AT_LOG("[ERR] Timeout waiting for: %s\r\n", g_expected_ack);
                // ✅ 新增：输出超时时的缓冲区内容，方便诊断
                if (ESP8266_RecvLen > 0) {
                    ESP8266_RecvBuf[ESP8266_RecvLen] = '\0';  // 确保字符串结束
                    AT_LOG("[DBG] Recv buffer (%d bytes): [%s]\r\n", ESP8266_RecvLen, ESP8266_RecvBuf);
                } else {
                    AT_LOG("[DBG] Recv buffer is empty\r\n");
                }
                return AT_RESULT_TIMEOUT;
            }
            
            // 检查是否收到期望响应
            if (Check_Ack(g_expected_ack)) {
                g_at_state = AT_STATE_DONE;
                // ✅ 新增：输出收到的完整响应内容
                ESP8266_RecvBuf[ESP8266_RecvLen] = '\0';  // 确保字符串结束
                AT_LOG("[OK] Received: %s\r\n", g_expected_ack);
                AT_LOG("[DBG] Full response (%d bytes): [%s]\r\n", ESP8266_RecvLen, ESP8266_RecvBuf);
                return AT_RESULT_OK;
            }
            
            // 继续等待
            return AT_RESULT_IN_PROGRESS;
            
        case AT_STATE_DONE:
            // 完成后重置状态
            g_at_state = AT_STATE_IDLE;
            g_at_busy = 0;  // ✅ 完成时清除忙标志
            return AT_RESULT_OK;
            
        case AT_STATE_TIMEOUT:
        case AT_STATE_ERROR:
            // 错误状态，需手动重置
            return AT_RESULT_ERROR;
            
        default:
            return AT_RESULT_BUSY;
    }
}

/**
 * @brief 非阻塞执行AT+CIPSEND两阶段发送
 */
AT_Result_t AT_SendData_NonBlocking(const uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    static uint8_t cipsend_cmd[64];
    
    // ✅ 检查忙标志，防止并发调用
    if (g_at_busy && g_at_state != AT_STATE_IDLE) {
        return AT_RESULT_BUSY;
    }
    
    switch (g_at_state) {
        case AT_STATE_IDLE:
            // 阶段1：发送AT+CIPSEND=len
            if (data == NULL || len == 0) {
                return AT_RESULT_ERROR;
            }
            
            // ✅ 设置忙标志
            g_at_busy = 1;
            
            sprintf((char *)cipsend_cmd, "AT+CIPSEND=%d\r\n", len);
            
            // 清空AT响应缓冲区
            memset(ESP8266_RecvBuf, 0, sizeof(ESP8266_RecvBuf));
            ESP8266_RecvLen = 0;
            
            // 发送CIPSEND指令
            Usart_SendString(Bemfa_USART, cipsend_cmd, strlen((char *)cipsend_cmd));
            
            g_at_state = AT_STATE_SENDING_CMD;
            g_at_start_time = sys_tick_ms;
            g_at_timeout = timeout_ms;
            g_send_data = data;
            g_send_len = len;
            
            AT_LOG("[INFO] CIPSEND phase1: %s\r\n", cipsend_cmd);
            return AT_RESULT_IN_PROGRESS;
            
        case AT_STATE_SENDING_CMD:
            // 检查超时
            if (sys_tick_ms - g_at_start_time > g_at_timeout) {
                g_at_state = AT_STATE_TIMEOUT;
                g_at_busy = 0;  // ✅ 超时时清除忙标志
                AT_LOG("[ERR] Timeout waiting for '>'\r\n");
                return AT_RESULT_TIMEOUT;
            }
            
            // 检查是否收到'>'提示符
            if (Check_Ack(">")) {
                // 阶段2：发送实际数据
                Usart_SendString(Bemfa_USART, (unsigned char *)g_send_data, g_send_len);
                
                g_at_state = AT_STATE_SENDING_DATA;
                g_at_start_time = sys_tick_ms;  // 重置超时计时
                
                AT_LOG("[INFO] CIPSEND phase2: sending %d bytes\r\n", g_send_len);
                return AT_RESULT_IN_PROGRESS;
            }
            
            return AT_RESULT_IN_PROGRESS;
            
        case AT_STATE_SENDING_DATA:
            // 检查超时
            if (sys_tick_ms - g_at_start_time > g_at_timeout) {
                g_at_state = AT_STATE_TIMEOUT;
                g_at_busy = 0;  // ✅ 超时时清除忙标志
                AT_LOG("[ERR] Timeout waiting for 'SEND OK'\r\n");
                return AT_RESULT_TIMEOUT;
            }
            
            // 检查是否收到"SEND OK"
            if (Check_Ack("SEND OK")) {
                g_at_state = AT_STATE_DONE;
                g_at_busy = 0;  // ✅ 完成时清除忙标志
                AT_LOG("[OK] Data sent successfully\r\n");
                return AT_RESULT_OK;
            }
            
            return AT_RESULT_IN_PROGRESS;
            
        case AT_STATE_DONE:
            g_at_state = AT_STATE_IDLE;
            return AT_RESULT_OK;
            
        case AT_STATE_TIMEOUT:
        case AT_STATE_ERROR:
            return AT_RESULT_ERROR;
            
        default:
            return AT_RESULT_BUSY;
    }
}
