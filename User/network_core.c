#include "network_core.h"
#include "wifi_driver.h"  // 包含WIFI_SSID等宏定义
#include "delay.h"
#include "oled.h"
#include "Usart.h"  // 用于Serial_Printf调试输出
#include "stm32f10x_usart.h"  // 用于USART_SendData等
#include <stdio.h>
#include <string.h>

/* 系统滴答定时器（在main.c中定义）*/
extern volatile uint32_t sys_tick_ms;

/* 网络状态 */
static NetworkState_t g_net_state = NET_STATE_DISCONNECTED;

/* OLED锁定标志（联网初始化期间禁止其他OLED刷新）*/
static uint8_t g_oled_locked = 0;

/**
 * @brief 显示联网失败并倒计时
 */
static void Show_Network_Failure(const char *message) {
    OLED_Clear(0);
    
    OLED_ShowCHinese(0, 0, 56);  // 联
    OLED_ShowCHinese(18, 0, 57); // 网
    OLED_ShowCHinese(54, 0, 36); // 失
    OLED_ShowCHinese(72, 0, 37); // 败
    OLED_ShowString(0, 3, (uint8_t *)message, 16);
    
    char countdown_str[10];
    for (int i = 5; i > 0; i--) {
        sprintf(countdown_str, "%ds", i);
        OLED_ShowString(90, 6, (uint8_t *)countdown_str, 16);
        delay_ms(1000);
    }
    
    OLED_Clear(0);
    
    // 重新显示初始化信息
    OLED_ShowCHinese(0, 3, 21);  // 正
    OLED_ShowCHinese(18, 3, 22); // 在
    OLED_ShowCHinese(36, 3, 23); // 连
    OLED_ShowCHinese(54, 3, 24); // 接
    OLED_ShowString(72, 3, "WIFI", 16);
    OLED_ShowString(108, 3, "..", 16);
    OLED_ShowCHinese(0, 6, 4);   // 进
    OLED_ShowCHinese(18, 6, 5);  // 度
    OLED_ShowCHinese(36, 6, 13); // ：
    
    // 注意：失败后不解锁OLED，因为会重试初始化
}

/**
 * @brief 显示联网成功
 */
static void Show_Network_Success(void) {
    OLED_Clear(0);
    OLED_ShowCHinese(0, 0, 56);  // 联
    OLED_ShowCHinese(18, 0, 57); // 网
    OLED_ShowCHinese(36, 0, 38); // 成
    OLED_ShowCHinese(54, 0, 39); // 功
    OLED_ShowString(0, 3, (uint8_t *)"WiFi connected", 16);
    OLED_ShowString(0, 6, (uint8_t *)"Bemfa connected", 16);
    delay_ms(1500);
    OLED_Clear(0);
    
    // 解锁OLED，允许其他任务刷新
    g_oled_locked = 0;
}

/* 初始化子状态 */
typedef enum {
    INIT_STEP_IDLE = 0,
    INIT_STEP_RESET,
    INIT_STEP_AT_TEST,
    INIT_STEP_SET_STA,
    INIT_STEP_CONNECT_WIFI,
    INIT_STEP_CONNECT_TCP,
    INIT_STEP_SUBSCRIBE,
    INIT_STEP_COMPLETE
} InitStep_t;

static InitStep_t g_init_step = INIT_STEP_IDLE;
// static uint32_t g_init_start_time = 0;  // 未使用，注释掉

/* TCP连接状态 */
static uint8_t g_tcp_connected = 0;
// static uint8_t g_wifi_connected = 0;  // 未使用，注释掉

/* 上传状态 */
static uint8_t g_upload_busy = 0;
static uint32_t g_upload_start_time = 0;

/* 云端指令缓冲区 */
static CloudCommand_t g_pending_cmd;
static uint8_t g_cmd_available = 0;

/* 接收缓冲区 */
static uint8_t g_recv_buffer[512];

/* 外部函数声明（wifi_driver.c中的静态函数）*/
extern void RingBuffer_AT_Clear(void);

/* 上传监控状态（第二阶段：支持重连）*/
typedef struct {
    uint8_t upload_fail_count;      // 连续上传失败计数
    uint8_t tcp_disconnected;       // TCP是否断开标志
    uint8_t upload_paused;          // 上传是否暂停（TCP断开后暂停）
    
    /* 重连状态 */
    uint8_t is_reconnecting;        // 是否正在重连
    uint8_t reconnect_attempts;     // 当前重连尝试次数
    uint32_t reconnect_start_time;  // 重连开始时间
    uint32_t next_retry_time;       // 下次重试时间
} UploadMonitor_t;

static UploadMonitor_t g_upload_monitor = {0};

/**
 * @brief 检测TCP连接状态
 * @return 1=已连接, 0=已断开
 */
static uint8_t Check_TCP_Connection(void) {
    Serial_Printf("[NET] Sending AT+CIPSTATUS...\r\n");
    
    // 使用WiFi_Send_AT_Command查询TCP状态
    // 只有收到"STATUS:3"才认为TCP已连接
    if (WiFi_Send_AT_Command("AT+CIPSTATUS\r\n", "STATUS:3", 5000)) {
        // 收到STATUS:3，TCP已连接
        Serial_Printf("[NET] TCP status: CONNECTED (STATUS:3)\r\n");
        return 1;
    } else {
        // 未收到STATUS:3，可能是：
        // 1. STATUS:2 - 已获取IP但未建立TCP
        // 2. STATUS:4 - TCP已断开
        // 3. 超时或无响应
        Serial_Printf("[NET] TCP status: DISCONNECTED (no STATUS:3 received)\r\n");
        return 0;  // 默认认为已断开
    }
}

/**
 * @brief 执行TCP重连
 * @return 1=成功, 0=失败
 */
static uint8_t TCP_Reconnect(void) {
    char cmd_buf[128];
    char subscribe_cmd[128];
    char cipsend_cmd[32];
    uint16_t data_len;
    
    Serial_Printf("[NET] === Starting TCP Reconnection ===\r\n");
    
    // Step 1: 建立TCP连接
    Serial_Printf("[NET] Step 1: Connecting to Bemfa Cloud...\r\n");
    sprintf(cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", BEMFA_SERVER_IP, BEMFA_SERVER_PORT);
    
    // 尝试CONNECT或OK响应
    if (WiFi_Send_AT_Command(cmd_buf, "CONNECT", 15000)) {
        Serial_Printf("[NET] TCP connected (response: CONNECT)\r\n");
    } else if (WiFi_Send_AT_Command(cmd_buf, "OK", 15000)) {
        Serial_Printf("[NET] TCP connected (response: OK)\r\n");
    } else {
        Serial_Printf("[NET] TCP connection FAILED\r\n");
        return 0;
    }
    
    // Step 2: 订阅主题
    Serial_Printf("[NET] Step 2: Subscribing topic...\r\n");
    data_len = sprintf(subscribe_cmd, "cmd=1&uid=%s&topic=%s", BEMFA_UID, BEMFA_TOPIC_CONTROL);
    
    // 发送CIPSEND指令
    sprintf(cipsend_cmd, "AT+CIPSEND=%d\r\n", data_len);
    
    if (!WiFi_Send_AT_Command(cipsend_cmd, ">", 1000)) {
        Serial_Printf("[NET] CIPSEND failed\r\n");
        return 0;
    }
    
    // 发送订阅数据
    WiFi_Send_Data((uint8_t *)subscribe_cmd, data_len);
    
    // 等待SEND OK
    if (!WiFi_Send_AT_Command("", "SEND OK", 2000)) {
        Serial_Printf("[NET] Subscribe SEND OK not received\r\n");
        return 0;
    }
    
    Serial_Printf("[NET] Topic subscribed successfully\r\n");
    Serial_Printf("[NET] === TCP Reconnection COMPLETE ===\r\n");
    
    return 1;
}

/**
 * @brief 处理TCP重连逻辑（非阻塞状态机）
 */
static void Handle_TCP_Reconnection(void) {
    extern volatile uint32_t sys_tick_ms;
    uint32_t remaining;
    static uint32_t last_display = 0;
    
    if (!g_upload_monitor.is_reconnecting) {
        return;  // 不在重连状态
    }
    
    // 检查是否到达重试时间
    if (sys_tick_ms < g_upload_monitor.next_retry_time) {
        // 还未到重试时间，显示倒计时
        remaining = (g_upload_monitor.next_retry_time - sys_tick_ms) / 1000;
        if (sys_tick_ms - last_display >= 1000) {  // 每秒更新一次
            Serial_Printf("[NET] Reconnect retry in %lu seconds... (attempt %d/3)\r\n", 
                         remaining, g_upload_monitor.reconnect_attempts + 1);
            last_display = sys_tick_ms;
        }
        return;
    }
    
    // 到达重试时间，执行重连
    Serial_Printf("[NET] Attempting reconnection #%d...\r\n", g_upload_monitor.reconnect_attempts + 1);
    
    if (TCP_Reconnect()) {
        // 重连成功
        Serial_Printf("[NET] Reconnection SUCCESS! Resuming uploads.\r\n");
        
        // 重置监控状态
        g_upload_monitor.upload_fail_count = 0;
        g_upload_monitor.tcp_disconnected = 0;
        g_upload_monitor.upload_paused = 0;
        g_upload_monitor.is_reconnecting = 0;
        g_upload_monitor.reconnect_attempts = 0;
        
        // 恢复网络状态
        g_net_state = NET_STATE_CONNECTED;
    } else {
        // 重连失败
        g_upload_monitor.reconnect_attempts++;
        
        if (g_upload_monitor.reconnect_attempts >= 3) {
            // 3次都失败，进入离线状态
            Serial_Printf("[NET] Reconnection FAILED after 3 attempts. Going OFFLINE.\r\n");
            g_upload_monitor.is_reconnecting = 0;
            g_upload_monitor.reconnect_attempts = 0;
            g_net_state = NET_STATE_OFFLINE;
        } else {
            // 还有重试机会，计算下次重试时间（指数退避）
            uint32_t delays[] = {15000, 20000, 40000};  // 15s, 20s, 40s
            g_upload_monitor.next_retry_time = sys_tick_ms + delays[g_upload_monitor.reconnect_attempts];
            Serial_Printf("[NET] Will retry in %lu seconds...\r\n", delays[g_upload_monitor.reconnect_attempts] / 1000);
        }
    }
}

/**
 * @brief 执行初始化步骤
 */
static void Init_Step_Execute(void) {
    char cmd_buf[128];
    uint8_t tcp_result;      // TCP连接结果
    uint16_t data_len;       // 数据长度
    
    switch (g_init_step) {
        case INIT_STEP_RESET:
            Serial_Printf("[NET] Step 1: Resetting module...\r\n");
            
            // 锁定OLED，禁止其他任务刷新
            g_oled_locked = 1;
            
            // 显示OLED
            OLED_ShowCHinese(0, 3, 21);  // 正
            OLED_ShowCHinese(18, 3, 22); // 在
            OLED_ShowCHinese(36, 3, 23); // 连
            OLED_ShowCHinese(54, 3, 24); // 接
            OLED_ShowString(72, 3, "WIFI", 16);
            OLED_ShowString(108, 3, "..", 16);
            OLED_ShowCHinese(0, 6, 4);   // 进
            OLED_ShowCHinese(18, 6, 5);  // 度
            OLED_ShowCHinese(36, 6, 13); // ：
            OLED_ShowString(60, 6, "10%", 16);
            
            // 复位模块
            WiFi_Module_Reset();
            Serial_Printf("[NET] Module reset complete\r\n");
            g_init_step = INIT_STEP_AT_TEST;
            break;
            
        case INIT_STEP_AT_TEST:
            Serial_Printf("[NET] Step 2: Testing AT...\r\n");
            
            // 先清空缓冲区，确保没有残留数据
            // 清空AT缓冲区（初始化阶段，确保干净环境）
            RingBuffer_AT_Clear();
            
            Serial_Printf("[NET] Buffer cleared, sending AT...\r\n");
            
            if (WiFi_Send_AT_Command("AT\r\n", "OK", 1000)) {
                Serial_Printf("[NET] AT test OK\r\n");
                OLED_ShowString(60, 6, "30%", 16);
                g_init_step = INIT_STEP_SET_STA;
            } else {
                Serial_Printf("[NET] AT test FAILED\r\n");
                Show_Network_Failure("AT command failed");
                
                // 检查是否有收到任何数据
                // 调试信息已移除（新架构下AT响应在WiFi_Send_AT_Command内部处理）
                
                // 重试3次后失败
                static uint8_t retry = 0;
                retry++;
                Serial_Printf("[NET] AT retry count: %d\r\n", retry);
                if (retry >= 3) {
                    Serial_Printf("[NET] AT test failed after 3 retries, going OFFLINE\r\n");
                    g_net_state = NET_STATE_OFFLINE;
                    g_init_step = INIT_STEP_IDLE;
                    retry = 0;  // ✅ 重置计数器，以便下次初始化
                    
                    // 最终失败，解锁OLED
                    g_oled_locked = 0;
                }
            }
            break;
            
        case INIT_STEP_SET_STA:
            Serial_Printf("[NET] Step 3: Setting STA mode...\r\n");
            if (WiFi_Send_AT_Command("AT+CWMODE=1\r\n", "OK", 1000)) {
                Serial_Printf("[NET] STA mode set OK\r\n");
                OLED_ShowString(60, 6, "50%", 16);
                g_init_step = INIT_STEP_CONNECT_WIFI;
            } else {
                Serial_Printf("[NET] STA mode FAILED\r\n");
                Show_Network_Failure("WiFi mode setting failed");
                g_net_state = NET_STATE_OFFLINE;
                g_init_step = INIT_STEP_IDLE;
                g_oled_locked = 0;  // 解锁OLED
            }
            break;
            
        case INIT_STEP_CONNECT_WIFI:
            Serial_Printf("[NET] Step 4: Connecting WiFi...\r\n");
            sprintf(cmd_buf, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
            
            // 先清空AT缓冲区，然后发送指令
            RingBuffer_AT_Clear();
            
            if (WiFi_Send_AT_Command(cmd_buf, "WIFI CONNECTED", 10000)) {
                Serial_Printf("[NET] WiFi connected OK\r\n");
                OLED_ShowString(60, 6, "70%", 16);
                g_init_step = INIT_STEP_CONNECT_TCP;
            } else {
                Serial_Printf("[NET] WiFi connection FAILED\r\n");
                Show_Network_Failure("WiFi connect failed");
                
                // 调试信息已移除（新架构下响应在WiFi_Send_AT_Command内部处理）
                
                g_net_state = NET_STATE_OFFLINE;
                g_init_step = INIT_STEP_IDLE;
                g_oled_locked = 0;  // 解锁OLED
            }
            break;
            
        case INIT_STEP_CONNECT_TCP:
            Serial_Printf("[NET] Step 5: Connecting TCP...\r\n");
            
            // 先清空缓冲区
            RingBuffer_AT_Clear();
            
            // 尝试使用域名连接
            sprintf(cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", BEMFA_SERVER_IP, BEMFA_SERVER_PORT);
            Serial_Printf("[NET] Connecting to %s:%s...\r\n", BEMFA_SERVER_IP, BEMFA_SERVER_PORT);
            
            // 增加超时到10秒（DNS解析可能需要时间）
            tcp_result = WiFi_Send_AT_Command(cmd_buf, "CONNECT", 10000);
            
            if (tcp_result) {
                Serial_Printf("[NET] TCP connected (response: CONNECT)\r\n");
                OLED_ShowString(60, 6, "90%", 16);
                g_tcp_connected = 1;
                g_init_step = INIT_STEP_SUBSCRIBE;
            } else {
                // 尝试检查是否有OK响应
                RingBuffer_AT_Clear();
                sprintf(cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", BEMFA_SERVER_IP, BEMFA_SERVER_PORT);
                if (WiFi_Send_AT_Command(cmd_buf, "OK", 10000)) {
                    Serial_Printf("[NET] TCP connected (response: OK)\r\n");
                    OLED_ShowString(60, 6, "90%", 16);
                    g_tcp_connected = 1;
                    g_init_step = INIT_STEP_SUBSCRIBE;
                } else {
                    Serial_Printf("[NET] TCP connection FAILED\r\n");
                    Show_Network_Failure("Bemfa connect failed");
                    
                    // 调试信息已移除（新架构下响应在WiFi_Send_AT_Command内部处理）
                    Serial_Printf("[NET] No response received (timeout or DNS failed)\r\n");
                    Serial_Printf("[NET] Hint: Try using IP address instead of domain\r\n");
                    
                    g_net_state = NET_STATE_OFFLINE;
                    g_init_step = INIT_STEP_IDLE;
                    g_oled_locked = 0;  // 解锁OLED
                }
            }
            break;
            
        case INIT_STEP_SUBSCRIBE:
            Serial_Printf("[NET] Step 6: Subscribing topic...\r\n");
            
            // 只订阅control主题（接收控制指令）
            // data主题用于上传，不需要订阅（避免收到自己上传的数据回显）
            // online主题对硬件系统无用，不订阅
            char subscribe_cmd[128];
            sprintf(subscribe_cmd, "cmd=1&uid=%s&topic=%s\r\n", 
                    BEMFA_UID, 
                    BEMFA_TOPIC_CONTROL);   // 只订阅control主题
            
            Serial_Printf("[NET] Subscribe cmd: %s", subscribe_cmd);
            
            // 先清空缓冲区
            RingBuffer_AT_Clear();
            
            // 使用AT+CIPSEND两阶段发送（与备份代码一致）
            data_len = strlen(subscribe_cmd);
            char cipsend_cmd[32];
            sprintf(cipsend_cmd, "AT+CIPSEND=%d\r\n", data_len);
            
            Serial_Printf("[NET] Sending CIPSEND: %s", cipsend_cmd);
            
            // 阶段1：发送CIPSEND指令，等待 ">"
            if (WiFi_Send_AT_Command(cipsend_cmd, ">", 1000)) {
                Serial_Printf("[NET] Got '>' prompt, sending subscription data...\r\n");
                
                // 阶段2：发送实际的订阅数据
                WiFi_Send_Data((uint8_t *)subscribe_cmd, data_len);
                
                // 阶段3：等待 "SEND OK"
                if (WiFi_Send_AT_Command("", "SEND OK", 2000)) {
                    Serial_Printf("[NET] Data sent OK\r\n");
                    
                    // 订阅指令发送成功，不等待服务器响应（与备份代码一致）
                    // 服务器可能会异步返回 cmd=1&res=1，但我们不阻塞等待
                    Serial_Printf("[NET] Topic subscribed (async response expected)\r\n");
                    
                    // 显示联网成功
                    Show_Network_Success();
                    
                    g_init_step = INIT_STEP_COMPLETE;
                    g_net_state = NET_STATE_CONNECTED;
                    Serial_Printf("[NET] === Network initialization COMPLETE ===\r\n");
                } else {
                    Serial_Printf("[NET] SEND OK not received\r\n");
                    // 调试信息已移除
                    g_net_state = NET_STATE_OFFLINE;
                    g_init_step = INIT_STEP_IDLE;
                }
            } else {
                Serial_Printf("[NET] CIPSEND command failed\r\n");
                // 调试信息已移除
                g_net_state = NET_STATE_OFFLINE;
                g_init_step = INIT_STEP_IDLE;
            }
            break;
            
        case INIT_STEP_COMPLETE:
            // 初始化完成
            break;
            
        default:
            break;
    }
}

/**
 * @brief 初始化网络模块
 */
void Network_Core_Init(void) {
    Serial_Printf("\r\n[NET] ========================================\r\n");
    Serial_Printf("[NET] Network Core Initialization Started\r\n");
    Serial_Printf("[NET] ========================================\r\n");
    
    // 初始化WiFi驱动（先尝试115200）
    Serial_Printf("[NET] Initializing WiFi driver at 115200 baud...\r\n");
    WiFi_Driver_Init(115200);
    Serial_Printf("[NET] WiFi driver initialized\r\n");
    
    // 开始初始化流程
    g_net_state = NET_STATE_INITIALIZING;
    g_init_step = INIT_STEP_RESET;
    // g_init_start_time = sys_tick_ms;  // 已注释，未使用
}

/**
 * @brief 检查并处理云端指令
 */
static void Check_Cloud_Command(void) {
    uint8_t frame_buf[256];
    uint16_t frame_len;
    static uint16_t recv_count = 0;  // 接收计数器
    
    // 从云端缓冲区读取完整帧（基于\r\n判断）
    frame_len = WiFi_Read_Cloud_Complete_Frame(frame_buf, sizeof(frame_buf) - 1);
    if (frame_len == 0) {
        return;  // 没有完整帧，等待下次
    }
    
    frame_buf[frame_len] = '\0';
    
    // 调试：打印每次读取的帧内容和缓冲区剩余量
    Serial_Printf("[DBG] Frame read: len=%d, remaining=%d, content=%.50s\r\n", 
                 frame_len, WiFi_Get_Cloud_Data_Length(), frame_buf);
    
    // 每10次接收打印一次，避免日志风暴
    if (++recv_count % 10 == 0) {
        // 只打印前256字节，避免过长日志导致性能问题
        if (frame_len > 256) {
            uint8_t temp_buf[257];
            memcpy(temp_buf, frame_buf, 256);
            temp_buf[256] = '\0';
            Serial_Printf("[NET] Received %d bytes: %s...\r\n", frame_len, temp_buf);
        } else {
            Serial_Printf("[NET] Received %d bytes: %s\r\n", frame_len, frame_buf);
        }
    }
    
    // 解析指令
    if (Cloud_Parse_Command((char *)frame_buf, frame_len, &g_pending_cmd)) {
        Serial_Printf("[NET] Command parsed: type=%d\r\n", g_pending_cmd.type);
        
        // 调试：打印解析后云端缓冲区的剩余内容
        uint16_t remaining_len = WiFi_Get_Cloud_Data_Length();
        if (remaining_len > 0) {
            uint8_t peek_buf[128];
            uint16_t peek_len = WiFi_Peek_Cloud_Data(peek_buf, sizeof(peek_buf) - 1);
            if (peek_len > 0) {
                peek_buf[peek_len] = '\0';
                Serial_Printf("[DBG] Cloud buffer after parse: %d bytes, content: %s\r\n", 
                             remaining_len, peek_buf);
            }
        } else {
            Serial_Printf("[DBG] Cloud buffer after parse: empty\r\n");
        }
        
        g_cmd_available = 1;
    }
    
    // 注意：WiFi_Read_Cloud_Complete_Frame已经清空了该帧，无需手动清空
}

/**
 * @brief 网络任务
 */
void Network_Core_Task(void) {
    // 如果正在初始化，执行初始化步骤
    if (g_net_state == NET_STATE_INITIALIZING) {
        Init_Step_Execute();
        return;
    }
    
    // 处理TCP重连逻辑（非阻塞）
    Handle_TCP_Reconnection();
    
    // 如果已连接，检查云端指令
    if (g_net_state == NET_STATE_CONNECTED) {
        Check_Cloud_Command();
    }
    
    // 离线状态
    if (g_net_state == NET_STATE_OFFLINE) {
        static uint8_t offline_logged = 0;
        if (!offline_logged) {
            Serial_Printf("[NET] *** Network is OFFLINE ***\r\n");
            offline_logged = 1;
        }
    }
}

/**
 * @brief 上传传感器数据
 */
uint8_t Network_Core_Upload(
    uint8_t mode,
    int16_t temp_high, int16_t temp_low,
    int16_t humid_high, int16_t humid_low,
    uint8_t heater, uint8_t cooler,
    uint8_t dehumidifier, uint8_t humidifier,
    int16_t temp_x10, int16_t hum_x10)
{
    char upload_buf[256];
    uint16_t len;
    
    // 如果上传已暂停（TCP断开），直接返回失败
    if (g_upload_monitor.upload_paused) {
        return 1;
    }
    
    // 检查网络状态
    if (g_net_state != NET_STATE_CONNECTED || !g_tcp_connected) {
        static uint8_t warn_count = 0;
        if (warn_count++ % 10 == 0) {  // 每10次警告一次，避免刷屏
            Serial_Printf("[NET] Upload failed: state=%d, tcp=%d\r\n", g_net_state, g_tcp_connected);
        }
        return 1;
    }
    
    // 检查是否正在上传
    if (g_upload_busy) {
        // 检查超时（5秒）
        extern volatile uint32_t sys_tick_ms;
        if (sys_tick_ms - g_upload_start_time > 5000) {
            Serial_Printf("[NET] Upload timeout!\r\n");
            g_upload_busy = 0;  // 超时，重置
        }
        return 1;
    }
    
    // 构建数据包
    len = Cloud_Build_Upload_Packet(
        upload_buf, sizeof(upload_buf),
        mode, temp_high, temp_low, humid_high, humid_low,
        heater, cooler, dehumidifier, humidifier,
        temp_x10, hum_x10
    );
    
    if (len == 0) {
        Serial_Printf("[NET] Build packet failed!\r\n");
        return 1;
    }
    
    // 使用AT+CIPSEND两阶段发送（与备份代码一致）
    char cipsend_cmd[32];
    sprintf(cipsend_cmd, "AT+CIPSEND=%d\r\n", len);
    
    // 阶段1：发送CIPSEND指令，等待 ">"
    if (!WiFi_Send_AT_Command(cipsend_cmd, ">", 1000)) {
        Serial_Printf("[NET] CIPSEND failed (count=%d)\r\n", g_upload_monitor.upload_fail_count + 1);
        // 记录上传失败
        g_upload_monitor.upload_fail_count++;
        goto check_tcp_status;
    }
    
    // 阶段2：发送实际数据
    WiFi_Send_Data((uint8_t *)upload_buf, len);
    
    // 阶段3：等待 "SEND OK"
    if (!WiFi_Send_AT_Command("", "SEND OK", 2000)) {
        Serial_Printf("[NET] SEND OK not received (count=%d)\r\n", g_upload_monitor.upload_fail_count + 1);
        // 记录上传失败
        g_upload_monitor.upload_fail_count++;
        goto check_tcp_status;
    }
    
    // 上传成功，重置计数器
    g_upload_monitor.upload_fail_count = 0;
    g_upload_monitor.tcp_disconnected = 0;  // 清除断开标志
    
    // 打印上传数据（调试用）
    static uint16_t upload_count = 0;
    if (++upload_count % 5 == 0) {  // 每5次打印一次，避免刷屏
        Serial_Printf("[NET] Upload #%d: %s", upload_count, upload_buf);
    }
    
    return 0;
    
check_tcp_status:
    // 检查是否需要检测TCP连接
    if (g_upload_monitor.upload_fail_count >= 3) {
        // 每3次失败检测一次TCP（第3、6、9、12...次）
        if (g_upload_monitor.upload_fail_count % 3 == 0) {
            Serial_Printf("[NET] Checking TCP connection after %d failures...\r\n", 
                         g_upload_monitor.upload_fail_count);
            
            // 先测试ESP-01S是否仍然响应AT指令（增加容错）
            Serial_Printf("[NET] Testing ESP8266 health with AT...\r\n");
            uint8_t at_responding = 0;
            
            // 尝试3次，每次给足超时时间（ESP-01S响应可能很慢）
            for (uint8_t retry = 0; retry < 3; retry++) {
                uint32_t timeout = 3000 + (retry * 1000);  // 3s, 4s, 5s
                Serial_Printf("[NET] AT test #%d (timeout=%lums)...\r\n", retry + 1, timeout);
                
                if (WiFi_Send_AT_Command("AT\r\n", "OK", timeout)) {
                    at_responding = 1;
                    Serial_Printf("[NET] ESP8266 responding on attempt #%d\r\n", retry + 1);
                    break;
                }
            }
            
            if (!at_responding) {
                Serial_Printf("[NET] ESP8266 NOT responding after 3 attempts! Module may be frozen.\r\n");
                // TODO: 第二阶段将在此处触发模块重置
                g_upload_monitor.tcp_disconnected = 1;
                g_upload_monitor.upload_paused = 1;
                return 1;
            }
            
            Serial_Printf("[NET] ESP8266 responding, checking TCP status...\r\n");
            
            if (!Check_TCP_Connection()) {
                // TCP已断开，启动重连流程
                Serial_Printf("[NET] TCP disconnected! Starting reconnection process...\r\n");
                g_upload_monitor.tcp_disconnected = 1;
                g_upload_monitor.upload_paused = 1;
                g_upload_monitor.is_reconnecting = 1;
                g_upload_monitor.reconnect_attempts = 0;
                g_upload_monitor.next_retry_time = sys_tick_ms + 15000;  // 第一次等待15秒
                
                Serial_Printf("[NET] Will attempt reconnection in 15 seconds...\r\n");
            } else {
                Serial_Printf("[NET] TCP still connected, will continue monitoring.\r\n");
            }
        }
    }
    
    return 1;
}

/**
 * @brief 获取云端指令
 */
uint8_t Network_Core_Get_Command(CloudCommand_t *cmd) {
    if (g_cmd_available) {
        memcpy(cmd, &g_pending_cmd, sizeof(CloudCommand_t));
        g_cmd_available = 0;
        return 1;
    }
    return 0;
}

/**
 * @brief 获取当前网络状态
 */
NetworkState_t Network_Core_Get_State(void) {
    return g_net_state;
}

/**
 * @brief 检查OLED是否被锁定（联网初始化期间）
 * @return 1=锁定，0=未锁定
 */
uint8_t Network_Core_Is_OLED_Locked(void) {
    return g_oled_locked;
}

/**
 * @brief 重置上传监控状态（TCP重连成功后调用）
 */
void Network_Core_Reset_Upload_Monitor(void) {
    g_upload_monitor.upload_fail_count = 0;
    g_upload_monitor.tcp_disconnected = 0;
    g_upload_monitor.upload_paused = 0;
}
