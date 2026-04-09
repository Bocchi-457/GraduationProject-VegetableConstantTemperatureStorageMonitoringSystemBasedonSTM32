/**
 * network_manager.c
 * 网络管理模块实现
 * 功能：统一管理ESP8266网络连接、数据上传、指令解析
 * 版本：V1.0
 */

#include "network_manager.h"
#include "esp8266.h"
#include "Timer.h"  // sys_tick_ms
#include <string.h>
#include <stdio.h>
#include <stdlib.h>  // abs()

#if DEBUG_NETWORK
  #include "Usart.h"  // Serial_Printf
  #define NETWORK_LOG(fmt, ...) Serial_Printf("[NET] " fmt, ##__VA_ARGS__)
#else
  #define NETWORK_LOG(fmt, ...)  // 空宏，不产生任何代码
#endif

/**
 * WiFi配置常量（需根据实际网络修改）
 */
#define BEMFA_WIFI_SSID "TheWorld"   
#define BEMFA_WIFI_PASS "2020625663"

/**
 * 内部状态变量
 */
static NetworkState_t g_net_state = NET_DISCONNECTED;
static uint8_t g_reconnect_step = 0;      // 重连子状态（0-9）
static uint8_t g_send_fail_count = 0;     // 发送失败计数
static uint32_t g_last_reconnect_time = 0; // 上次重连时间戳
static CloudCommand_t g_pending_cmd = {CMD_NONE, 0}; // 待处理指令（显式初始化枚举）
static uint8_t g_cmd_available = 0;       // 指令可用标志
static uint8_t g_module_initialized = 0;  // 模块初始化标志

// 时间同步相关
static uint8_t g_time_sync_state = 0;      // 0=空闲, 1=等待响应
static uint32_t g_time_sync_timer = 0;     // 超时计时器
static char g_synced_time[32] = {0};       // 同步后的时间字符串

// 重连防抖与退避
static uint8_t g_reconnect_retry_count = 0;  // 当前重连重试次数
static uint32_t g_reconnect_start_time = 0;  // 重连开始时间戳

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
    
    // ✅ 初始化AT非阻塞执行器
    AT_Executor_Init();
    
    // 初始化状态
    g_net_state = NET_CONNECTING;
    g_reconnect_step = 0;
    g_send_fail_count = 0;
    g_cmd_available = 0;
    g_module_initialized = 1;  // ✅ 标记已初始化
    
    // 时间同步状态
    g_time_sync_state = 0;
    g_synced_time[0] = '\0';
    
    NETWORK_LOG("[INFO] Network manager initialized\r\n");
}

/**
 * @brief 发送心跳包（保持在线）
 * @note 每60秒调用一次，由应用层控制调用时机
 */
void Network_SendHeartbeat(void) {
    if (!g_module_initialized) {
        return;
    }
    
    if (g_net_state != NET_CONNECTED) {
        return;
    }
    
    char data[128];
    sprintf(data, "cmd=2&uid=%s&topic=online&msg=Keep online\r\n", BEMFA_ID);
    
    // 心跳包发送失败不触发重连（避免误判），但记录日志
    if (ESP8266_SendData((unsigned char *)data) != 0) {
        NETWORK_LOG("[WARN] Heartbeat send failed\r\n");
    }
}

/**
 * @brief 网络任务调度
 * @note 此函数被调度器调用，返回类型为void
 */
void Network_Task(void) {
    static uint32_t last_task_time = 0;
    
    // 限流：每50ms执行一次
    if (sys_tick_ms - last_task_time < 50) {
        return;
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
            
            // 处理时间同步超时（自动重试）
            if (g_time_sync_state == 1) {
                if (sys_tick_ms - g_time_sync_timer > 6000) {
                    NETWORK_LOG("[WARN] Time sync timeout, will retry\r\n");
                    
                    // 重置状态，下次调用Network_RequestTimeSync可重新发送
                    g_time_sync_state = 0;
                    
                    // 清空AT响应缓冲区，避免旧数据干扰
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
    // 检查模块是否已初始化
    if (!g_module_initialized) {
        NETWORK_LOG("[ERR] Module not initialized!\r\n");
        return 1;
    }
    
    // 仅在已连接状态下允许上传
    if (g_net_state != NET_CONNECTED) {
        NETWORK_LOG("[ERR] Upload failed: not connected\r\n");
        return 1;
    }
    
    char data[256];
    
    // 构建巴法云数据包（与旧代码格式完全兼容）
    // sprintf的%d格式符自动处理负号，无需手动判断
    sprintf(data, 
        "cmd=2&uid=%s&topic=data&msg=Mode:%d th:%d tl:%d hh:%d hl:%d "
        "jr:%d zl:%d cs:%d js:%d temp:%d.%d humi:%d.%d\r\n",
        BEMFA_ID,
        mode,
        wendu_high, wendu_low,
        shidu_high, shidu_low,
        jiare, zhileng, chushi, jiashi,
        temp_x10 / 10,           // ✅ 自动处理负号
        abs(temp_x10) % 10,      // 取绝对值的个位
        hum_x10 / 10, 
        hum_x10 % 10
    );
    
    // 发送数据（TODO: 阶段2改为非阻塞异步发送）
    if (ESP8266_SendData((unsigned char *)data) == 0) {
        g_send_fail_count = 0;
        return 0;  // 成功
    } else {
        g_send_fail_count++;
        NETWORK_LOG("[ERR] Upload failed, count=%d\r\n", g_send_fail_count);
        
        // 连续3次失败，启动重连
        if (g_send_fail_count >= 3) {
            NETWORK_LOG("[WARN] 3 consecutive failures, triggering reconnect\r\n");
            g_net_state = NET_RECONNECTING;
            g_reconnect_step = 0;
        }
        
        return 1;  // 失败
    }
}

/**
 * @brief 处理云端指令
 * @param cmd: 输出参数，返回解析后的指令
 * @return 0=有指令, 1=无指令
 */
uint8_t Network_ProcessCloudCommand(CloudCommand_t *cmd) {
    if (!g_module_initialized) {
        return 1;
    }
    
    if (!g_cmd_available) {
        return 1;  // 无可用指令
    }
    
    // 拷贝指令到输出参数
    if (cmd != NULL) {
        memcpy(cmd, &g_pending_cmd, sizeof(CloudCommand_t));
    }
    
    // 清除标志
    g_cmd_available = 0;
    
    return 0;  // 成功返回指令
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
    if (!g_module_initialized) {
        NETWORK_LOG("[ERR] Module not initialized!\r\n");
        return 1;
    }
    
    if (g_net_state != NET_CONNECTED) {
        NETWORK_LOG("[ERR] Time sync failed: not connected\r\n");
        return 1;  // 未连接
    }
    
    // 如果正在等待响应，不允许重复请求
    if (g_time_sync_state == 1) {
        NETWORK_LOG("[WARN] Time sync already in progress\r\n");
        return 1;
    }
    
    // 发送时间同步指令
    if (ESP8266_SendData((unsigned char *)Return_Time) == 0) {
        g_time_sync_state = 1;
        g_time_sync_timer = sys_tick_ms;
        NETWORK_LOG("[INFO] Time sync requested\r\n");
        return 0;  // 成功
    }
    
    NETWORK_LOG("[ERR] Time sync request failed\r\n");
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
 * 
 * 状态流转：
 * Step 0: 诊断TCP连接状态
 * Step 1: TCP断开 → 尝试TCP重连
 * Step 2: TCP重连成功 → 订阅主题
 * Step 3: 订阅成功 → 完成重连
 * Step 4: TCP重连失败 → 检测WiFi状态
 * Step 5: WiFi断开 → 尝试WiFi重连
 * Step 6: WiFi重连成功 → 回到Step 1
 * Step 7: WiFi正常 → 模块复位（终极兜底）
 * Step 8: 模块复位后重新初始化
 * Step 9: 初始化完成 → 回到Step 0
 */
static void Reconnect_StateMachine(void) {
    AT_Result_t result;
    static uint8_t warn_logged = 0;  // ✅ 防止重复输出警告
    
    // 防抖：最小重连间隔60秒（模块复位120秒）
    if (sys_tick_ms - g_last_reconnect_time < 60000 && g_reconnect_step < 7) {
        if (!warn_logged) {  // ✅ 仅在首次输出
            NETWORK_LOG("[WARN] Reconnect too frequent, wait %d ms\r\n", 
                       60000 - (sys_tick_ms - g_last_reconnect_time));
            warn_logged = 1;
        }
        return;
    }
    
    warn_logged = 0;  // ✅ 重置标志，允许下次输出
    
    switch (g_reconnect_step) {
        case 0:  // 诊断阶段：检查TCP连接状态
            {
                NETWORK_LOG("[DIAG] Checking TCP status...\r\n");
                
                // 使用AT+CIPSTATUS诊断
                result = AT_Execute_NonBlocking("AT+CIPSTATUS\r\n", "STATUS:", 2000);
                
                if (result == AT_RESULT_OK) {
                    // 检查响应中是否包含"STATUS:2"或"STATUS:3"（已连接）
                    if (strstr((char *)ESP8266_RecvBuf, "STATUS:2") != NULL ||
                        strstr((char *)ESP8266_RecvBuf, "STATUS:3") != NULL) {
                        NETWORK_LOG("[OK] TCP still connected\r\n");
                        g_net_state = NET_CONNECTED;
                        g_send_fail_count = 0;
                        g_reconnect_step = 0;
                        return;
                    } else {
                        NETWORK_LOG("[ERR] TCP disconnected\r\n");
                        g_reconnect_step = 1;  // 进入TCP重连
                    }
                } else if (result == AT_RESULT_TIMEOUT) {
                    NETWORK_LOG("[ERR] Diagnosis timeout\r\n");
                    g_reconnect_step = 4;  // 直接进入WiFi检测
                }
                // result == IN_PROGRESS 时继续等待
            }
            break;
            
        case 1:  // TCP层重连
            {
                NETWORK_LOG("[RECONNECT] TCP layer reconnecting (retry=%d)...\r\n", g_reconnect_retry_count);
                
                // 步骤1.1: 关闭当前连接
                result = AT_Execute_NonBlocking("AT+CIPCLOSE\r\n", "CLOSED", 2000);
                if (result == AT_RESULT_IN_PROGRESS) return;  // ✅ 等待完成
                if (result != AT_RESULT_OK) {
                    NETWORK_LOG("[WARN] CIPCLOSE failed, continue anyway\r\n");
                }
                
                // 步骤1.2: 重新建立TCP连接
                static char connect_cmd[128];  // ✅ 使用static避免栈溢出
                sprintf(connect_cmd, "AT+CIPSTART=\"TCP\",\"bemfa.com\",8344\r\n");
                result = AT_Execute_NonBlocking(connect_cmd, "CONNECT", 5000);
                
                if (result == AT_RESULT_IN_PROGRESS) {
                    return;  // ✅ 等待响应
                } else if (result == AT_RESULT_OK) {
                    NETWORK_LOG("[OK] TCP reconnected\r\n");
                    g_reconnect_step = 2;  // 进入订阅阶段
                    g_reconnect_retry_count = 0;
                } else if (result == AT_RESULT_TIMEOUT) {
                    g_reconnect_retry_count++;
                    if (g_reconnect_retry_count >= 3) {
                        NETWORK_LOG("[ERR] TCP reconnect failed after 3 retries\r\n");
                        g_reconnect_step = 4;  // 进入WiFi检测
                        g_reconnect_retry_count = 0;
                    }
                    // 否则继续重试（保持step=1）
                } else {
                    // ERROR或其他异常
                    NETWORK_LOG("[ERR] TCP reconnect error\r\n");
                    g_reconnect_step = 4;
                    g_reconnect_retry_count = 0;
                }
            }
            break;
            
        case 2:  // 订阅主题
            {
                NETWORK_LOG("[SUBSCRIBE] Subscribing to topic...\r\n");
                
                static char sub_cmd[128];  // ✅ 使用static避免栈溢出
                sprintf(sub_cmd, "cmd=1&uid=%s&topic=data\r\n", BEMFA_ID);
                
                // 使用AT+CIPSEND两阶段发送
                result = AT_SendData_NonBlocking((uint8_t *)sub_cmd, strlen(sub_cmd), 3000);
                
                if (result == AT_RESULT_IN_PROGRESS) {
                    return;  // ✅ 等待两阶段发送完成
                } else if (result == AT_RESULT_OK) {
                    NETWORK_LOG("[OK] Topic subscribed\r\n");
                    g_reconnect_step = 3;  // 重连完成
                } else if (result == AT_RESULT_TIMEOUT) {
                    NETWORK_LOG("[ERR] Subscribe timeout\r\n");
                    g_reconnect_step = 1;  // 退回TCP重连
                } else {
                    NETWORK_LOG("[ERR] Subscribe error\r\n");
                    g_reconnect_step = 1;
                }
            }
            break;
            
        case 3:  // 重连完成
            {
                NETWORK_LOG("[SUCCESS] Reconnection completed!\r\n");
                
                // ✅ 关键：清空应用层缓冲区，避免AT回显污染
                memset(esp8266_buf, 0, sizeof(esp8266_buf));
                esp8266_cnt = 0;
                
                g_net_state = NET_CONNECTED;
                g_send_fail_count = 0;
                g_reconnect_step = 0;
                g_reconnect_retry_count = 0;
                g_last_reconnect_time = sys_tick_ms;
            }
            break;
            
        case 4:  // 检测WiFi状态
            {
                NETWORK_LOG("[DIAG] Checking WiFi status...\r\n");
                
                result = AT_Execute_NonBlocking("AT+CWJAP?\r\n", "+CWJAP:", 2000);
                
                if (result == AT_RESULT_IN_PROGRESS) {
                    return;  // ✅ 等待响应
                } else if (result == AT_RESULT_OK) {
                    // WiFi已连接
                    if (strstr((char *)ESP8266_RecvBuf, BEMFA_WIFI_SSID) != NULL) {
                        NETWORK_LOG("[OK] WiFi connected, but TCP failed\r\n");
                        g_reconnect_step = 7;  // WiFi正常但TCP失败，进入模块复位
                    } else {
                        NETWORK_LOG("[ERR] WiFi connected to wrong AP\r\n");
                        g_reconnect_step = 5;  // 进入WiFi重连
                    }
                } else if (result == AT_RESULT_TIMEOUT) {
                    NETWORK_LOG("[ERR] WiFi disconnected or timeout\r\n");
                    g_reconnect_step = 5;  // 进入WiFi重连
                } else {
                    NETWORK_LOG("[ERR] WiFi check error\r\n");
                    g_reconnect_step = 5;
                }
            }
            break;
            
        case 5:  // WiFi层重连
            {
                NETWORK_LOG("[RECONNECT] WiFi layer reconnecting (retry=%d)...\r\n", g_reconnect_retry_count);
                
                static char wifi_cmd[128];  // ✅ 使用static避免栈溢出
                sprintf(wifi_cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", 
                       BEMFA_WIFI_SSID, BEMFA_WIFI_PASS);
                
                result = AT_Execute_NonBlocking(wifi_cmd, "WIFI CONNECTED", 10000);
                
                if (result == AT_RESULT_IN_PROGRESS) {
                    return;  // ✅ 等待响应
                } else if (result == AT_RESULT_OK) {
                    NETWORK_LOG("[OK] WiFi reconnected\r\n");
                    g_reconnect_step = 1;  // 回到TCP重连
                    g_reconnect_retry_count = 0;
                } else if (result == AT_RESULT_TIMEOUT) {
                    g_reconnect_retry_count++;
                    if (g_reconnect_retry_count >= 2) {
                        NETWORK_LOG("[ERR] WiFi reconnect failed after 2 retries\r\n");
                        g_reconnect_step = 7;  // 进入模块复位
                        g_reconnect_retry_count = 0;
                    }
                    // 否则继续重试
                } else {
                    NETWORK_LOG("[ERR] WiFi reconnect error\r\n");
                    g_reconnect_step = 7;
                    g_reconnect_retry_count = 0;
                }
            }
            break;

        case 7:  // 模块复位（终极兜底）
            {
                // 防抖：模块复位最小间隔120秒
                if (sys_tick_ms - g_last_reconnect_time < 120000) {
                    NETWORK_LOG("[WARN] Module reset too frequent, waiting %d ms...\r\n", 
                               120000 - (sys_tick_ms - g_last_reconnect_time));
                    return;
                }
                
                NETWORK_LOG("[RESET] Triggering module reset...\r\n");
                
                // 发送复位指令
                result = AT_Execute_NonBlocking("AT+RST\r\n", "ready", 5000);
                
                if (result == AT_RESULT_IN_PROGRESS) {
                    return;  // ✅ 等待响应
                } else if (result == AT_RESULT_OK) {
                    NETWORK_LOG("[OK] Module reset triggered\r\n");
                    g_reconnect_step = 8;
                    g_reconnect_start_time = sys_tick_ms;
                } else {
                    NETWORK_LOG("[ERR] Reset command failed, retry later\r\n");
                    // 保持当前状态，下次继续尝试
                }
            }
            break;

        case 8:  // 等待模块重启完成
            {
                // 等待至少5秒让模块完全启动
                if (sys_tick_ms - g_reconnect_start_time < 5000) {
                    return;  // 继续等待
                }
                
                NETWORK_LOG("[INIT] Re-initializing ESP8266...\r\n");
                
                // 重新初始化ESP8266
                ESP8266_Init(115200);
                
                g_reconnect_step = 9;
                g_reconnect_start_time = sys_tick_ms;
            }
            break;
            
        case 9:  // 等待初始化完成后重新开始
            {
                // 等待至少3秒让AT指令就绪
                if (sys_tick_ms - g_reconnect_start_time < 3000) {
                    return;
                }
                
                NETWORK_LOG("[READY] Module ready, restarting diagnosis...\r\n");
                g_reconnect_step = 0;  // 回到诊断阶段
                g_last_reconnect_time = sys_tick_ms;
            }
            break;
            
        default:
            NETWORK_LOG("[ERR] Invalid reconnect step: %d\r\n", g_reconnect_step);
            g_reconnect_step = 0;
            break;
    }
}

/**
 * @brief 解析云端指令
 * @note 从esp8266_buf中提取有效指令，过滤AT响应和回显
 * 
 * 支持的指令格式：
 * - ZD:0/1 (自动模式开关)
 * - SD:0/1 (手动模式开关)
 * - KJR:0/1 (加热控制)
 * - LZ:0/1 (制冷控制)
 * - CS:0/1 (除湿控制)
 * - JS:0/1 (加湿控制)
 * - TH:xxx (温度上限)
 * - TL:xxx (温度下限)
 * - HH:xxx (湿度上限)
 * - HL:xxx (湿度下限)
 */
static void Parse_Cloud_Command(void) {
    char *payload_start;
    char *msg_end;
    uint16_t payload_len;
    uint16_t remaining_len;
    
    // 1. 检查是否有数据
    if (esp8266_cnt == 0) {
        return;
    }
    
    // 2. 过滤AT响应和回显，定位有效载荷
    // 巴法云指令格式：cmd=1&uid=xxx&topic=data&msg=ZD:1
    payload_start = strstr((char *)esp8266_buf, "msg=");
    
    if (payload_start == NULL) {
        // 无有效载荷，清空缓冲区
        NETWORK_LOG("[DEBUG] No valid payload, clearing buffer\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    // 3. 提取msg后的内容
    payload_start += 4;  // 跳过"msg="
    
    // ✅ 计算剩余缓冲区长度，防止越界
    remaining_len = esp8266_cnt - (payload_start - (char *)esp8266_buf);
    if (remaining_len == 0) {
        NETWORK_LOG("[WARN] Empty payload after msg=\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    // 查找消息结束位置（\r\n或缓冲区末尾）
    msg_end = strstr(payload_start, "\r\n");
    if (msg_end != NULL) {
        payload_len = msg_end - payload_start;
    } else {
        payload_len = remaining_len;  // ✅ 使用剩余长度而非strlen
    }
    
    // ✅ 边界检查
    if (payload_len > remaining_len) {
        payload_len = remaining_len;
    }
    
    // ✅ 检查空载荷
    if (payload_len == 0) {
        NETWORK_LOG("[WARN] Empty payload\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    // 4. 调试模式下拷贝有效载荷到结构体
#if DEBUG_NETWORK
    if (payload_len >= sizeof(g_pending_cmd.payload)) {
        payload_len = sizeof(g_pending_cmd.payload) - 1;
    }
    strncpy(g_pending_cmd.payload, payload_start, payload_len);
    g_pending_cmd.payload[payload_len] = '\0';
    NETWORK_LOG("[CMD] Received: %s\r\n", g_pending_cmd.payload);
#endif
    
    // 5. 解析具体指令（直接使用payload_start指针）
    if (strncmp(payload_start, "ZD:", 3) == 0) {
        int value = atoi(payload_start + 3);
        
        // ✅ 校验数值范围
        if (value < 0 || value > 1) {
            NETWORK_LOG("[ERR] Invalid ZD value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_AUTO_MODE;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Auto mode: %d\r\n", value);
        
    } else if (strncmp(payload_start, "SD:", 3) == 0) {
        int value = atoi(payload_start + 3);
        
        if (value < 0 || value > 1) {
            NETWORK_LOG("[ERR] Invalid SD value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_MANUAL_MODE;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Manual mode: %d\r\n", value);
        
    } else if (strncmp(payload_start, "KJR:", 4) == 0) {
        int value = atoi(payload_start + 4);
        
        if (value < 0 || value > 1) {
            NETWORK_LOG("[ERR] Invalid KJR value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_HEATER;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Heater: %d\r\n", value);
        
    } else if (strncmp(payload_start, "LZ:", 3) == 0) {
        int value = atoi(payload_start + 3);
        
        if (value < 0 || value > 1) {
            NETWORK_LOG("[ERR] Invalid LZ value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_COOLER;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Cooler: %d\r\n", value);
        
    } else if (strncmp(payload_start, "CS:", 3) == 0) {
        int value = atoi(payload_start + 3);
        
        if (value < 0 || value > 1) {
            NETWORK_LOG("[ERR] Invalid CS value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_DEHUMIDIFIER;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Dehumidifier: %d\r\n", value);
        
    } else if (strncmp(payload_start, "JS:", 3) == 0) {
        int value = atoi(payload_start + 3);
        
        if (value < 0 || value > 1) {
            NETWORK_LOG("[ERR] Invalid JS value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_HUMIDIFIER;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Humidifier: %d\r\n", value);
        
    } else if (strncmp(payload_start, "TH:", 3) == 0) {
        int value = atoi(payload_start + 3);
        
        // 温度范围：-50℃ ~ 100℃（放大10倍后：-500 ~ 1000）
        if (value < -500 || value > 1000) {
            NETWORK_LOG("[ERR] Invalid TH value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_TEMP_HIGH;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Temp high: %d\r\n", value);
        
    } else if (strncmp(payload_start, "TL:", 3) == 0) {
        int value = atoi(payload_start + 3);
        
        if (value < -500 || value > 1000) {
            NETWORK_LOG("[ERR] Invalid TL value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_TEMP_LOW;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Temp low: %d\r\n", value);
        
    } else if (strncmp(payload_start, "HH:", 3) == 0) {
        int value = atoi(payload_start + 3);
        
        // 湿度范围：0% ~ 100%（放大10倍后：0 ~ 1000）
        if (value < 0 || value > 1000) {
            NETWORK_LOG("[ERR] Invalid HH value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_HUM_HIGH;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Humidity high: %d\r\n", value);
        
    } else if (strncmp(payload_start, "HL:", 3) == 0) {
        int value = atoi(payload_start + 3);
        
        if (value < 0 || value > 1000) {
            NETWORK_LOG("[ERR] Invalid HL value: %d\r\n", value);
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
        
        g_pending_cmd.type = CMD_HUM_LOW;
        g_pending_cmd.value = (int16_t)value;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Humidity low: %d\r\n", value);
        
    } else {
        NETWORK_LOG("[WARN] Unknown command\r\n");
    }
    
    // 6. 清空接收缓冲区，准备下一次接收
    memset(esp8266_buf, 0, esp8266_cnt);
    esp8266_cnt = 0;
}
