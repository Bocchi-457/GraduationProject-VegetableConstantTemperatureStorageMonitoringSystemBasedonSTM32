/**
 * network_manager.c
 * 网络管理模块实现
 * 功能：统一管理ESP8266网络连接、数据上传、指令解析
 * 版本：V1.0
 */

#include "network_manager.h"
#include "esp8266.h"
#include "control_task.h"  // ✅ 新增：访问g_temp_high等全局变量和阈值设置函数
#include "Timer.h"  // sys_tick_ms
#include <string.h>
#include <stdio.h>
#include <stdlib.h>  // abs(), atof()

#if DEBUG_NETWORK
  #include "Usart.h"  // Serial_Printf
  #define NETWORK_LOG(fmt, ...) Serial_Printf("[NET] " fmt, ##__VA_ARGS__)
#else
  #define NETWORK_LOG(fmt, ...)  // 空宏，不产生任何代码
#endif

/**
 * WiFi配置常量（需根据实际网络修改）
 */
#define BEMFA_WIFI_SSID "bocchi457"
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

// ✅ 新增：数据上传状态跟踪（非阻塞）
typedef enum {
    UPLOAD_STATE_IDLE = 0,      // 空闲
    UPLOAD_STATE_SENDING,       // 发送中
    UPLOAD_STATE_SUCCESS,       // 发送成功
    UPLOAD_STATE_FAILED         // 发送失败
} UploadState_t;

static UploadState_t g_upload_state = UPLOAD_STATE_IDLE;
static uint32_t g_upload_start_time = 0;  // 上传开始时间戳

/**
 * 内部辅助函数声明
 */
static void Reconnect_StateMachine(void);
static void Parse_Cloud_Command(void);
static uint8_t Poll_Upload_Result(void);  // ✅ 新增：上传结果轮询

/**
 * @brief 初始化网络管理模块
 */
void Network_Manager_Init(void) {
    // ESP8266硬件初始化（仅上电调用一次）
    // ⚠️ 注意：ESP8266_Init是阻塞式的，会完成WiFi连接、TCP连接、主题订阅
    ESP8266_Init(115200);
    
    // ✅ 初始化AT非阻塞执行器
    AT_Executor_Init();
    
    // ✅ 关键修复：ESP8266_Init成功后，直接设置为已连接状态
    // 避免立即进入重连状态机导致不必要的诊断
    g_net_state = NET_CONNECTED;
    g_reconnect_step = 0;
    g_send_fail_count = 0;
    g_cmd_available = 0;
    g_module_initialized = 1;  // ✅ 标记已初始化
    
    // 时间同步状态
    g_time_sync_state = 0;
    g_synced_time[0] = '\0';
    
    NETWORK_LOG("[INFO] Network manager initialized (CONNECTED)\r\n");
}

/**
 * @brief 发送心跳包（保持在线）
 * @note 根据巴法云协议，每次成功的数据上传即视为心跳，无需单独发送
 *       此函数保留仅为API兼容，实际不做任何操作
 */
void Network_SendHeartbeat(void) {
    // ✅ 空实现：数据上传已作为心跳，无需额外发送
    // 如果确实需要独立心跳，可在此处添加非阻塞发送逻辑
}

/**
 * @brief 网络任务调度（非阻塞，每50ms执行一次）
 * @note 此函数被调度器调用，负责处理重连状态机、上传结果轮询和指令解析
 */
void Network_Task(void) {
    static uint32_t last_call_time = 0;
    
    // 仅在联网模式下执行
    extern uint8_t g_network_enabled;
    if (!g_network_enabled) {
        return;
    }
    
    // ✅ 轮询上传结果（优先级最高）
    Poll_Upload_Result();
    
    // ✅ P0修复：如果正在重连，调用重连状态机
    if (g_send_fail_count >= 3 || g_net_state != NET_CONNECTED) {
        Reconnect_StateMachine();
    }
    
    // ✅ 定期解析云端指令（每100ms）
    if (sys_tick_ms - last_call_time > 100) {
        Parse_Cloud_Command();
        last_call_time = sys_tick_ms;
    }
}

/**
 * @brief 获取当前网络状态
 */
NetworkState_t Network_GetState(void) {
    return g_net_state;
}

/**
 * @brief 上传传感器数据（非阻塞版本）
 * @note 调用此函数启动上传，需在Network_Task中轮询结果
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
    
    // ✅ 如果上次上传还在进行中，跳过本次上传
    if (g_upload_state == UPLOAD_STATE_SENDING) {
        return 1;  // 忙，跳过
    }
    
    // ✅ 构建数据包（使用static避免栈溢出）
    static char data[256];
    int n = snprintf(data, sizeof(data), 
        "cmd=2&uid=%s&topic=data&msg=Mode:%d th:%d tl:%d hh:%d hl:%d "
        "jr:%d zl:%d cs:%d js:%d temp:%d.%d humi:%d.%d\r\n",
        BEMFA_ID,
        mode,
        wendu_high / 10,
        wendu_low / 10,
        shidu_high / 10,
        shidu_low / 10,
        jiare, zhileng, chushi, jiashi,
        temp_x10 / 10,
        abs(temp_x10) % 10,
        hum_x10 / 10, 
        abs(hum_x10) % 10);
    
    if (n < 0 || n >= (int)sizeof(data)) {
        NETWORK_LOG("[ERR] Payload too large (%d bytes)\r\n", n);
        return 1;
    }
    
    // ✅ 启动非阻塞上传（无需清空esp8266_buf，AT响应与云端指令内容天然隔离）
    AT_Result_t result = AT_SendData_NonBlocking((uint8_t *)data, strlen(data), 3000);
    
    if (result == AT_RESULT_IN_PROGRESS) {
        g_upload_state = UPLOAD_STATE_SENDING;
        g_upload_start_time = sys_tick_ms;
        NETWORK_LOG("[INFO] Upload started (non-blocking)\r\n");
        return 0;  // 上传已启动
    } else if (result == AT_RESULT_BUSY) {
        NETWORK_LOG("[WARN] AT executor busy, skip upload\r\n");
        return 1;
    } else {
        // 立即失败（参数错误等）
        NETWORK_LOG("[ERR] Upload start failed: %d\r\n", result);
        g_send_fail_count++;
        
        // ✅ 第1次失败即触发重连
        if (g_send_fail_count >= 1 && g_net_state == NET_CONNECTED) {
            g_net_state = NET_RECONNECTING;
            g_reconnect_step = 0;
            NETWORK_LOG("[INFO] Triggering reconnect after upload failure\r\n");
        }
        
        return 1;
    }
}

/**
 * @brief 轮询上传结果（在Network_Task中调用）
 * @return 0=空闲或成功, 1=仍在发送中, 2=失败
 */
static uint8_t Poll_Upload_Result(void) {
    if (g_upload_state != UPLOAD_STATE_SENDING) {
        return 0;  // 空闲或已完成
    }
    
    // 检查超时（5秒）
    if (sys_tick_ms - g_upload_start_time > 5000) {
        NETWORK_LOG("[ERR] Upload timeout (5s)\r\n");
        g_upload_state = UPLOAD_STATE_FAILED;
        g_send_fail_count++;
        
        if (g_send_fail_count >= 1 && g_net_state == NET_CONNECTED) {
            g_net_state = NET_RECONNECTING;
            g_reconnect_step = 0;
        }
        
        return 2;  // 失败
    }
    
    // 检查AT执行器状态
    AT_State_t at_state = AT_GetState();
    
    if (at_state == AT_STATE_DONE) {
        // 上传成功
        NETWORK_LOG("[OK] Upload completed successfully\r\n");
        g_upload_state = UPLOAD_STATE_SUCCESS;
        g_send_fail_count = 0;  // 重置失败计数
        
        // 重置AT执行器状态，准备下次上传
        AT_Reset();
        
        return 0;  // 成功
    } else if (at_state == AT_STATE_TIMEOUT || at_state == AT_STATE_ERROR) {
        // 上传失败
        NETWORK_LOG("[ERR] Upload failed: AT state=%d\r\n", at_state);
        g_upload_state = UPLOAD_STATE_FAILED;
        g_send_fail_count++;
        
        if (g_send_fail_count >= 1 && g_net_state == NET_CONNECTED) {
            g_net_state = NET_RECONNECTING;
            g_reconnect_step = 0;
        }
        
        // 重置AT执行器
        AT_Reset();
        
        return 2;  // 失败
    }
    
    // 仍在进行中
    return 1;
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
    
    // ✅ 关键修复：仅在step=0且刚进入重连状态时检查防抖
    // 一旦通过检查并更新时间戳，后续调用（包括等待AT响应期间）不应再被拦截
    if (g_reconnect_step == 0 && !warn_logged) {
        // 防抖：最小重连间隔60秒（模块复位120秒）
        if (sys_tick_ms - g_last_reconnect_time < 60000) {
            NETWORK_LOG("[WARN] Reconnect too frequent, wait %d ms\r\n", 
                       60000 - (sys_tick_ms - g_last_reconnect_time));
            warn_logged = 1;
            return;
        }
        
        // ✅ 通过防抖检查，更新时间戳
        g_last_reconnect_time = sys_tick_ms;
        warn_logged = 1;  // 标记已检查，本轮重连不再重复检查
        NETWORK_LOG("[INFO] Starting reconnection process (step=%d)...\r\n", g_reconnect_step);
    }
    
    // ✅ 重置标志：当重连完成或失败后，允许下一轮重连
    if (g_reconnect_step == 0 && g_net_state == NET_CONNECTED) {
        warn_logged = 0;  // 重连成功，重置标志
    }
    
    switch (g_reconnect_step) {
        case 0:  // 诊断阶段：检查TCP连接状态
            {
                static uint8_t diag_logged = 0;  // ✅ 防止重复输出诊断日志
                static uint32_t busy_start_time = 0; // ✅ 记录AT忙碌开始时间
                
                // ✅ P0修复：如果AT执行器处于错误状态，先重置
                extern uint8_t AT_IsBusy(void);
                extern void AT_Reset(void);
                
                if (AT_IsBusy()) {
                    // 记录开始忙碌的时间
                    if (busy_start_time == 0) {
                        busy_start_time = sys_tick_ms;
                    }
                    
                    // ✅ P0修复：如果已经超过5秒，强制重置
                    if (sys_tick_ms - busy_start_time > 5000) {
                        NETWORK_LOG("[ERR] AT executor stuck for %lu ms, forcing reset\r\n", 
                                   sys_tick_ms - busy_start_time);
                        AT_Reset();
                        busy_start_time = 0;
                        diag_logged = 0;
                        return;
                    }
                    
                    // ✅ P1修复：每2秒输出一次，避免日志风暴
                    static uint32_t last_busy_log_time = 0;
                    if (sys_tick_ms - last_busy_log_time > 2000) {
                        NETWORK_LOG("[WARN] AT executor busy, waiting...\r\n");
                        last_busy_log_time = sys_tick_ms;
                    }
                    
                    // ✅ 关键修复：轮询状态机（依赖AT_Execute_NonBlocking的幂等性）
                    result = AT_Execute_NonBlocking("AT+CIPSTATUS\r\n", "STATUS:", 2000);
                    
                    // ✅ 处理进行中状态：继续等待
                    if (result == AT_RESULT_IN_PROGRESS) {
                        return;
                    }
                    
                    // ✅ 处理忙状态：理论上不应出现，但作为防御
                    if (result == AT_RESULT_BUSY) {
                        return;
                    }
                    
                    // 如果返回OK/TIMEOUT/ERROR，跳出if分支，继续下面的结果处理
                } else {
                    // AT不忙时，重置计时器并启动新指令
                    busy_start_time = 0;
                    
                    // ✅ 仅在首次进入时输出日志
                    if (!diag_logged) {
                        NETWORK_LOG("[DIAG] Checking TCP status...\r\n");
                        diag_logged = 1;
                    }
                    
                    // 启动AT指令
                    result = AT_Execute_NonBlocking("AT+CIPSTATUS\r\n", "STATUS:", 2000);
                    
                    // 如果立即返回IN_PROGRESS，等待下次调度
                    if (result == AT_RESULT_IN_PROGRESS) {
                        return;
                    }
                    
                    // 如果返回BUSY，等待下次调度
                    if (result == AT_RESULT_BUSY) {
                        return;
                    }
                    
                    // 如果立即返回OK/TIMEOUT/ERROR（极少见），继续下面的结果处理
                }
                
                // ✅ 统一处理最终结果（OK/TIMEOUT/ERROR）
                if (result == AT_RESULT_OK) {
                    diag_logged = 0;  // ✅ 重置标志，允许下次诊断
                    
                    // 检查响应中是否包含"STATUS:2"或"STATUS:3"（已连接）
                    if (strstr((char *)ESP8266_RecvBuf, "STATUS:2") != NULL ||
                        strstr((char *)ESP8266_RecvBuf, "STATUS:3") != NULL) {
                        NETWORK_LOG("[OK] TCP still connected\r\n");
                        g_net_state = NET_CONNECTED;
                        g_send_fail_count = 0;
                        g_reconnect_step = 0;
                        warn_logged = 0;  // ✅ 重置标志，允许下次重连
                        
                        // ✅ P1修复：重置所有静态变量，防止状态污染
                        AT_Reset();  // 重置AT执行器
                        
                        return;
                    } else {
                        NETWORK_LOG("[ERR] TCP disconnected\r\n");
                        g_reconnect_step = 1;  // 进入TCP重连
                    }
                } else if (result == AT_RESULT_TIMEOUT) {
                    diag_logged = 0;  // ✅ 重置标志，允许下次诊断
                    NETWORK_LOG("[ERR] Diagnosis timeout\r\n");
                    g_reconnect_step = 4;  // 直接进入WiFi检测
                } else {
                    // ✅ 处理错误状态（AT_RESULT_ERROR等）
                    diag_logged = 0;  // ✅ 重置标志
                    NETWORK_LOG("[ERR] AT executor error, resetting...\r\n");
                    
                    // ✅ P0修复：强制重置AT执行器
                    AT_Reset();
                    
                    g_reconnect_step = 7;  // 进入模块复位步骤
                }
            }
            break;
            
        case 1:  // TCP层重连 - 步骤1：关闭连接
            {
                NETWORK_LOG("[RECONNECT] TCP layer reconnecting (retry=%d)...\r\n", g_reconnect_retry_count);
                
                // 步骤1.1: 关闭当前连接
                result = AT_Execute_NonBlocking("AT+CIPCLOSE\r\n", "CLOSED", 2000);
                
                if (result == AT_RESULT_IN_PROGRESS) {
                    return;  // ✅ 等待完成
                } else if (result == AT_RESULT_OK) {
                    NETWORK_LOG("[OK] Connection closed\r\n");
                    g_reconnect_step = 6;  // ✅ 进入子步骤：建立新连接
                } else {
                    NETWORK_LOG("[WARN] CIPCLOSE failed, continue anyway\r\n");
                    g_reconnect_step = 6;  // 即使失败也尝试重新连接
                }
            }
            break;
            
        case 6:  // TCP层重连 - 步骤2：建立新连接
            {
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
                    // 否则继续重试（回到case 1）
                    else {
                        g_reconnect_step = 1;
                    }
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
                    
                    // ✅ 关键修复：立即更新时间戳，防止重复触发
                    g_last_reconnect_time = sys_tick_ms;
                    
                    g_reconnect_step = 8;
                    g_reconnect_start_time = sys_tick_ms;
                } else {
                    NETWORK_LOG("[ERR] Reset command failed, retry later\r\n");
                    // ✅ 关键修复：即使失败也更新时间戳，避免无限重试
                    g_last_reconnect_time = sys_tick_ms;
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
        // ✅ 关键修复：如果没有找到msg=，检查是否是分片数据
        // 如果缓冲区包含部分关键字（如wend/shidu/ZD/SD等），保留等待完整数据
        if (strstr((char *)esp8266_buf, "wend") != NULL || 
            strstr((char *)esp8266_buf, "shidu") != NULL ||
            strstr((char *)esp8266_buf, "ZD") != NULL ||
            strstr((char *)esp8266_buf, "SD") != NULL ||
            strstr((char *)esp8266_buf, "KJR") != NULL ||
            strstr((char *)esp8266_buf, "GJR") != NULL) {
            // 可能是分片数据，保留缓冲区等待下次接收
            NETWORK_LOG("[DEBUG] Incomplete data, waiting for more...\r\n");
            return;
        }
        
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
    
    // ===== 【新增】优先尝试解析旧格式（巴法云标准格式）=====
    // ✅ 实际格式：wendu_low=20.0;wendu_high=25.0;shidu_low=50.0;shidu_high=64.0
    // 分隔符为分号;，数值为浮点数
    
    // ✅ 关键修复：过滤自己上传的数据回显
    // 上传格式：Mode:1 th:25 tl:20 hh:65 hl:50 jr:0 zl:0 cs:0 js:0 temp:24.3 humi:59.2
    if (strstr(payload_start, "Mode:") != NULL && strstr(payload_start, "th:") != NULL) {
        // 这是自己上传的数据回显，直接忽略
        NETWORK_LOG("[DEBUG] Ignoring own upload data echo\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    // ✅ 调试：输出完整载荷内容
    if (strstr(payload_start, "wendu") != NULL || strstr(payload_start, "shidu") != NULL ||
        strstr(payload_start, "ZD") != NULL || strstr(payload_start, "SD") != NULL) {
        NETWORK_LOG("[DEBUG] Payload: %.*s\r\n", payload_len, payload_start);
    }
    
    // ===== 解析复合阈值消息（一次性包含四个阈值）=====
    // 格式：wendu_high=25.0;wendu_low=20.0;shidu_high=64.0;shidu_low=50.0
    if (strstr(payload_start, "wendu_high=") != NULL || 
        strstr(payload_start, "wendu_low=") != NULL ||
        strstr(payload_start, "shidu_high=") != NULL ||
        strstr(payload_start, "shidu_low=") != NULL) {
        
        int temp_high_found = 0, temp_low_found = 0;
        int humid_high_found = 0, humid_low_found = 0;
        int16_t temp_high_val = 0, temp_low_val = 0;
        int16_t humid_high_val = 0, humid_low_val = 0;
        
        // 解析温度上限 wendu_high=25.0
        char *pos = strstr(payload_start, "wendu_high=");
        if (pos != NULL) {
            pos += 11;  // 跳过"wendu_high="
            float fval = atof(pos);  // ✅ 使用atof解析浮点数
            
            // ✅ 关键修复：先校验浮点数范围，再转换
            if (fval >= -40.0f && fval <= 80.0f) {
                temp_high_val = (int16_t)(fval * 10.0f);  // ✅ 直接乘以10保留精度（如25.5→255）
                temp_high_found = 1;
                NETWORK_LOG("[PARSED] Temp high: %.1f -> %d\r\n", fval, temp_high_val);
            } else {
                NETWORK_LOG("[ERR] Temp high out of range: %.1f (valid: -40.0~80.0)\r\n", fval);
            }
        }
        
        // 解析温度下限 wendu_low=20.0
        pos = strstr(payload_start, "wendu_low=");
        if (pos != NULL) {
            pos += 10;  // 跳过"wendu_low="
            float fval = atof(pos);
            
            // ✅ 先校验浮点数范围
            if (fval >= -40.0f && fval <= 80.0f) {
                temp_low_val = (int16_t)(fval * 10.0f);
                temp_low_found = 1;
                NETWORK_LOG("[PARSED] Temp low: %.1f -> %d\r\n", fval, temp_low_val);
            } else {
                NETWORK_LOG("[ERR] Temp low out of range: %.1f (valid: -40.0~80.0)\r\n", fval);
            }
        }
        
        // 解析湿度上限 shidu_high=64.0
        pos = strstr(payload_start, "shidu_high=");
        if (pos != NULL) {
            pos += 11;  // 跳过"shidu_high="
            float fval = atof(pos);
            
            // ✅ 先校验浮点数范围
            if (fval >= 0.0f && fval <= 100.0f) {
                humid_high_val = (int16_t)(fval * 10.0f);
                humid_high_found = 1;
                NETWORK_LOG("[PARSED] Humid high: %.1f -> %d\r\n", fval, humid_high_val);
            } else {
                NETWORK_LOG("[ERR] Humid high out of range: %.1f (valid: 0.0~100.0)\r\n", fval);
            }
        }
        
        // 解析湿度下限 shidu_low=50.0
        pos = strstr(payload_start, "shidu_low=");
        if (pos != NULL) {
            pos += 10;  // 跳过"shidu_low="
            float fval = atof(pos);
            
            // ✅ 先校验浮点数范围
            if (fval >= 0.0f && fval <= 100.0f) {
                humid_low_val = (int16_t)(fval * 10.0f);
                humid_low_found = 1;
                NETWORK_LOG("[PARSED] Humid low: %.1f -> %d\r\n", fval, humid_low_val);
            } else {
                NETWORK_LOG("[ERR] Humid low out of range: %.1f (valid: 0.0~100.0)\r\n", fval);
            }
        }
        
        // ✅ 批量设置阈值（只有当至少有一个有效值时才设置）
        if (temp_high_found || temp_low_found || humid_high_found || humid_low_found) {
            // 使用当前值作为默认值，避免覆盖未下发的阈值
            int16_t final_temp_high = temp_high_found ? temp_high_val : g_temp_high;
            int16_t final_temp_low = temp_low_found ? temp_low_val : g_temp_low;
            int16_t final_humid_high = humid_high_found ? humid_high_val : g_humid_high;
            int16_t final_humid_low = humid_low_found ? humid_low_val : g_humid_low;
            
            // 验证合理性
            if (final_temp_high > final_temp_low && final_humid_high > final_humid_low) {
                Control_Task_SetTempThreshold(final_temp_high, final_temp_low);
                Control_Task_SetHumidThreshold(final_humid_high, final_humid_low);
                NETWORK_LOG("[OK] All thresholds updated\r\n");
            } else {
                NETWORK_LOG("[WARN] Invalid threshold combination, skip update\r\n");
            }
            
            // 清空缓冲区
            memset(esp8266_buf, 0, esp8266_cnt);
            esp8266_cnt = 0;
            return;
        }
    }
    
    // ===== 单条指令解析（模式切换、设备控制）=====
    
    // 模式切换（旧格式）
    if (strstr(payload_start, "ZD") != NULL && strstr(payload_start, "ZD:") == NULL) {
        g_pending_cmd.type = CMD_AUTO_MODE;
        g_pending_cmd.value = 1;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Auto mode\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    if (strstr(payload_start, "SD") != NULL && strstr(payload_start, "SD:") == NULL) {
        g_pending_cmd.type = CMD_MANUAL_MODE;
        g_pending_cmd.value = 1;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Manual mode\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    // 手动控制（旧格式：KJR/GJR分离）
    if (strstr(payload_start, "KJR") != NULL && strstr(payload_start, "KJR:") == NULL) {
        g_pending_cmd.type = CMD_HEATER;
        g_pending_cmd.value = 1;  // 开启
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Heater ON\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    if (strstr(payload_start, "GJR") != NULL) {
        g_pending_cmd.type = CMD_HEATER;
        g_pending_cmd.value = 0;  // 关闭
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Heater OFF\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    if (strstr(payload_start, "KZL") != NULL && strstr(payload_start, "KZL:") == NULL) {
        g_pending_cmd.type = CMD_COOLER;
        g_pending_cmd.value = 1;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Cooler ON\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    if (strstr(payload_start, "GZL") != NULL) {
        g_pending_cmd.type = CMD_COOLER;
        g_pending_cmd.value = 0;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Cooler OFF\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    if (strstr(payload_start, "KCS") != NULL && strstr(payload_start, "KCS:") == NULL) {
        g_pending_cmd.type = CMD_DEHUMIDIFIER;
        g_pending_cmd.value = 1;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Dehumidifier ON\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    if (strstr(payload_start, "GCS") != NULL) {
        g_pending_cmd.type = CMD_DEHUMIDIFIER;
        g_pending_cmd.value = 0;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Dehumidifier OFF\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    if (strstr(payload_start, "KJS") != NULL && strstr(payload_start, "KJS:") == NULL) {
        g_pending_cmd.type = CMD_HUMIDIFIER;
        g_pending_cmd.value = 1;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Humidifier ON\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    if (strstr(payload_start, "GJS") != NULL) {
        g_pending_cmd.type = CMD_HUMIDIFIER;
        g_pending_cmd.value = 0;
        g_cmd_available = 1;
        NETWORK_LOG("[PARSED] Humidifier OFF\r\n");
        memset(esp8266_buf, 0, esp8266_cnt);
        esp8266_cnt = 0;
        return;
    }
    
    // ===== 如果旧格式都不匹配，再尝试新格式 =====
    
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

