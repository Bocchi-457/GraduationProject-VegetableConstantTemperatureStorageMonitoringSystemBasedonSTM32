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
  #define DEBUG_AT_EXECUTOR  1  // ✅ 临时启用：诊断超时失效问题
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
// static uint8_t g_at_retry_count = 0;  // ✅ 已删除：未使用的变量

// CIPSEND两阶段发送相关
static const uint8_t *g_send_data = NULL;
static uint16_t g_send_len = 0;

// ✅ 新增：调用计数器，用于诊断日志时序问题
static uint32_t g_at_call_count = 0;

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
 * @note 用于异常情况下的状态恢复
 */
void AT_Reset(void) {
    g_at_state = AT_STATE_IDLE;
    g_at_busy = 0;
    g_at_start_time = 0;
    g_at_timeout = 0;
    g_expected_ack = NULL;
    g_send_data = NULL;
    g_send_len = 0;
    
    // 清空接收缓冲区
    memset(ESP8266_RecvBuf, 0, sizeof(ESP8266_RecvBuf));
    ESP8266_RecvLen = 0;
    
    AT_LOG("[INFO] AT executor reset\r\n");
}

/**
 * @brief 获取AT执行器忙标志
 * @return 1=忙（正在执行AT指令），0=空闲
 * @note 用于防止并发调用AT指令导致状态机冲突
 */
uint8_t AT_IsBusy(void) {
    return g_at_busy;
}

/**
 * @brief 获取AT执行器调用计数器（用于诊断日志时序）
 * @return 当前调用次数
 */
uint32_t AT_GetCallCount(void) {
    return g_at_call_count;
}

/**
 * @brief 检查是否收到期望的AT响应
 * @note ✅ P0修复：读取ESP8266_RecvBuf（驱动层AT响应缓冲区），而非esp8266_buf（应用层云端指令缓冲区）
 */
static uint8_t Check_Ack(const char *expected) {
    if (expected == NULL || ESP8266_RecvLen == 0) {
        return 0;
    }
    
    // ✅ 在AT响应缓冲区中查找关键字
    if (strstr((char *)ESP8266_RecvBuf, expected) != NULL) {
        return 1;
    }
    
    return 0;
}

/**
 * @brief 非阻塞执行AT指令（无数据阶段）
 * @note 可以重复调用以轮询状态机进度（幂等性）
 */
AT_Result_t AT_Execute_NonBlocking(const char *cmd, const char *expected_ack, uint32_t timeout_ms) {
    // ✅ 新增：每次调用递增计数器
    g_at_call_count++;
    uint32_t call_id = g_at_call_count;
    
    // ✅ P0修复：如果处于等待响应状态，继续轮询状态机而非返回BUSY
    if (g_at_state == AT_STATE_WAITING_ACK) {
        // 跳过IDLE分支，直接进入状态机轮询
        goto POLL_STATE_MACHINE;
    }
    
    // ✅ 检查忙标志，防止并发调用（仅在IDLE状态下才允许新指令）
    if (g_at_busy && g_at_state != AT_STATE_IDLE) {
        // ✅ P1修复：每2秒输出一次，避免日志风暴
        static uint32_t last_busy_log_time = 0;
        if (sys_tick_ms - last_busy_log_time > 2000) {
            AT_LOG("[DBG] Call#%lu Busy check: g_at_busy=%d, g_at_state=%d\r\n", 
                   call_id, g_at_busy, g_at_state);
            last_busy_log_time = sys_tick_ms;
        }
        return AT_RESULT_BUSY;
    }
    
POLL_STATE_MACHINE:
    // ✅ P0调试：输出进入状态机时的状态
    AT_LOG("[DBG] Call#%lu Entering state machine: g_at_state=%d, g_at_busy=%d\r\n", 
           call_id, g_at_state, g_at_busy);
    
    // 状态机逻辑
    switch (g_at_state) {
        case AT_STATE_IDLE:
            // 启动新的AT指令执行
            if (cmd == NULL || expected_ack == NULL) {
                AT_LOG("[ERR] Call#%lu Invalid parameters\r\n", call_id);
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
            
            AT_LOG("[INFO] Call#%lu Executing: %s\r\n", call_id, cmd);
            AT_LOG("[DBG] Call#%lu State transition: IDLE -> WAITING_ACK\r\n", call_id);
            AT_LOG("[DBG] Call#%lu Returning IN_PROGRESS (1)\r\n", call_id);  // ✅ 调试：确认返回值
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
            AT_LOG("[DBG] State transition: DONE -> IDLE\r\n");
            return AT_RESULT_OK;
            
        case AT_STATE_TIMEOUT:
            AT_LOG("[ERR] Timeout in AT command, auto-reset to IDLE\r\n");
            g_at_state = AT_STATE_IDLE;  // ✅ 自动重置状态
            g_at_busy = 0;
            return AT_RESULT_TIMEOUT;  // ✅ 返回TIMEOUT而非ERROR
            
        case AT_STATE_ERROR:
            AT_LOG("[ERR] Error in AT command, auto-reset to IDLE\r\n");
            g_at_state = AT_STATE_IDLE;  // ✅ 自动重置状态
            g_at_busy = 0;
            return AT_RESULT_ERROR;
            
        default:
            AT_LOG("[ERR] Unknown state in AT executor: %d\r\n", g_at_state);
            g_at_state = AT_STATE_IDLE;  // ✅ 未知状态也重置
            g_at_busy = 0;
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
            // ✅ 新增：每500ms输出一次调试信息
            {
                static uint32_t last_debug_time = 0;
                if (sys_tick_ms - last_debug_time > 500) {
                    AT_LOG("[DBG] Waiting '>': elapsed=%lu ms, recv_len=%d\r\n", 
                           sys_tick_ms - g_at_start_time, ESP8266_RecvLen);
                    if (ESP8266_RecvLen > 0 && ESP8266_RecvLen < 100) {
                        ESP8266_RecvBuf[ESP8266_RecvLen] = '\0';
                        AT_LOG("[DBG] Recv buffer: [%s]\r\n", ESP8266_RecvBuf);
                    }
                    last_debug_time = sys_tick_ms;
                }
            }
            
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
            AT_LOG("[ERR] Timeout in CIPSEND, auto-reset to IDLE\r\n");
            g_at_state = AT_STATE_IDLE;  // ✅ 自动重置状态
            g_at_busy = 0;
            return AT_RESULT_TIMEOUT;
            
        case AT_STATE_ERROR:
            AT_LOG("[ERR] Error in CIPSEND, auto-reset to IDLE\r\n");
            g_at_state = AT_STATE_IDLE;  // ✅ 自动重置状态
            g_at_busy = 0;
            return AT_RESULT_ERROR;
            
        default:
            AT_LOG("[ERR] Unknown state in CIPSEND: %d\r\n", g_at_state);
            g_at_state = AT_STATE_IDLE;  // ✅ 未知状态也重置
            g_at_busy = 0;
            return AT_RESULT_BUSY;
    }
}
