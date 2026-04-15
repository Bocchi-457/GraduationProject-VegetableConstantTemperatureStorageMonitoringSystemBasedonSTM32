#include "wifi_driver.h"
#include "delay.h"
#include "stm32f10x_usart.h"
#include <stdio.h>
#include <string.h>

/* 双缓冲区定义 */
typedef struct {
    uint8_t buffer[AT_RING_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} RingBuffer_AT_t;

typedef struct {
    uint8_t buffer[CLOUD_RING_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} RingBuffer_Cloud_t;

static RingBuffer_AT_t g_at_ring_buffer = {0};
static RingBuffer_Cloud_t g_cloud_ring_buffer = {0};

/* ESP8266数据帧解析状态机 */
typedef enum {
    PARSE_STATE_IDLE = 0,
    PARSE_STATE_CHECK_PLUS,
    PARSE_STATE_CHECK_I,
    PARSE_STATE_CHECK_P,
    PARSE_STATE_READ_CLOUD_DATA
} ParseState_t;

static ParseState_t g_parse_state = PARSE_STATE_IDLE;
static uint8_t g_last_cloud_byte = 0;  // 记录云端数据上一个字节，用于检测\r\n

/**
 * @brief AT缓冲区写入
 */
static void RingBuffer_AT_Write(uint8_t data) {
    if (g_at_ring_buffer.count < AT_RING_BUFFER_SIZE) {
        g_at_ring_buffer.buffer[g_at_ring_buffer.head] = data;
        g_at_ring_buffer.head = (g_at_ring_buffer.head + 1) % AT_RING_BUFFER_SIZE;
        g_at_ring_buffer.count++;
    }
}

/**
 * @brief 云端缓冲区写入
 */
static void RingBuffer_Cloud_Write(uint8_t data) {
    if (g_cloud_ring_buffer.count < CLOUD_RING_BUFFER_SIZE) {
        g_cloud_ring_buffer.buffer[g_cloud_ring_buffer.head] = data;
        g_cloud_ring_buffer.head = (g_cloud_ring_buffer.head + 1) % CLOUD_RING_BUFFER_SIZE;
        g_cloud_ring_buffer.count++;
    }
}

/**
 * @brief 从AT缓冲区读取（供外部调用）
 */
uint16_t RingBuffer_AT_Read(uint8_t *data, uint16_t max_len) {
    uint16_t len = 0;
    
    while (len < max_len && g_at_ring_buffer.count > 0) {
        data[len] = g_at_ring_buffer.buffer[g_at_ring_buffer.tail];
        g_at_ring_buffer.tail = (g_at_ring_buffer.tail + 1) % AT_RING_BUFFER_SIZE;
        g_at_ring_buffer.count--;
        len++;
    }
    
    return len;
}

/**
 * @brief 清空AT缓冲区（供外部调用）
 */
void RingBuffer_AT_Clear(void) {
    g_at_ring_buffer.head = 0;
    g_at_ring_buffer.tail = 0;
    g_at_ring_buffer.count = 0;
}

/**
 * @brief USART2中断服务函数（RXNE中断）- 双缓冲区架构
 */
void USART2_IRQHandler(void) {
    if (USART_GetITStatus(ESP8266_USART, USART_IT_RXNE) != RESET) {
        uint8_t data = USART_ReceiveData(ESP8266_USART);
        
        // 状态机处理
        switch (g_parse_state) {
            case PARSE_STATE_IDLE:
                if (data == '+') {
                    g_parse_state = PARSE_STATE_CHECK_PLUS;
                } else {
                    // AT响应或其他数据，放入AT缓冲区
                    RingBuffer_AT_Write(data);
                }
                break;
                
            case PARSE_STATE_CHECK_PLUS:
                if (data == 'I') {
                    g_parse_state = PARSE_STATE_CHECK_I;
                } else {
                    // 不是+I，是其他ESP8266上报
                    RingBuffer_AT_Write('+');
                    RingBuffer_AT_Write(data);
                    g_parse_state = PARSE_STATE_IDLE;
                }
                break;
                
            case PARSE_STATE_CHECK_I:
                if (data == 'P') {
                    g_parse_state = PARSE_STATE_CHECK_P;
                } else {
                    RingBuffer_AT_Write('+');
                    RingBuffer_AT_Write('I');
                    RingBuffer_AT_Write(data);
                    g_parse_state = PARSE_STATE_IDLE;
                }
                break;
                
            case PARSE_STATE_CHECK_P:
                if (data == 'D') {
                    // 确认是+IPD，开始读取云端数据
                    RingBuffer_Cloud_Write('+');
                    RingBuffer_Cloud_Write('I');
                    RingBuffer_Cloud_Write('P');
                    RingBuffer_Cloud_Write('D');
                    g_parse_state = PARSE_STATE_READ_CLOUD_DATA;
                    g_last_cloud_byte = 0;  // 重置上一个字节记录
                } else {
                    RingBuffer_AT_Write('+');
                    RingBuffer_AT_Write('I');
                    RingBuffer_AT_Write('P');
                    RingBuffer_AT_Write(data);
                    g_parse_state = PARSE_STATE_IDLE;
                }
                break;
                
            case PARSE_STATE_READ_CLOUD_DATA:
                // 所有后续数据都放入云端缓冲区
                RingBuffer_Cloud_Write(data);
                
                // 检测帧结束符 \r\n
                if (g_last_cloud_byte == '\r' && data == '\n') {
                    // 完整帧接收完毕，回到空闲状态
                    g_parse_state = PARSE_STATE_IDLE;
                }
                g_last_cloud_byte = data;
                break;
                
            default:
                g_parse_state = PARSE_STATE_IDLE;
                break;
        }
        
        USART_ClearITPendingBit(ESP8266_USART, USART_IT_RXNE);
    }
}

/**
 * @brief 初始化WiFi模块（双缓冲区架构）
 */
void WiFi_Driver_Init(uint32_t baudrate) {
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    
    // 使能时钟
    RCC_APB2PeriphClockCmd(ESP8266_GPIO_CLK | ESP8266_RST_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(ESP8266_USART_CLK, ENABLE);
    
    // 配置TX引脚（PA2）
    GPIO_InitStruct.GPIO_Pin = ESP8266_TX_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ESP8266_GPIO_PORT, &GPIO_InitStruct);
    
    // 配置RX引脚（PA3）
    GPIO_InitStruct.GPIO_Pin = ESP8266_RX_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(ESP8266_GPIO_PORT, &GPIO_InitStruct);
    
    // 配置RST引脚（PA1）
    GPIO_InitStruct.GPIO_Pin = ESP8266_RST_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ESP8266_RST_PORT, &GPIO_InitStruct);
    GPIO_SetBits(ESP8266_RST_PORT, ESP8266_RST_PIN);
    
    // 配置USART2
    USART_InitStruct.USART_BaudRate = baudrate;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(ESP8266_USART, &USART_InitStruct);
    
    // 配置NVIC
    NVIC_InitStruct.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
    
    // 使能RXNE中断
    USART_ITConfig(ESP8266_USART, USART_IT_RXNE, ENABLE);
    
    // 使能USART2
    USART_Cmd(ESP8266_USART, ENABLE);
    
    // 初始化双缓冲区
    g_at_ring_buffer.head = 0;
    g_at_ring_buffer.tail = 0;
    g_at_ring_buffer.count = 0;
    
    g_cloud_ring_buffer.head = 0;
    g_cloud_ring_buffer.tail = 0;
    g_cloud_ring_buffer.count = 0;
    
    // 初始化解析状态机
    g_parse_state = PARSE_STATE_IDLE;
}

/**
 * @brief 复位ESP8266模块
 */
void WiFi_Module_Reset(void) {
    GPIO_ResetBits(ESP8266_RST_PORT, ESP8266_RST_PIN);
    delay_ms(500);
    GPIO_SetBits(ESP8266_RST_PORT, ESP8266_RST_PIN);
    delay_ms(2000);  // 等待模块启动
    
    // 清空双缓冲区
    RingBuffer_AT_Clear();
    g_cloud_ring_buffer.head = 0;
    g_cloud_ring_buffer.tail = 0;
    g_cloud_ring_buffer.count = 0;
    
    // 重置状态机
    g_parse_state = PARSE_STATE_IDLE;
}

/**
 * @brief 发送AT指令（阻塞方式，响应确认后清空缓冲区）
 */
uint8_t WiFi_Send_AT_Command(const char *cmd, const char *expected_ack, uint32_t timeout_ms) {
    extern volatile uint32_t sys_tick_ms;
    uint32_t start_time = sys_tick_ms;
    uint8_t recv_buf[256];
    uint16_t recv_len = 0;
    
    // ✅ 不在这里清空缓冲区，保留可能已到达的响应
    
    // 发送AT指令
    const char *p = cmd;
    while (*p) {
        USART_SendData(ESP8266_USART, (uint8_t)*p++);
        while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
    }
    
    // 等待响应
    while (sys_tick_ms - start_time < timeout_ms) {
        delay_ms(10);
        
        recv_len = RingBuffer_AT_Read(recv_buf, sizeof(recv_buf) - 1);
        if (recv_len > 0) {
            recv_buf[recv_len] = '\0';
            
            if (strstr((char *)recv_buf, expected_ack) != NULL) {
                // ⭐ 打印完整响应内容，用于调试
                extern void Serial_Printf(const char *format, ...);
                Serial_Printf("[NET][DBG] AT Response [%s]: [%s]\r\n", expected_ack, recv_buf);
                
                // ✅ 确认响应后清空
                RingBuffer_AT_Clear();
                return 1;
            }
        }
    }
    
    // 超时也清空，避免残留
    RingBuffer_AT_Clear();
    return 0;
}

/**
 * @brief 从云端缓冲区读取完整帧（基于\r\n判断）
 */
uint16_t WiFi_Read_Cloud_Complete_Frame(uint8_t *data, uint16_t max_len) {
    // 查找 \r\n
    for (uint16_t i = 0; i < g_cloud_ring_buffer.count - 1; i++) {
        uint16_t pos = (g_cloud_ring_buffer.tail + i) % CLOUD_RING_BUFFER_SIZE;
        uint16_t next_pos = (pos + 1) % CLOUD_RING_BUFFER_SIZE;
        
        if (g_cloud_ring_buffer.buffer[pos] == '\r' && 
            g_cloud_ring_buffer.buffer[next_pos] == '\n') {
            
            // 找到完整帧
            uint16_t frame_len = i + 2;
            
            if (frame_len > max_len) {
                // 帧太长，丢弃整个帧
                g_cloud_ring_buffer.tail = (g_cloud_ring_buffer.tail + frame_len) % CLOUD_RING_BUFFER_SIZE;
                g_cloud_ring_buffer.count -= frame_len;
                return 0;
            }
            
            // 读取完整帧
            for (uint16_t j = 0; j < frame_len; j++) {
                data[j] = g_cloud_ring_buffer.buffer[(g_cloud_ring_buffer.tail + j) % CLOUD_RING_BUFFER_SIZE];
            }
            
            // 从缓冲区移除已读数据
            g_cloud_ring_buffer.tail = (g_cloud_ring_buffer.tail + frame_len) % CLOUD_RING_BUFFER_SIZE;
            g_cloud_ring_buffer.count -= frame_len;
            
            return frame_len;
        }
    }
    
    return 0;  // 没有完整帧
}

/**
 * @brief 获取云端缓冲区数据量
 */
uint16_t WiFi_Get_Cloud_Data_Length(void) {
    return g_cloud_ring_buffer.count;
}

/**
 * @brief 预览云端缓冲区内容（不移除数据）
 */
uint16_t WiFi_Peek_Cloud_Data(uint8_t *data, uint16_t max_len) {
    uint16_t len = 0;
    uint16_t pos = g_cloud_ring_buffer.tail;
    
    while (len < max_len && len < g_cloud_ring_buffer.count) {
        data[len] = g_cloud_ring_buffer.buffer[pos];
        pos = (pos + 1) % CLOUD_RING_BUFFER_SIZE;
        len++;
    }
    
    return len;
}

/**
 * @brief 发送原始数据（用于AT+CIPSEND第二阶段）
 */
void WiFi_Send_Data(const uint8_t *data, uint16_t len) {
    // 逐字节发送，使用TXE标志
    for (uint16_t i = 0; i < len; i++) {
        USART_SendData(ESP8266_USART, data[i]);
        while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
    }
}
