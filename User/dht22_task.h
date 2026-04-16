/**
 * dht22_task.h
 * DHT22温湿度传感器读取任务头文件
 * 版本：V1.0
 * MCU：STM32F103C8T6
 */

#ifndef __DHT22_TASK_H__
#define __DHT22_TASK_H__

#include "stdint.h"

/**
 * 【调试开关】DHT22任务日志控制
 */
#ifndef DEBUG_DHT22_TASK
  #define DEBUG_DHT22_TASK  1
#endif

// DHT22数据有效性标志（外部可访问）
extern uint8_t g_dht22_data_valid;

/**
 * @brief DHT22任务初始化
 */
void DHT22_Task_Init(void);

/**
 * @brief DHT22周期性读取任务（非阻塞，每2秒执行一次）
 */
void DHT22_Task_Run(void);

/**
 * @brief 获取DHT22数据有效性
 * @return 1=有效, 0=无效
 */
uint8_t DHT22_Task_IsDataValid(void);

#endif // __DHT22_TASK_H__
