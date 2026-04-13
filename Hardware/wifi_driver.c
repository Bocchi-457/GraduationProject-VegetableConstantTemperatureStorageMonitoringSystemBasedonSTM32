#include "wifi_driver.h"
#include "delay.h"
#include "stm32f10x_usart.h"  // 包含USART_SendData等函数
#include <stdio.h>
#include <string.h>

/* DMA接收缓冲区 */
static uint8_t dma_rx_buffer[DMA_BUFFER_SIZE];

/* 环形缓冲区结构 */
typedef struct {
    uint8_t buffer[DMA_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} RingBuffer_t;

static RingBuffer_t g_ring_buffer = {0};

/**
 * @brief 初始化环形缓冲区
 */
static void RingBuffer_Init(void) {
    g_ring_buffer.head = 0;
    g_ring_buffer.tail = 0;
    g_ring_buffer.count = 0;
    memset(g_ring_buffer.buffer, 0, DMA_BUFFER_SIZE);
}

/**
 * @brief 写入数据到环形缓冲区
 */
static void RingBuffer_Write(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        g_ring_buffer.buffer[g_ring_buffer.head] = data[i];
        g_ring_buffer.head = (g_ring_buffer.head + 1) % DMA_BUFFER_SIZE;
        g_ring_buffer.count++;
        
        // 缓冲区满时覆盖旧数据
        if (g_ring_buffer.count > DMA_BUFFER_SIZE) {
            g_ring_buffer.tail = (g_ring_buffer.tail + 1) % DMA_BUFFER_SIZE;
            g_ring_buffer.count = DMA_BUFFER_SIZE;
        }
    }
}

/**
 * @brief 从环形缓冲区读取数据
 */
static uint16_t RingBuffer_Read(uint8_t *data, uint16_t max_len) {
    uint16_t read_len = 0;
    
    while (read_len < max_len && g_ring_buffer.count > 0) {
        data[read_len] = g_ring_buffer.buffer[g_ring_buffer.tail];
        g_ring_buffer.tail = (g_ring_buffer.tail + 1) % DMA_BUFFER_SIZE;
        g_ring_buffer.count--;
        read_len++;
    }
    
    return read_len;
}

/**
 * @brief USART2中断服务函数（RXNE中断）
 */
void USART2_IRQHandler(void) {
    if (USART_GetITStatus(ESP8266_USART, USART_IT_RXNE) != RESET) {
        uint8_t data = (uint8_t)USART_ReceiveData(ESP8266_USART);
        
        // 写入环形缓冲区
        RingBuffer_Write(&data, 1);
        
        // 清除RXNE标志
        USART_ClearITPendingBit(ESP8266_USART, USART_IT_RXNE);
    }
}

/**
 * @brief 初始化WiFi模块（DMA+IDLE中断）
 */
void WiFi_Driver_Init(uint32_t baudrate) {
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    DMA_InitTypeDef DMA_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    
    // 使能时钟
    RCC_APB2PeriphClockCmd(ESP8266_GPIO_CLK | ESP8266_RST_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(ESP8266_USART_CLK, ENABLE);
    RCC_AHBPeriphClockCmd(ESP8266_DMA_CLK, ENABLE);
    
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
    
    // 使能RXNE中断（不使用DMA）
    USART_ITConfig(ESP8266_USART, USART_IT_RXNE, ENABLE);
    
    // 配置NVIC
    NVIC_InitStruct.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
    
    // 使能USART
    USART_Cmd(ESP8266_USART, ENABLE);
    
    // 初始化环形缓冲区
    RingBuffer_Init();
}

/**
 * @brief 复位ESP8266模块
 */
void WiFi_Module_Reset(void) {
    GPIO_ResetBits(ESP8266_RST_PORT, ESP8266_RST_PIN);
    delay_ms(500);
    GPIO_SetBits(ESP8266_RST_PORT, ESP8266_RST_PIN);
    delay_ms(2000);  // 等待模块启动
    
    // 清空缓冲区
    WiFi_Clear_Buffer();
}

/**
 * @brief 发送AT指令（阻塞方式）
 */
uint8_t WiFi_Send_AT_Command(const char *cmd, const char *expected_ack, uint32_t timeout_ms) {
    extern volatile uint32_t sys_tick_ms;  // 系统滴答定时器
    uint32_t start_time = sys_tick_ms;
    uint8_t recv_buf[256];
    uint16_t recv_len = 0;
    
    // 清空缓冲区
    WiFi_Clear_Buffer();
    
    // 发送AT指令（逐字节发送到USART2）
    const char *p = cmd;
    while (*p) {
        USART_SendData(ESP8266_USART, (uint8_t)*p++);
        while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
    }
    
    // 等待响应
    while (sys_tick_ms - start_time < timeout_ms) {
        delay_ms(10);
        
        recv_len = WiFi_Read_Data(recv_buf, sizeof(recv_buf) - 1);
        if (recv_len > 0) {
            recv_buf[recv_len] = '\0';
            // 调试：打印接收到的数据（可选，生产环境可注释）
            // Serial_Printf("[WiFi] Received: %s\r\n", recv_buf);
            
            if (strstr((char *)recv_buf, expected_ack) != NULL) {
                return 1;  // 成功
            }
        }
    }
    
    return 0;  // 超时
}

/**
 * @brief 从DMA缓冲区读取数据
 */
uint16_t WiFi_Read_Data(uint8_t *data, uint16_t max_len) {
    return RingBuffer_Read(data, max_len);
}

/**
 * @brief 清空接收缓冲区
 */
void WiFi_Clear_Buffer(void) {
    RingBuffer_Init();
}

/**
 * @brief 获取接收缓冲区中的数据长度
 */
uint16_t WiFi_Get_Data_Length(void) {
    return g_ring_buffer.count;
}

/**
 * @brief 发送原始数据
 */
void WiFi_Send_Data(const uint8_t *data, uint16_t len) {
    // 逐字节发送，使用TXE标志（与AT指令发送保持一致）
    for (uint16_t i = 0; i < len; i++) {
        USART_SendData(ESP8266_USART, data[i]);
        while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
    }
}
