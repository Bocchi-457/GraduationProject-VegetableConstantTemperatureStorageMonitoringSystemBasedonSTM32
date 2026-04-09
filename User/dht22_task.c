/**
 * @file dht22_task.c
 * @brief DHT22温湿度传感器读取任务模块
 * @note 遵循非阻塞原则，每2秒读取一次
 */

#include "dht22_task.h"
#include "dht22.h"
#include "Timer.h"
#include <stdio.h>
#include <stdlib.h>       // abs()函数

#if DEBUG_DHT22_TASK
  #include "Usart.h"
  #define DHT22_LOG(fmt, ...) Serial_Printf("[DHT22] " fmt, ##__VA_ARGS__)
#else
  #define DHT22_LOG(fmt, ...)
#endif

// 上次读取时间戳
static uint32_t g_last_read_time = 0;

// 数据有效性标志
uint8_t g_dht22_data_valid = 0;

/**
 * @brief DHT22任务初始化
 */
void DHT22_Task_Init(void) {
    // 丢弃第一次读取（上电后DHT22需要稳定）
    Read_DHT22(&DHT22_Data);
    delay_ms(2000);
    
    // 正式读取第一次有效数据
    if (Read_DHT22(&DHT22_Data) == SUCCESS) {
        g_dht22_data_valid = 1;
        DHT22_LOG("Data valid: T=%d.%d, H=%d.%d\r\n", 
                 DHT22_Data.temperature / 10, abs(DHT22_Data.temperature % 10),
                 DHT22_Data.humidity / 10, abs(DHT22_Data.humidity % 10));
    } else {
        g_dht22_data_valid = 0;
        DHT22_LOG("Init failed\r\n");
    }
    
    g_last_read_time = sys_tick_ms;
}

/**
 * @brief DHT22周期性读取任务（非阻塞）
 * @note 每2秒执行一次读取
 */
void DHT22_Task_Run(void) {
    // 限流：每2秒读取一次
    if (sys_tick_ms - g_last_read_time < 2000) {
        return;
    }
    g_last_read_time = sys_tick_ms;
    
    // 中断保护下读取DHT22（约3-4ms）
    __disable_irq();
    uint8_t result = Read_DHT22(&DHT22_Data);
    __enable_irq();
    
    if (result == SUCCESS) {
        g_dht22_data_valid = 1;
        DHT22_LOG("T=%d.%d, H=%d.%d\r\n", 
                 DHT22_Data.temperature / 10, abs(DHT22_Data.temperature % 10),
                 DHT22_Data.humidity / 10, abs(DHT22_Data.humidity % 10));
    } else {
        g_dht22_data_valid = 0;
        DHT22_LOG("Read error\r\n");
    }
}

/**
 * @brief 获取DHT22数据有效性
 * @return 1=有效, 0=无效
 */
uint8_t DHT22_Task_IsDataValid(void) {
    return g_dht22_data_valid;
}
