/**
 * network_manager.c
 * 网络管理模块实现
 * 功能：统一管理ESP8266网络连接、数据上传、指令解析
 * 版本：V1.0
 */

#include "network_manager.h"
#include "../Hardware/esp8266.h"
#include "../Hardware/Timer.h"  // sys_tick_ms
#include <string.h>
#include <stdio.h>

/**
 * 内部状态变量
 */
static NetworkState_t g_net_state = NET_DISCONNECTED;
static uint8_t g_reconnect_step = 0;      // 重连子状态
static uint8_t g_send_fail_count = 0;     // 发送失败计数
static uint32_t g_last_reconnect_time = 0; // 上次重连时间
static CloudCommand_t g_pending_cmd = {0}; // 待处理指令
static uint8_t g_cmd_available = 0;       // 指令可用标志

// 时间同步相关
static uint8_t g_time_sync_requested = 0;  // 时间同步请求标志
static uint8_t g_time_sync_state = 0;      // 0=空闲, 1=等待响应
static uint32_t g_time_sync_timer = 0;     // 超时计时器
static char g_synced_time[32] = {0};       // 同步后的时间字符串

/**
 * 内部辅助函数声明
 */
static void Reconnect_StateMachine(void);
static void Parse_Cloud_Command(void);

/**
 * @brief 初始化网络管理模块
 */
void Network_Manager_Init(void) {
    // ESP8266硬件初始化（仅上电调用一次）
    ESP8266_Init(115200);
    
    // 初始化状态
    g_net_state = NET_CONNECTING;
    g_reconnect_step = 0;
    g_send_fail_count = 0;
    g_cmd_available = 0;
    
    // 时间同步状态
    g_time_sync_requested = 0;
    g_time_sync_state = 0;
    g_synced_time[0] = '\0';
}

/**
 * @brief 发送心跳包（保持在线）
 * @note 每60秒调用一次，由应用层控制调用时机
 */
void Network_SendHeartbeat(void) {
    if (g_net_state != NET_CONNECTED) {
        return;
    }
    
    char data[128];
    sprintf(data, "cmd=2&uid=%s&topic=online&msg=Keep online\r\n", BEMFA_ID);
    
    // 心跳包发送失败不触发重连（避免误判）
    ESP8266_SendData((unsigned char *)data);
}

/**
 * @brief 网络任务调度
 */
NetworkState_t Network_Task(void) {
    static uint32_t last_task_time = 0;
    
    // 限流：每50ms执行一次
    if (sys_tick_ms - last_task_time < 50) {
        return g_net_state;
    }
    last_task_time = sys_tick_ms;
    
    switch (g_net_state) {
        case NET_CONNECTING:
        case NET_RECONNECTING:
            // 执行非阻塞重连状态机
            Reconnect_StateMachine();
            break;
            
        case NET_CONNECTED:
            // 解析云端指令
            Parse_Cloud_Command();
            
            // 处理时间同步超时
            if (g_time_sync_state == 1) {
                if (sys_tick_ms - g_time_sync_timer > 6000) {
                    // 超时，重置状态
                    g_time_sync_state = 0;
                    g_time_sync_requested = 0;
                    
                    // 清空AT响应缓冲区
                    memset(ESP8266_RecvBuf, 0, buf_len);
                    ESP8266_RecvLen = 0;
                }
            }
            break;
            
        case NET_OFFLINE:
            // 每30秒尝试重连
            if (sys_tick_ms - g_last_reconnect_time > 30000) {
                g_net_state = NET_RECONNECTING;
                g_reconnect_step = 0;
            }
            break;
            
        default:
            break;
    }
    
    return g_net_state;
}

/**
 * @brief 获取当前网络状态
 */
NetworkState_t Network_GetState(void) {
    return g_net_state;
}

/**
 * @brief 上传传感器数据
 */
uint8_t Network_UploadSensorData(int16_t temp_x10, int16_t hum_x10, 
                                  uint8_t mode,
                                  int16_t wendu_high, int16_t wendu_low,
                                  int16_t shidu_high, int16_t shidu_low,
                                  uint8_t jiare, uint8_t zhileng,
                                  uint8_t chushi, uint8_t jiashi) {
    // 仅在已连接状态下允许上传
    if (g_net_state != NET_CONNECTED) {
        return 1;
    }
    
    char data[256];
    
    // 处理温度符号
    char sign_str[2] = "";
    int16_t abs_temp = temp_x10;
    if (temp_x10 < 0) {
        strcpy(sign_str, "-");
        abs_temp = -temp_x10;
    }
    
    // 构建巴法云数据包（与旧代码格式完全兼容）
    sprintf(data, 
        "cmd=2&uid=%s&topic=data&msg=Mode:%d th:%d tl:%d hh:%d hl:%d "
        "jr:%d zl:%d cs:%d js:%d temp:%s%d.%d humi:%d.%d\r\n",
        BEMFA_ID,
        mode,
        wendu_high, wendu_low,
        shidu_high, shidu_low,
        jiare, zhileng, chushi, jiashi,
        sign_str,
        abs_temp / 10, abs_temp % 10,
        hum_x10 / 10, hum_x10 % 10
    );
    
    // 发送数据（TODO: 改为非阻塞异步发送）
    if (ESP8266_SendData((unsigned char *)data) == 0) {
        g_send_fail_count = 0;
        return 0;  // 成功
    } else {
        g_send_fail_count++;
        
        // 连续3次失败，启动重连
        if (g_send_fail_count >= 3) {
            g_net_state = NET_RECONNECTING;
            g_reconnect_step = 0;
        }
        
        return 1;  // 失败
    }
}

/**
 * @brief 处理云端下发的指令
 */
uint8_t Network_ProcessCloudCommand(CloudCommand_t *cmd) {
    if (g_cmd_available) {
        // 拷贝指令到输出参数
        memcpy(cmd, &g_pending_cmd, sizeof(CloudCommand_t));
        
        // 清除标志
        g_cmd_available = 0;
        
        return 0;  // 有新指令
    }
    
    return 1;  // 无指令
}

/**
 * @brief 强制触发重连
 */
void Network_ForceReconnect(void) {
    g_net_state = NET_RECONNECTING;
    g_reconnect_step = 0;
    g_send_fail_count = 0;
}

/**
 * @brief 请求时间同步
 */
uint8_t Network_RequestTimeSync(void) {
    if (g_net_state != NET_CONNECTED) {
        return 1;  // 未连接
    }
    
    // 发送时间同步指令
    if (ESP8266_SendData((unsigned char *)Return_Time) == 0) {
        g_time_sync_requested = 1;
        g_time_sync_state = 1;
        g_time_sync_timer = sys_tick_ms;
        return 0;  // 成功
    }
    
    return 1;  // 失败
}

/**
 * @brief 检查时间同步是否完成并获取结果
 */
uint8_t Network_GetSyncedTime(char *time_str) {
    // 检查是否有已同步的时间
    if (g_synced_time[0] != '\0') {
        strcpy(time_str, g_synced_time);
        g_synced_time[0] = '\0';  // 清除，避免重复读取
        return 0;  // 有数据
    }
    
    return 1;  // 无数据
}

// ============================================================================
// 以下为内部辅助函数（需在后续阶段实现）
// ============================================================================

/**
 * @brief 重连状态机（非阻塞）
 * @note 每次调用只执行一步，然后立即返回
 */
static void Reconnect_StateMachine(void) {
    // TODO: 实现完整的非阻塞重连状态机
    // 包括：诊断 → TCP重连 → WiFi重连 → 模块复位
    
    // 临时占位实现
    g_net_state = NET_CONNECTED;
}

/**
 * @brief 解析云端指令
 * @note 从esp8266_buf中提取有效指令，过滤AT响应和回显
 */
static void Parse_Cloud_Command(void) {
    // TODO: 实现指令解析逻辑
    // 1. 检查esp8266_cnt > 0
    // 2. 过滤AT响应（SEND OK, CONNECT等）
    // 3. 提取有效载荷（ZD/SD/KJR等）
    // 4. 填充g_pending_cmd并设置g_cmd_available
}
