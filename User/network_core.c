#include "network_core.h"
#include "wifi_driver.h"  // 包含WIFI_SSID等宏定义
#include "delay.h"
#include "oled.h"
#include "bmp.h"  // WiFi状态图标
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
    
    g_oled_locked = 0;
}

/**
 * @brief 显示联网离线状态
 * @note 初始化失败时调用，显示离线提示后清屏
 */
static void Show_Network_Offline(void) {
    OLED_Clear(0);
    
    OLED_ShowCHinese(0, 0, 56);  // 联
    OLED_ShowCHinese(18, 0, 57); // 网
    OLED_ShowCHinese(36, 0, 36); // 失  
    OLED_ShowCHinese(54, 0, 37); // 败
    OLED_ShowString(0, 3, (uint8_t *)"Offline Mode", 16);
    OLED_ShowString(0, 6, (uint8_t *)"System running", 16);
    
    delay_ms(2000);
    OLED_Clear(0);
}

/**
 * @brief 显示初始化失败信息（带3秒倒计时）
 * @param message: 失败原因（英文）
 */
static void Show_Network_Init_Failure(const char *message) {
    int i;
    char countdown_str[30];
    
    OLED_Clear(0);
    
    // 第一行：联网失败（大字）
    OLED_ShowCHinese(0, 0, 56);   // 联
    OLED_ShowCHinese(18, 0, 57);  // 网
    OLED_ShowCHinese(36, 0, 36);  // 失
    OLED_ShowCHinese(54, 0, 37);  // 败
    
    // 第二行：失败原因（小字英文，12号字体）
    OLED_ShowString(0, 3, (uint8_t *)message, 12);
    
    // 第三行：稍后重试提示 + 倒计时
    for (i = 3; i > 0; i--) {
        sprintf(countdown_str, "Will retry in %ds", i);
        OLED_ShowString(0, 6, (uint8_t *)countdown_str, 12);
        delay_ms(1000);
    }
    
    OLED_Clear(0);
}

/**
 * @brief 在右上角显示WiFi状态图标（不清屏）
 * @param state: 0=未连接, 1=已连接, 2=重连中
 */
void Show_WiFi_Status_Icon(uint8_t state) {
    static uint32_t last_update = 0;
    static uint8_t anim_frame = 0;
    static uint8_t last_state = 0xFF;
    
    if (state != last_state) {
        last_state = state;
        last_update = 0;
    }
    
    if (sys_tick_ms - last_update < 300) {
        return;
    }
    last_update = sys_tick_ms;
    
    switch (state) {
        case 0:  // WiFi未连接 - 显示断开图标
            anim_frame = 0;  // 重置动画帧
            OLED_DrawBMP(112, 0, 128, 16, WIFI_DISCONNECTED);
            break;
            
        case 1:  // WiFi已连接 - 显示连接图标（常亮）
            anim_frame = 0;  // 重置动画帧
            OLED_DrawBMP(112, 0, 128, 16, WIFI_CONNECTED);
            break;
            
        case 2:  // WiFi重连中 - 4帧动画循环
            anim_frame = (anim_frame + 1) % 4;  // 0-3循环
            switch (anim_frame) {
                case 0:  // 帧1：只有底部点
                    OLED_DrawBMP(112, 0, 128, 16, WIFI_ANIM_FRAME1);
                    break;
                case 1:  // 帧2：底部点 + 小圆弧
                    OLED_DrawBMP(112, 0, 128, 16, WIFI_ANIM_FRAME2);
                    break;
                case 2:  // 帧3：底部点 + 小圆弧 + 中圆弧
                    OLED_DrawBMP(112, 0, 128, 16, WIFI_ANIM_FRAME3);
                    break;
                case 3:  // 帧4：完整WiFi图标
                    OLED_DrawBMP(112, 0, 128, 16, WIFI_CONNECTED);
                    break;
            }
            break;
            
        default:
            // 清除图标区域
            {
                uint8_t i;
                OLED_Set_Pos(112, 0);
                for (i = 0; i < 16; i++) OLED_WR_Byte(0x00, OLED_DATA);
                OLED_Set_Pos(112, 1);
                for (i = 0; i < 16; i++) OLED_WR_Byte(0x00, OLED_DATA);
            }
            break;
    }
}

/**
 * @brief 显示WiFi断开状态（在右上角显示图标，不清屏）
 */
static void Show_WiFi_Disconnected(void) {
    // 调用通用的WiFi状态图标显示函数
    Show_WiFi_Status_Icon(0);  // 0=未连接
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

/* 初始化重试计数器 */
static uint8_t g_wifi_retry_count = 0;
static uint8_t g_tcp_retry_count = 0;
static uint8_t g_subscribe_retry_count = 0;

/* 初始化重试最大次数 */
#define INIT_MAX_RETRY 3

/* 重试间隔常量 */
#define RETRY_INTERVAL_SHORT    2000    // 短重试间隔（初始化阶段，单位：ms）
#define RETRY_INTERVAL_LONG_1   10000   // 长重试间隔1（重连第1次，单位：ms）
#define RETRY_INTERVAL_LONG_2   20000   // 长重试间隔2（重连第2次，单位：ms）
#define RETRY_INTERVAL_LONG_3   30000   // 长重试间隔3（重连第3次，单位：ms）
#define WIFI_CHECK_INTERVAL     20000   // WiFi检测间隔（单位：ms）

/* 上次重试时间（用于2秒延迟）
 * @note 由于初始化步骤是顺序执行的（WiFi→TCP→订阅），此变量可安全复用 */
static uint32_t g_last_retry_time = 0;

/* TCP连接状态 */
static uint8_t g_tcp_connected = 0;
// static uint8_t g_wifi_connected = 0;  // 未使用，注释掉

/* 上传状态 */
static uint8_t g_upload_busy = 0;
static uint32_t g_upload_start_time = 0;

/* 云端指令缓冲区 */
static CloudCommand_t g_pending_cmd;
static uint8_t g_cmd_available = 0;

/* 外部函数声明（wifi_driver.c中的静态函数）*/
extern void RingBuffer_AT_Clear(void);

/* 重连子状态 */
typedef enum {
    RECONNECT_STATE_IDLE,           // 空闲
    RECONNECT_STATE_CONNECTING,     // 正在连接TCP
    RECONNECT_STATE_WAIT_CONNECT,   // 等待CONNECT/OK响应
    RECONNECT_STATE_SUBSCRIBING,    // 正在订阅主题
    RECONNECT_STATE_WAIT_CIPSEND,   // 等待CIPSEND的>提示
    RECONNECT_STATE_SENDING_DATA,   // 发送订阅数据
    RECONNECT_STATE_WAIT_SENDOK,    // 等待SEND OK
    RECONNECT_STATE_SUCCESS,        // 重连成功
    RECONNECT_STATE_FAILED          // 重连失败
} ReconnectState_t;

/* WiFi检测子状态 */
typedef enum {
    WIFI_CHECK_STATE_IDLE,          // 空闲
    WIFI_CHECK_STATE_CHECKING,      // 正在检测
    WIFI_CHECK_STATE_CONNECTED,     // WiFi已连接
    WIFI_CHECK_STATE_DISCONNECTED   // WiFi已断开
} WiFiCheckState_t;

/* 上传监控状态（第三阶段：支持WiFi检测）*/
typedef struct {
    uint8_t upload_fail_count;      // 连续上传失败计数
    uint8_t tcp_disconnected;       // TCP是否断开标志
    uint8_t upload_paused;          // 上传是否暂停（TCP断开后暂停）
    
    /* 重连状态 */
    uint8_t is_reconnecting;        // 是否正在重连
    uint8_t reconnect_attempts;     // 当前重连尝试次数
    uint32_t reconnect_start_time;  // 重连开始时间
    uint32_t next_retry_time;       // 下次重试时间
    
    /* 重连子状态机 */
    ReconnectState_t reconnect_state;           // 重连子状态
    uint32_t reconnect_step_start_time;         // 当前步骤开始时间
    char reconnect_cmd_buf[128];                // 重连命令缓冲区
    char reconnect_subscribe_cmd[128];          // 订阅命令缓冲区
    uint16_t reconnect_data_len;                // 订阅数据长度
    
    /* WiFi检测状态 */
    uint8_t wifi_check_enabled;                 // 是否启用WiFi检测
    uint32_t wifi_check_next_time;              // 下次检测时间
    uint8_t wifi_connected;                     // WiFi连接状态
    WiFiCheckState_t wifi_check_state;          // WiFi检测子状态
} UploadMonitor_t;

static UploadMonitor_t g_upload_monitor = {0};

/* 网络连接状态标志（用于初始化失败后的重试）*/
static uint8_t g_wifi_init_connected = 0;    // WiFi是否已连接（初始化阶段）
static uint8_t g_tcp_init_connected = 0;     // TCP是否已连接（初始化阶段）
static uint8_t g_subscribed = 0;             // 是否已订阅主题

/* WiFi连续断开计数器（用于触发WiFi重连）*/
static uint8_t g_wifi_consecutive_disconnect_count = 0;

/* WiFi检测统一状态管理（所有函数共享）*/
static uint32_t g_wifi_check_start_time = 0;  // WiFi检测开始时间，0表示空闲

/* 初始化重试控制 */
static uint8_t g_init_retry_enabled = 0;     // 是否启用初始化重试
static uint32_t g_init_next_retry_time = 0;  // 下次重试时间
#define INIT_RETRY_INTERVAL 30000            // 30秒重试间隔

/**
 * @brief 获取WiFi状态（供OLED显示使用）
 * @return 0=断开, 1=已连接, 2=重连中
 */
uint8_t Get_WiFi_State(void) {
    // 优先检查运行时状态
    if (g_upload_monitor.wifi_connected) {
        if (g_net_state == NET_STATE_RECONNECTING) {
            return 2;  // 重连中
        }
        return 1;  // 已连接
    }
    
    // 检查初始化阶段的状态
    if (g_wifi_init_connected) {
        return 1;  // WiFi已连接（即使初始化未完成）
    }
    
    return 0;  // 断开
}

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
 * @brief 检测WiFi连接状态（非阻塞，无内部状态机）
 * @return 1=已连接, 0=已断开, 2=检测中
 * @note 由Handle_WiFi_Check()调用，依赖外层的wifi_check_state管理时序
 *       使用全局变量g_wifi_check_start_time记录检测开始时间
 */
static uint8_t Check_WiFi_Connection(void) {
    uint16_t recv_len;
    uint8_t recv_buf[64];
    
    // 首次调用，记录开始时间并发送指令
    if (g_wifi_check_start_time == 0) {
        // 发送AT+CWJAP?指令
        const char *cmd = "AT+CWJAP?\r\n";
        const char *p = cmd;
        while (*p) {
            USART_SendData(ESP8266_USART, (uint8_t)*p++);
            while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
        }
        
        g_wifi_check_start_time = sys_tick_ms;
        return 2;  // 刚发送，需要等待
    }
    
    // 检查是否超时（3秒）
    if (sys_tick_ms - g_wifi_check_start_time > 3000) {
        Serial_Printf("[NET] WiFi check timeout\r\n");
        g_wifi_check_start_time = 0;  // 重置
        return 0;  // 超时，认为断开
    }
    
    // 检查是否有响应
    recv_len = RingBuffer_AT_Read(recv_buf, sizeof(recv_buf) - 1);
    if (recv_len > 0) {
        recv_buf[recv_len] = '\0';
        
        // ⭐ 优先检查+CWJAP:，避免OK先到达导致误判
        if (strstr((char *)recv_buf, "+CWJAP:") != NULL) {
            // 收到+CWJAP:，WiFi已连接
            Serial_Printf("[NET] WiFi status: CONNECTED\r\n");
            g_wifi_check_start_time = 0;  // 重置
            return 1;
        }
        
        // 如果只收到OK或ERROR，再等待一小段时间看是否有+CWJAP:
        if ((strstr((char *)recv_buf, "OK") != NULL || strstr((char *)recv_buf, "ERROR") != NULL) &&
            strstr((char *)recv_buf, "+CWJAP:") == NULL) {
            // 等待额外500ms，看是否有+CWJAP:到达
            delay_ms(500);
            recv_len = RingBuffer_AT_Read(recv_buf, sizeof(recv_buf) - 1);
            if (recv_len > 0) {
                recv_buf[recv_len] = '\0';
                if (strstr((char *)recv_buf, "+CWJAP:") != NULL) {
                    Serial_Printf("[NET] WiFi status: CONNECTED\r\n");
                    g_wifi_check_start_time = 0;  // 重置
                    return 1;
                }
            }
            // 仍然没有+CWJAP:，判断为未连接
            Serial_Printf("[NET] WiFi status: DISCONNECTED\r\n");
            g_wifi_check_start_time = 0;  // 重置
            return 0;
        }
        // 其他响应，继续等待
        return 2;
    }
    
    return 2;  // 还没收到响应，继续等待
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
    
    if (!WiFi_Send_AT_Command(cipsend_cmd, ">", 2000)) {
        Serial_Printf("[NET] CIPSEND failed\r\n");
        return 0;
    }
    
    // 发送订阅数据
    WiFi_Send_Data((uint8_t *)subscribe_cmd, data_len);
    
    // 等待SEND OK
    if (!WiFi_Send_AT_Command("", "SEND OK", 3000)) {
        Serial_Printf("[NET] Subscribe SEND OK not received\r\n");
        return 0;
    }
    
    Serial_Printf("[NET] Topic subscribed successfully\r\n");
    Serial_Printf("[NET] === TCP Reconnection COMPLETE ===\r\n");
    
    return 1;
}

/**
 * @brief 执行重连的一个步骤（非阻塞）
 * @return 1=步骤完成, 0=步骤进行中
 */
static uint8_t Reconnect_Step_Execute(void) {
    extern volatile uint32_t sys_tick_ms;
    char cipsend_cmd[32];
    uint16_t recv_len;
    uint8_t recv_buf[64];
    const char *p;
    const char *q;
    
    switch (g_upload_monitor.reconnect_state) {
        case RECONNECT_STATE_IDLE:
            // 不应该到达这里
            return 1;
            
        case RECONNECT_STATE_CONNECTING:
            // Step 1: 发送AT+CIPSTART
            Serial_Printf("[NET] Step 1: Connecting to Bemfa Cloud...\r\n");
            sprintf(g_upload_monitor.reconnect_cmd_buf, 
                   "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", 
                   BEMFA_SERVER_IP, BEMFA_SERVER_PORT);
            
            // 发送命令
            p = g_upload_monitor.reconnect_cmd_buf;
            while (*p) {
                USART_SendData(ESP8266_USART, (uint8_t)*p++);
                while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
            }
            
            // 进入等待响应状态
            g_upload_monitor.reconnect_state = RECONNECT_STATE_WAIT_CONNECT;
            g_upload_monitor.reconnect_step_start_time = sys_tick_ms;
            return 1;
            
        case RECONNECT_STATE_WAIT_CONNECT:
            // 检查是否超时（15秒）
            if (sys_tick_ms - g_upload_monitor.reconnect_step_start_time > 15000) {
                Serial_Printf("[NET] TCP connection timeout\r\n");
                g_upload_monitor.reconnect_state = RECONNECT_STATE_FAILED;
                return 1;
            }
            
            // 检查是否有响应
            recv_len = RingBuffer_AT_Read(recv_buf, sizeof(recv_buf) - 1);
            if (recv_len > 0) {
                recv_buf[recv_len] = '\0';
                
                if (strstr((char *)recv_buf, "CONNECT") != NULL) {
                    Serial_Printf("[NET] TCP connected (response: CONNECT)\r\n");
                    g_upload_monitor.reconnect_state = RECONNECT_STATE_SUBSCRIBING;
                    return 1;
                } else if (strstr((char *)recv_buf, "OK") != NULL) {
                    Serial_Printf("[NET] TCP connected (response: OK)\r\n");
                    g_upload_monitor.reconnect_state = RECONNECT_STATE_SUBSCRIBING;
                    return 1;
                }
                // 其他响应，继续等待
            }
            return 0;  // 还在等待中
            
        case RECONNECT_STATE_SUBSCRIBING:
            // Step 2: 准备订阅命令
            Serial_Printf("[NET] Step 2: Subscribing topic...\r\n");
            g_upload_monitor.reconnect_data_len = sprintf(
                g_upload_monitor.reconnect_subscribe_cmd,
                "cmd=1&uid=%s&topic=%s", 
                BEMFA_UID, BEMFA_TOPIC_CONTROL
            );
            
            // 发送CIPSEND指令
            sprintf(cipsend_cmd, "AT+CIPSEND=%d\r\n", g_upload_monitor.reconnect_data_len);
            q = cipsend_cmd;
            while (*q) {
                USART_SendData(ESP8266_USART, (uint8_t)*q++);
                while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
            }
            
            g_upload_monitor.reconnect_state = RECONNECT_STATE_WAIT_CIPSEND;
            g_upload_monitor.reconnect_step_start_time = sys_tick_ms;
            return 1;
            
        case RECONNECT_STATE_WAIT_CIPSEND:
            // 检查是否超时（3秒）
            if (sys_tick_ms - g_upload_monitor.reconnect_step_start_time > 3000) {
                Serial_Printf("[NET] CIPSEND timeout\r\n");
                g_upload_monitor.reconnect_state = RECONNECT_STATE_FAILED;
                return 1;
            }
            
            // 检查是否有 '>' 提示
            recv_len = RingBuffer_AT_Read(recv_buf, sizeof(recv_buf) - 1);
            if (recv_len > 0) {
                recv_buf[recv_len] = '\0';
                if (strstr((char *)recv_buf, ">") != NULL) {
                    // 发送订阅数据
                    WiFi_Send_Data((uint8_t *)g_upload_monitor.reconnect_subscribe_cmd, 
                                  g_upload_monitor.reconnect_data_len);
                    g_upload_monitor.reconnect_state = RECONNECT_STATE_WAIT_SENDOK;
                    g_upload_monitor.reconnect_step_start_time = sys_tick_ms;
                    return 1;
                }
            }
            return 0;  // 还在等待中
            
        case RECONNECT_STATE_WAIT_SENDOK:
            // 检查是否超时（5秒）
            if (sys_tick_ms - g_upload_monitor.reconnect_step_start_time > 5000) {
                Serial_Printf("[NET] Subscribe SEND OK timeout\r\n");
                g_upload_monitor.reconnect_state = RECONNECT_STATE_FAILED;
                return 1;
            }
            
            // 检查是否有SEND OK
            recv_len = RingBuffer_AT_Read(recv_buf, sizeof(recv_buf) - 1);
            if (recv_len > 0) {
                recv_buf[recv_len] = '\0';
                if (strstr((char *)recv_buf, "SEND OK") != NULL) {
                    Serial_Printf("[NET] Topic subscribed successfully\r\n");
                    g_upload_monitor.reconnect_state = RECONNECT_STATE_SUCCESS;
                    return 1;
                }
            }
            return 0;  // 还在等待中
            
        case RECONNECT_STATE_SUCCESS:
        case RECONNECT_STATE_FAILED:
            // 终态，不应该再执行
            return 1;
            
        default:
            return 1;
    }
}

/**
 * @brief 处理TCP重连逻辑（非阻塞状态机）
 */
static void Handle_TCP_Reconnection(void) {
    extern volatile uint32_t sys_tick_ms;
    
    if (!g_upload_monitor.is_reconnecting) {
        return;  // 不在重连状态
    }
    
    // 检查是否到达重试时间
    if (sys_tick_ms < g_upload_monitor.next_retry_time) {
        // 还未到重试时间，显示倒计时
        uint32_t remaining = (g_upload_monitor.next_retry_time - sys_tick_ms) / 1000;
        static uint32_t last_display = 0;
        if (sys_tick_ms - last_display >= 1000) {  // 每秒更新一次
            Serial_Printf("[NET] Reconnect retry in %lu seconds... (attempt %d/3)\r\n", 
                         remaining, g_upload_monitor.reconnect_attempts + 1);
            last_display = sys_tick_ms;
        }
        return;
    }
    
    // 到达重试时间，执行重连步骤
    if (g_upload_monitor.reconnect_state == RECONNECT_STATE_IDLE) {
        // ⭐ 新增：如果已经在WiFi恢复模式，不要重复启动检测
        if (g_net_state == NET_STATE_WIFI_DISCONNECTED) {
            // WiFi恢复模式正在运行，等待它完成
            return;
        }
        
        // 首次重连前检查WiFi状态
        if (!g_upload_monitor.wifi_connected) {
            // ⭐ 检查是否已有WiFi检测在进行中
            if (g_wifi_check_start_time != 0) {
                // WiFi检测正在进行，等待完成
                return;
            }
            
            Serial_Printf("[NET] Reconnect: WiFi not connected, starting WiFi check...\r\n");
            // 启动WiFi检测
            if (g_upload_monitor.wifi_check_state == WIFI_CHECK_STATE_IDLE) {
                g_upload_monitor.wifi_check_state = WIFI_CHECK_STATE_CHECKING;
            }
            return;  // 等待WiFi检测结果
        }
        
        // WiFi已连接，开始TCP重连
        Serial_Printf("[NET] Attempting reconnection #%d...\r\n", g_upload_monitor.reconnect_attempts + 1);
        Serial_Printf("[NET] === Starting TCP Reconnection ===\r\n");
        g_upload_monitor.reconnect_state = RECONNECT_STATE_CONNECTING;
    }
    
    // 执行一个重连步骤（非阻塞）
    uint8_t step_completed = Reconnect_Step_Execute();
    
    // 如果步骤完成，检查结果
    if (step_completed) {
        if (g_upload_monitor.reconnect_state == RECONNECT_STATE_SUCCESS) {
            // 重连成功
            Serial_Printf("[NET] === TCP Reconnection COMPLETE ===\r\n");
            Serial_Printf("[NET] Reconnection SUCCESS! Resuming uploads.\r\n");
            
            // 重置监控状态
            g_upload_monitor.upload_fail_count = 0;
            g_upload_monitor.tcp_disconnected = 0;
            g_upload_monitor.upload_paused = 0;
            g_upload_monitor.is_reconnecting = 0;
            g_upload_monitor.reconnect_attempts = 0;
            g_upload_monitor.reconnect_state = RECONNECT_STATE_IDLE;
            
            // ⭐ 重置WiFi检测状态
            g_wifi_check_start_time = 0;
            g_upload_monitor.wifi_check_state = WIFI_CHECK_STATE_IDLE;
            
            // 恢复网络状态
            g_net_state = NET_STATE_CONNECTED;
            
        } else if (g_upload_monitor.reconnect_state == RECONNECT_STATE_FAILED) {
            // 重连失败
            Serial_Printf("[NET] Reconnection step FAILED\r\n");
            g_upload_monitor.reconnect_attempts++;
            g_upload_monitor.reconnect_state = RECONNECT_STATE_IDLE;
            
            if (g_upload_monitor.reconnect_attempts >= 3) {
                // 3次都失败，检测WiFi状态
                Serial_Printf("[NET] TCP reconnection FAILED after 3 attempts. Checking WiFi...\r\n");
                g_upload_monitor.is_reconnecting = 0;
                g_upload_monitor.reconnect_attempts = 0;
                
                // 启动WiFi检测状态机（由Handle_WiFi_Check()周期性执行）
                if (g_upload_monitor.wifi_check_state == WIFI_CHECK_STATE_IDLE) {
                    g_upload_monitor.wifi_check_state = WIFI_CHECK_STATE_CHECKING;
                    Serial_Printf("[NET] WiFi check started...\r\n");
                }
                // 不再直接调用Check_WiFi_Connection()，交给Handle_WiFi_Check()处理
            } else {
                // 还有重试机会，计算下次重试时间（指数退避）
                uint32_t delays[] = {RETRY_INTERVAL_LONG_1, RETRY_INTERVAL_LONG_2, RETRY_INTERVAL_LONG_3};
                g_upload_monitor.next_retry_time = sys_tick_ms + delays[g_upload_monitor.reconnect_attempts];
                Serial_Printf("[NET] Will retry in %lu seconds...\r\n", delays[g_upload_monitor.reconnect_attempts] / 1000);
            }
        }
        // 其他状态（等待中），继续下次调用
    }
}

/**
 * @brief 处理WiFi检测（独立状态机，在Network_Core_Task中周期性调用）
 */
static void Handle_WiFi_Check(void) {
    // 如果正在检测WiFi，继续执行
    if (g_upload_monitor.wifi_check_state == WIFI_CHECK_STATE_CHECKING) {
        uint8_t wifi_result = Check_WiFi_Connection();
        
        if (wifi_result == 2) {
            return;  // 还在检测中
        } else if (wifi_result == 1) {
            // WiFi正常
            Serial_Printf("[NET] WiFi check result: CONNECTED\r\n");
            
            // 根据当前网络状态决定下一步
            if (g_net_state == NET_STATE_WIFI_DISCONNECTED) {
                // WiFi恢复，启动TCP重连
                Serial_Printf("[NET] WiFi recovered! Starting TCP reconnection...\r\n");
                g_upload_monitor.wifi_check_enabled = 0;
                g_upload_monitor.wifi_connected = 1;
                
                // 启动TCP重连流程（立即开始，不等待）
                g_net_state = NET_STATE_RECONNECTING;
                g_upload_monitor.is_reconnecting = 1;
                g_upload_monitor.reconnect_attempts = 0;
                g_upload_monitor.next_retry_time = sys_tick_ms;  // 立即开始
                g_upload_monitor.reconnect_state = RECONNECT_STATE_IDLE;
                g_upload_monitor.tcp_disconnected = 1;
                g_upload_monitor.upload_paused = 1;
                
                Serial_Printf("[NET] Will attempt TCP connection immediately...\r\n");
            } else {
                // 其他场景（如TCP断开时检测），启动TCP重连
                Serial_Printf("[NET] WiFi is OK. Starting TCP reconnection...\r\n");
                
                // 启动TCP重连流程（立即开始，不等待）
                g_net_state = NET_STATE_RECONNECTING;
                g_upload_monitor.is_reconnecting = 1;
                g_upload_monitor.reconnect_attempts = 0;
                g_upload_monitor.next_retry_time = sys_tick_ms;  // 立即开始
                g_upload_monitor.reconnect_state = RECONNECT_STATE_IDLE;
                g_upload_monitor.tcp_disconnected = 1;
                g_upload_monitor.upload_paused = 1;
                
                Serial_Printf("[NET] Will attempt TCP connection immediately...\r\n");
            }
            
            g_upload_monitor.wifi_check_state = WIFI_CHECK_STATE_IDLE;
            
        } else {
            // WiFi断开
            Serial_Printf("[NET] WiFi check result: DISCONNECTED\r\n");
            
            // ⭐ 新增：跟踪连续断开次数（使用全局变量）
            g_wifi_consecutive_disconnect_count++;
            
            if (g_net_state != NET_STATE_WIFI_DISCONNECTED) {
                // 首次检测到WiFi断开
                g_net_state = NET_STATE_WIFI_DISCONNECTED;
                g_upload_monitor.wifi_check_enabled = 1;
                g_upload_monitor.wifi_check_next_time = sys_tick_ms + WIFI_CHECK_INTERVAL;
                g_upload_monitor.wifi_connected = 0;
                Serial_Printf("[NET] Entering WiFi recovery mode...\r\n");
            } else {
                // 已经在WiFi断开状态，设置下次检测时间
                g_upload_monitor.wifi_check_next_time = sys_tick_ms + WIFI_CHECK_INTERVAL;
                Serial_Printf("[NET] WiFi still disconnected. Next check in 20 seconds...\r\n");
                
                // ⭐ 新增：连续3次检测断开，触发WiFi重连（不复位模块）
                if (g_wifi_consecutive_disconnect_count >= 3) {
                    Serial_Printf("[NET] WiFi disconnected for 3 consecutive checks. Triggering WiFi reconnection...\r\n");
                    g_wifi_consecutive_disconnect_count = 0;  // 重置计数器
                    
                    // 启动WiFi重连（复用初始化中的WiFi连接逻辑）
                    g_net_state = NET_STATE_INITIALIZING;  // 临时切换到初始化状态
                    g_init_step = INIT_STEP_CONNECT_WIFI;  // 直接跳到WiFi连接步骤
                    g_wifi_retry_count = 0;
                    g_oled_locked = 1;  // 锁定OLED
                    
                    Serial_Printf("[NET] Starting WiFi reconnection without reset...\r\n");
                    return;  // 退出WiFi检测，让Network_Core_Task处理初始化
                }
            }
            
            g_upload_monitor.wifi_check_state = WIFI_CHECK_STATE_IDLE;
        }
    }
}

/**
 * @brief 处理WiFi恢复检测（非阻塞状态机）
 */
static void Handle_WiFi_Recovery(void) {
    if (g_net_state != NET_STATE_WIFI_DISCONNECTED) {
        return;  // 不在WiFi断开状态
    }
    
    // 更新OLED显示
    Show_WiFi_Disconnected();
    
    if (!g_upload_monitor.wifi_check_enabled) {
        return;  // 未启用WiFi检测
    }
    
    // 检查是否到达检测时间
    if (sys_tick_ms < g_upload_monitor.wifi_check_next_time) {
        // 还未到检测时间，显示倒计时
        uint32_t remaining = (g_upload_monitor.wifi_check_next_time - sys_tick_ms) / 1000;
        static uint32_t last_display = 0;
        if (sys_tick_ms - last_display >= 1000) {  // 每秒更新一次
            Serial_Printf("[NET] WiFi recovery check in %lu seconds...\r\n", remaining);
            last_display = sys_tick_ms;
        }
        return;
    }
    
    // 到达检测时间，启动WiFi检测（由Handle_WiFi_Check()执行）
    if (g_upload_monitor.wifi_check_state == WIFI_CHECK_STATE_IDLE) {
        // ⭐ 检查是否已有WiFi检测在进行中
        if (g_wifi_check_start_time != 0) {
            // WiFi检测正在进行，等待完成
            return;
        }
        
        g_upload_monitor.wifi_check_state = WIFI_CHECK_STATE_CHECKING;
        Serial_Printf("[NET] WiFi check started (recovery mode)...\r\n");
    }
    // 不再直接调用Check_WiFi_Connection()，交给Handle_WiFi_Check()处理
}

/**
 * @brief 预检WiFi是否已连接（初始化前调用）
 * @return 1=已连接, 0=未连接
 */
static uint8_t PreCheck_WiFi_Connection(void) {
    Serial_Printf("[NET] Pre-checking WiFi connection...\r\n");
    
    // 发送AT+CWJAP?查询当前WiFi连接状态
    RingBuffer_AT_Clear();
    
    const char *cmd = "AT+CWJAP?\r\n";
    const char *p = cmd;
    while (*p) {
        USART_SendData(ESP8266_USART, (uint8_t)*p++);
        while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
    }
    
    // 等待响应（最多8秒，给模块足够时间响应）
    uint32_t start_time = sys_tick_ms;
    uint8_t recv_buf[128];
    
    while (sys_tick_ms - start_time < 8000) {
        
        uint16_t recv_len = RingBuffer_AT_Read(recv_buf, sizeof(recv_buf) - 1);
        if (recv_len > 0) {
            recv_buf[recv_len] = '\0';
            
            if (strstr((char *)recv_buf, "+CWJAP:") != NULL) {
                // 收到+CWJAP:，WiFi已连接
                Serial_Printf("[NET] WiFi pre-check: ALREADY CONNECTED\r\n");
                RingBuffer_AT_Clear();
                return 1;
            } else if (strstr((char *)recv_buf, "OK") != NULL || strstr((char *)recv_buf, "ERROR") != NULL) {
                // 收到OK或ERROR但没有+CWJAP:，WiFi未连接
                Serial_Printf("[NET] WiFi pre-check: NOT CONNECTED\r\n");
                RingBuffer_AT_Clear();
                return 0;
            }
            // 其他响应，继续等待
        }
    }
    
    // 超时，认为未连接
    Serial_Printf("[NET] WiFi pre-check: TIMEOUT (assume disconnected)\r\n");
    RingBuffer_AT_Clear();
    return 0;
}

/**
 * @brief 执行初始化步骤
 */
static void Init_Step_Execute(void) {
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
  
  char cmd_buf[128];
  uint8_t tcp_result; // TCP连接结果
  uint16_t data_len;  // 数据长度

  /* WiFi重试超时配置（5s -> 6s -> 8s）*/
  static const uint32_t wifi_timeout[] = {5000, 6000, 8000};

  /* TCP重试超时配置（10s -> 15s -> 20s）*/
  static const uint32_t tcp_timeout[] = {10000, 15000, 20000};

  /* 订阅重试超时配置（3s -> 4s -> 5s）*/
  static const uint32_t subscribe_timeout[] = {3000, 4000, 5000};

  switch (g_init_step) {
  case INIT_STEP_RESET:
    Serial_Printf("[NET] Step 1: Resetting module...\r\n");

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
        Serial_Printf(
            "[NET] AT test failed after 3 retries, going OFFLINE\r\n");
        g_net_state = NET_STATE_OFFLINE;
        g_init_step = INIT_STEP_IDLE;
        retry = 0; // 重置计数器，以便下次初始化

        // 显示离线提示并清屏
        Show_Network_Offline();
        g_oled_locked = 0;
      }
    }
    break;

  case INIT_STEP_SET_STA:
    Serial_Printf("[NET] Step 3: Setting STA mode...\r\n");
    if (WiFi_Send_AT_Command("AT+CWMODE=1\r\n", "OK", 1000)) {
      Serial_Printf("[NET] STA mode set OK\r\n");
      OLED_ShowString(60, 6, "50%", 16);

      // STA模式设置完成后，直接进入WiFi连接步骤
      g_wifi_retry_count = 0; // 初始化计数器
      g_init_step = INIT_STEP_CONNECT_WIFI;
    } else {
      Serial_Printf("[NET] STA mode FAILED\r\n");
      Show_Network_Failure("WiFi mode setting failed");
      g_net_state = NET_STATE_OFFLINE;
      g_init_step = INIT_STEP_IDLE;

      // 显示离线提示并清屏
      Show_Network_Offline();
      g_oled_locked = 0; // 解锁OLED
    }
    break;

  case INIT_STEP_CONNECT_WIFI:
    // 检查是否需要等待2秒重试间隔
    if (g_wifi_retry_count > 0 && sys_tick_ms - g_last_retry_time < RETRY_INTERVAL_SHORT) {
      // 还在等待2秒间隔
      return;
    }

    Serial_Printf("[NET] Step 4: Connecting WiFi (attempt %d/%d)...\r\n",
                  g_wifi_retry_count + 1, INIT_MAX_RETRY);
    sprintf(cmd_buf, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);

    // 根据重试次数选择超时时间
    uint32_t current_timeout = wifi_timeout[g_wifi_retry_count < INIT_MAX_RETRY
                                                ? g_wifi_retry_count
                                                : (INIT_MAX_RETRY - 1)];

    Serial_Printf("[NET] WiFi timeout: %lums\r\n", current_timeout);

    // 先清空AT缓冲区，然后发送指令
    RingBuffer_AT_Clear();

    if (WiFi_Send_AT_Command(cmd_buf, "WIFI CONNECTED", current_timeout)) {
      Serial_Printf("[NET] WiFi connected OK\r\n");
      g_wifi_retry_count = 0; // 重置计数器
      
      // ⭐ 新增：重置所有相关计数器，避免显示异常
      g_tcp_retry_count = 0;
      g_subscribe_retry_count = 0;
      
      // ⭐ 新增：重置WiFi连续断开计数器
      g_wifi_consecutive_disconnect_count = 0;
      
      OLED_ShowString(60, 6, "70%", 16);

      g_init_step = INIT_STEP_CONNECT_TCP;
    } else {
      Serial_Printf("[NET] WiFi connection FAILED (attempt %d)\r\n",
                    g_wifi_retry_count + 1);
      g_wifi_retry_count++;

      if (g_wifi_retry_count >= INIT_MAX_RETRY) {
        // 3次重试都失败，进入重连状态
        Serial_Printf("[NET] WiFi connection failed after %d attempts\r\n",
                      INIT_MAX_RETRY);

        // 标记所有状态为未连接
        g_wifi_init_connected = 0;
        g_tcp_init_connected = 0;
        g_subscribed = 0;

        // 显示失败信息（3秒倒计时）
        Show_Network_Init_Failure("WiFi connect failed");

        // 进入重连状态（与运行时重连相同）
        g_net_state = NET_STATE_RECONNECTING;
        g_upload_monitor.is_reconnecting = 1;
        g_upload_monitor.reconnect_attempts = 0;
        g_upload_monitor.next_retry_time = sys_tick_ms + RETRY_INTERVAL_LONG_1; // 10秒后开始重连
        g_upload_monitor.reconnect_state = RECONNECT_STATE_IDLE;

        // 解锁OLED
        g_oled_locked = 0;
      } else {
        // 还有重试机会，记录时间并等待2秒后重试
        Serial_Printf("[NET] Will retry WiFi connection in 2 seconds...\r\n");
        g_last_retry_time = sys_tick_ms;
        // 保持当前状态，下次调用时检查时间
      }
    }
    break;

  case INIT_STEP_CONNECT_TCP:
    // 检查是否需要等待2秒重试间隔
    if (g_tcp_retry_count > 0 && sys_tick_ms - g_last_retry_time < RETRY_INTERVAL_SHORT) {
      // 还在等待2秒间隔
      return;
    }

    Serial_Printf("[NET] Step 5: Connecting TCP (attempt %d/%d)...\r\n",
                  g_tcp_retry_count + 1, INIT_MAX_RETRY);

    // 先清空缓冲区
    RingBuffer_AT_Clear();

    // 尝试使用域名连接
    sprintf(cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", BEMFA_SERVER_IP,
            BEMFA_SERVER_PORT);
    Serial_Printf("[NET] Connecting to %s:%s...\r\n", BEMFA_SERVER_IP,
                  BEMFA_SERVER_PORT);

    // 根据重试次数选择超时时间
    uint32_t current_tcp_timeout =
        tcp_timeout[g_tcp_retry_count < INIT_MAX_RETRY ? g_tcp_retry_count
                                                       : (INIT_MAX_RETRY - 1)];

    Serial_Printf("[NET] TCP timeout: %lums\r\n", current_tcp_timeout);

    // 尝试CONNECT或OK响应
    tcp_result = WiFi_Send_AT_Command(cmd_buf, "CONNECT", current_tcp_timeout);

    if (tcp_result) {
      Serial_Printf("[NET] TCP connected (response: CONNECT)\r\n");
      g_tcp_retry_count = 0; // 重置计数器
      OLED_ShowString(60, 6, "90%", 16);
      g_tcp_connected = 1;
      g_init_step = INIT_STEP_SUBSCRIBE;
    } else {
      // 尝试检查是否有OK响应
      RingBuffer_AT_Clear();
      sprintf(cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", BEMFA_SERVER_IP,
              BEMFA_SERVER_PORT);
      if (WiFi_Send_AT_Command(cmd_buf, "OK", current_tcp_timeout)) {
        Serial_Printf("[NET] TCP connected (response: OK)\r\n");
        g_tcp_retry_count = 0; // 重置计数器
        OLED_ShowString(60, 6, "90%", 16);
        g_tcp_connected = 1;
        g_init_step = INIT_STEP_SUBSCRIBE;
      } else {
        Serial_Printf("[NET] TCP connection FAILED (attempt %d)\r\n",
                      g_tcp_retry_count + 1);
        g_tcp_retry_count++;

        if (g_tcp_retry_count >= INIT_MAX_RETRY) {
          // 3次重试都失败，进入重连状态
          Serial_Printf("[NET] TCP connection failed after %d attempts\r\n",
                        INIT_MAX_RETRY);

          // WiFi已连接，但TCP未连接
          g_wifi_init_connected = 1; // 保持WiFi连接状态
          g_tcp_init_connected = 0;
          g_subscribed = 0;

          // 显示失败信息（3秒倒计时）
          Show_Network_Init_Failure("Bemfa connect failed");

          // 进入重连状态
          g_net_state = NET_STATE_RECONNECTING;
          g_upload_monitor.is_reconnecting = 1;
          g_upload_monitor.reconnect_attempts = 0;
          g_upload_monitor.next_retry_time = sys_tick_ms + RETRY_INTERVAL_LONG_1;
          g_upload_monitor.reconnect_state = RECONNECT_STATE_IDLE;

          g_oled_locked = 0; // 解锁OLED

          Serial_Printf(
              "[NET] No response received (timeout or DNS failed)\r\n");
          Serial_Printf(
              "[NET] Hint: Try using IP address instead of domain\r\n");
        } else {
          // 还有重试机会，记录时间并等待2秒后重试
          Serial_Printf("[NET] Will retry TCP connection in 2 seconds...\r\n");
          g_last_retry_time = sys_tick_ms;
          // 保持当前状态，下次调用时检查时间
        }
      }
    }
    break;

  case INIT_STEP_SUBSCRIBE:
    // 检查是否需要等待2秒重试间隔
    if (g_subscribe_retry_count > 0 && sys_tick_ms - g_last_retry_time < RETRY_INTERVAL_SHORT) {
      // 还在等待2秒间隔
      return;
    }

    Serial_Printf("[NET] Step 6: Subscribing topic (attempt %d/%d)...\r\n",
                  g_subscribe_retry_count + 1, INIT_MAX_RETRY);

    // 只订阅control主题（接收控制指令）
    // data主题用于上传，不需要订阅（避免收到自己上传的数据回显）
    // online主题对硬件系统无用，不订阅
    char subscribe_cmd[128];
    sprintf(subscribe_cmd, "cmd=1&uid=%s&topic=%s\r\n", BEMFA_UID,
            BEMFA_TOPIC_CONTROL); // 只订阅control主题

    Serial_Printf("[NET] Subscribe cmd: %s", subscribe_cmd);

    // 先清空缓冲区
    RingBuffer_AT_Clear();

    // 使用AT+CIPSEND两阶段发送（与备份代码一致）
    data_len = strlen(subscribe_cmd);
    char cipsend_cmd[32];
    sprintf(cipsend_cmd, "AT+CIPSEND=%d\r\n", data_len);

    Serial_Printf("[NET] Sending CIPSEND: %s", cipsend_cmd);

    // 根据重试次数选择超时时间
    uint32_t current_sub_timeout =
        subscribe_timeout[g_subscribe_retry_count < INIT_MAX_RETRY
                              ? g_subscribe_retry_count
                              : (INIT_MAX_RETRY - 1)];

    // 阶段1：发送CIPSEND指令，等待 ">"
    if (WiFi_Send_AT_Command(cipsend_cmd, ">", current_sub_timeout)) {
      Serial_Printf("[NET] Got '>' prompt, sending subscription data...\r\n");

      // 阶段2：发送实际的订阅数据
      WiFi_Send_Data((uint8_t *)subscribe_cmd, data_len);

      // 阶段3：等待 "SEND OK"
      if (WiFi_Send_AT_Command("", "SEND OK", current_sub_timeout)) {
        Serial_Printf("[NET] Data sent OK\r\n");

        // 订阅指令发送成功，不等待服务器响应（与备份代码一致）
        // 服务器可能会异步返回 cmd=1&res=1，但我们不阻塞等待
        Serial_Printf("[NET] Topic subscribed (async response expected)\r\n");

        // 标记WiFi已连接（供OLED显示使用）
        g_upload_monitor.wifi_connected = 1;

        // 同步初始化状态变量
        g_wifi_init_connected = 1;
        g_tcp_init_connected = 1;
        g_subscribed = 1;
        g_init_retry_enabled = 0; // 禁用重试

        // 显示联网成功
        Show_Network_Success();

        g_init_step = INIT_STEP_COMPLETE;
        g_net_state = NET_STATE_CONNECTED;
        g_subscribe_retry_count = 0; // 重置计数器
      } else {
        Serial_Printf("[NET] SEND OK not received (attempt %d)\r\n",
                      g_subscribe_retry_count + 1);
        g_subscribe_retry_count++;

        if (g_subscribe_retry_count >= INIT_MAX_RETRY) {
          // 3次重试都失败，进入重连状态
          Serial_Printf("[NET] Subscribe failed after %d attempts\r\n",
                        INIT_MAX_RETRY);

          // WiFi和TCP都正常，只是订阅失败
          g_wifi_init_connected = 1;
          g_tcp_init_connected = 1;
          g_subscribed = 0;

          // 显示失败信息（3秒倒计时）
          Show_Network_Init_Failure("Subscribe failed");

          // 进入重连状态
          g_net_state = NET_STATE_RECONNECTING;
          g_upload_monitor.is_reconnecting = 1;
          g_upload_monitor.reconnect_attempts = 0;
          g_upload_monitor.next_retry_time = sys_tick_ms + RETRY_INTERVAL_LONG_1;
          g_upload_monitor.reconnect_state = RECONNECT_STATE_IDLE;

          g_oled_locked = 0; // 解锁OLED
        } else {
          // 还有重试机会，记录时间并等待2秒后重试
          Serial_Printf("[NET] Will retry subscription in 2 seconds...\r\n");
          g_last_retry_time = sys_tick_ms;
          // 保持当前状态，下次调用时检查时间
        }
      }
    } else {
      Serial_Printf("[NET] CIPSEND command failed (attempt %d)\r\n",
                    g_subscribe_retry_count + 1);
      g_subscribe_retry_count++;

      if (g_subscribe_retry_count >= INIT_MAX_RETRY) {
        // 3次重试都失败，进入重连状态
        Serial_Printf("[NET] Subscribe failed after %d attempts\r\n",
                      INIT_MAX_RETRY);

        // WiFi和TCP都正常，只是订阅失败
        g_wifi_init_connected = 1;
        g_tcp_init_connected = 1;
        g_subscribed = 0;

        // 显示失败信息（3秒倒计时）
        Show_Network_Init_Failure("Subscribe failed");

        // 进入重连状态
        g_net_state = NET_STATE_RECONNECTING;
        g_upload_monitor.is_reconnecting = 1;
        g_upload_monitor.reconnect_attempts = 0;
        g_upload_monitor.next_retry_time = sys_tick_ms + 10000;
        g_upload_monitor.reconnect_state = RECONNECT_STATE_IDLE;

        g_oled_locked = 0; // 解锁OLED
      } else {
        // 还有重试机会，记录时间并等待2秒后重试
        Serial_Printf("[NET] Will retry subscription in 2 seconds...\r\n");
        g_last_retry_time = sys_tick_ms;
        // 保持当前状态，下次调用时检查时间
      }
    }
    break;

  case INIT_STEP_COMPLETE:
    // ⭐ 初始化完成，设置正确状态
    Serial_Printf("[NET] === Network initialization COMPLETE ===\r\n");
    
    // 设置网络状态为已连接
    g_net_state = NET_STATE_CONNECTED;
    
    // ⭐ 重置重连标志（避免WiFi重连后立即进入重连流程）
    g_upload_monitor.is_reconnecting = 0;
    g_upload_monitor.reconnect_attempts = 0;
    g_upload_monitor.reconnect_state = RECONNECT_STATE_IDLE;
    
    // ⭐ 重置WiFi检测状态
    g_wifi_check_start_time = 0;
    g_upload_monitor.wifi_check_state = WIFI_CHECK_STATE_IDLE;
    
    // 解锁OLED
    g_oled_locked = 0;
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
    
    // ⭐ 在复位前先预检WiFi是否已连接
    Serial_Printf("[NET] Pre-checking WiFi before reset...\r\n");
    if (PreCheck_WiFi_Connection()) {
        Serial_Printf("[NET] WiFi already connected, skipping reset and WiFi connect\r\n");
        // WiFi已连接，跳过复位和WiFi连接，直接从TCP连接开始
        g_wifi_init_connected = 1;
        g_tcp_init_connected = 0;
        g_subscribed = 0;
        g_init_step = INIT_STEP_CONNECT_TCP;  // 直接跳到TCP连接
    } else {
        Serial_Printf("[NET] WiFi not connected, will reset module and connect\r\n");
        // WiFi未连接，需要复位模块并从AT测试开始
        g_wifi_init_connected = 0;
        g_tcp_init_connected = 0;
        g_subscribed = 0;
        g_init_step = INIT_STEP_RESET;  // 从复位开始
    }
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
    if (Cloud_Parse_Command((const uint8_t *)frame_buf, frame_len, &g_pending_cmd)) {
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
    
    // 处理WiFi检测（独立状态机）
    Handle_WiFi_Check();
    
    // 处理WiFi恢复检测（非阻塞）
    Handle_WiFi_Recovery();
    
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
    
    // WiFi断开状态
    if (g_net_state == NET_STATE_WIFI_DISCONNECTED) {
        static uint8_t wifi_disconnected_logged = 0;
        if (!wifi_disconnected_logged) {
            Serial_Printf("[NET] *** WiFi is DISCONNECTED ***\r\n");
            wifi_disconnected_logged = 1;
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
                uint32_t timeout = 4000 + (retry * 1000);  // 4s, 5s, 6s
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
                // TCP已断开，先检测WiFi状态
                Serial_Printf("[NET] TCP disconnected! Checking WiFi status first...\r\n");
                g_upload_monitor.tcp_disconnected = 1;
                g_upload_monitor.upload_paused = 1;
                
                // 启动WiFi检测状态机（由Handle_WiFi_Check()周期性执行）
                if (g_upload_monitor.wifi_check_state == WIFI_CHECK_STATE_IDLE) {
                    g_upload_monitor.wifi_check_state = WIFI_CHECK_STATE_CHECKING;
                    Serial_Printf("[NET] WiFi check started...\r\n");
                }
                // 不再直接调用Check_WiFi_Connection()，交给Handle_WiFi_Check()处理
                return 1;
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
