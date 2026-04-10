/**
 * @file control_task.c
 * @brief 自动控制逻辑任务模块
 * @note 根据温湿度阈值自动控制执行器，每500ms执行一次
 */

#include "control_task.h"
#include "control.h"
#include "dht22.h"
#include "dht22_task.h"   // g_dht22_data_valid
#include "Timer.h"
#include <stdio.h>
#include <stdlib.h>       // abs()函数

#if DEBUG_CONTROL_TASK
  #include "Usart.h"
  #define CONTROL_LOG(fmt, ...) Serial_Printf("[CTRL] " fmt, ##__VA_ARGS__)
#else
  #define CONTROL_LOG(fmt, ...)
#endif

// 上次执行时间戳
static uint32_t g_last_control_time = 0;

// 工作模式（1=自动, 2=手动）
uint8_t g_work_mode = 1;

// 阈值参数（放大10倍）
int16_t g_temp_high = 250;  // 温度上限 25.0℃
int16_t g_temp_low = 200;   // 温度下限 20.0℃
int16_t g_humid_high = 650; // 湿度上限 65.0%
int16_t g_humid_low = 500;  // 湿度下限 50.0%

// 执行器状态
uint8_t g_heater_state = 0;   // 加热
uint8_t g_cooler_state = 0;   // 制冷
uint8_t g_dehumid_state = 0;  // 除湿
uint8_t g_humidifier_state = 0; // 加湿

/**
 * @brief 控制任务初始化
 */
void Control_Task_Init(void) {
    // 初始化执行器GPIO引脚
    jiare_init();     // 加热器GPIO初始化
    zhileng_init();   // 制冷器GPIO初始化
    chushi_init();    // 除湿器GPIO初始化
    jiashi_init();    // 加湿器GPIO初始化
    
    // 默认自动模式
    g_work_mode = 1;
    
    // 默认阈值
    g_temp_high = 250;
    g_temp_low = 200;
    g_humid_high = 650;
    g_humid_low = 500;
    
    // 关闭所有执行器
    jiare = 1;     // 高电平关闭
    zhileng = 1;
    chushi = 1;
    jiashi = 1;
    
    g_heater_state = 0;
    g_cooler_state = 0;
    g_dehumid_state = 0;
    g_humidifier_state = 0;
    
    g_last_control_time = sys_tick_ms;
    
    CONTROL_LOG("Control task initialized (Auto mode)\r\n");
}

/**
 * @brief 自动控制逻辑（非阻塞）
 * @note 每500ms执行一次控制判断
 */
void Control_Task_Run(void) {
    // 限流：每500ms执行一次
    if (sys_tick_ms - g_last_control_time < 500) {
        return;
    }
    g_last_control_time = sys_tick_ms;
    
    // 仅在自动模式下执行控制
    if (g_work_mode != 1) {
        return;
    }
    
    // 检查DHT22数据有效性
    if (!g_dht22_data_valid) {
        CONTROL_LOG("DHT22 data invalid, skip control\r\n");
        return;
    }
    
    int16_t temp = DHT22_Data.temperature;
    int16_t humid = DHT22_Data.humidity;
    
    // 温度控制逻辑（滞回比较防止抖动）
    if (temp > g_temp_high) {
        // 温度过高 → 开启制冷
        zhileng = 0;  // 低电平开启
        g_cooler_state = 1;
        jiare = 1;    // 关闭加热
        g_heater_state = 0;
        CONTROL_LOG("Temp high: %d.%d > %d.%d, cooler ON\r\n",
                   temp / 10, abs(temp % 10),
                   g_temp_high / 10, abs(g_temp_high % 10));
    } else if (temp < g_temp_low) {
        // 温度过低 → 开启加热
        jiare = 0;    // 低电平开启
        g_heater_state = 1;
        zhileng = 1;  // 关闭制冷
        g_cooler_state = 0;
        CONTROL_LOG("Temp low: %d.%d < %d.%d, heater ON\r\n",
                   temp / 10, abs(temp % 10),
                   g_temp_low / 10, abs(g_temp_low % 10));
    } else {
        // 温度正常 → 关闭加热和制冷
        jiare = 1;
        zhileng = 1;
        g_heater_state = 0;
        g_cooler_state = 0;
    }
    
    // 湿度控制逻辑
    if (humid > g_humid_high) {
        // 湿度过高 → 开启除湿
        chushi = 0;   // 低电平开启
        g_dehumid_state = 1;
        jiashi = 1;   // 关闭加湿
        g_humidifier_state = 0;
        CONTROL_LOG("Humid high: %d.%d > %d.%d, dehumid ON\r\n",
                   humid / 10, abs(humid % 10),
                   g_humid_high / 10, abs(g_humid_high % 10));
    } else if (humid < g_humid_low) {
        // 湿度过低 → 开启加湿
        jiashi = 0;   // 低电平开启
        g_humidifier_state = 1;
        chushi = 1;   // 关闭除湿
        g_dehumid_state = 0;
        CONTROL_LOG("Humid low: %d.%d < %d.%d, humidifier ON\r\n",
                   humid / 10, abs(humid % 10),
                   g_humid_low / 10, abs(g_humid_low % 10));
    } else {
        // 湿度正常 → 关闭除湿和加湿
        chushi = 1;
        jiashi = 1;
        g_dehumid_state = 0;
        g_humidifier_state = 0;
    }
}

/**
 * @brief 设置工作模式
 * @param mode: 1=自动模式, 2=手动模式
 */
void Control_Task_SetMode(uint8_t mode) {
    if (mode == 1 || mode == 2) {
        g_work_mode = mode;
        CONTROL_LOG("Mode set to: %s\r\n", mode == 1 ? "Auto" : "Manual");
        
        // 切换到手动模式时，关闭所有执行器
        if (mode == 2) {
            jiare = 1;
            zhileng = 1;
            chushi = 1;
            jiashi = 1;
        }
    }
}

/**
 * @brief 手动控制执行器
 * @param actuator: 执行器类型（CTRL_HEATER/CTRL_COOLER等）
 * @param state: 状态（0=关, 1=开）
 */
void Control_Task_ManualControl(ControlDevice_t actuator, uint8_t state) {
    // 仅在手动模式下允许操作
    if (g_work_mode != 2) {
        CONTROL_LOG("[WARN] Manual control denied in auto mode\r\n");
        return;
    }
    
    switch (actuator) {
        case CTRL_HEATER:
            jiare = (state == 1) ? 0 : 1;  // 低电平开启
            g_heater_state = state;
            CONTROL_LOG("[MANUAL] Heater: %s\r\n", state ? "ON" : "OFF");
            break;
            
        case CTRL_COOLER:
            zhileng = (state == 1) ? 0 : 1;
            g_cooler_state = state;
            CONTROL_LOG("[MANUAL] Cooler: %s\r\n", state ? "ON" : "OFF");
            break;
            
        case CTRL_DEHUMID:
            chushi = (state == 1) ? 0 : 1;
            g_dehumid_state = state;
            CONTROL_LOG("[MANUAL] Dehumidifier: %s\r\n", state ? "ON" : "OFF");
            break;
            
        case CTRL_HUMIDIFIER:
            jiashi = (state == 1) ? 0 : 1;
            g_humidifier_state = state;
            CONTROL_LOG("[MANUAL] Humidifier: %s\r\n", state ? "ON" : "OFF");
            break;
            
        default:
            CONTROL_LOG("[ERR] Invalid actuator: %d\r\n", actuator);
            break;
    }
}

/**
 * @brief 设置温度阈值
 * @param temp_high: 温度上限（放大10倍）
 * @param temp_low: 温度下限（放大10倍）
 */
void Control_Task_SetTempThreshold(int16_t temp_high, int16_t temp_low) {
    if (temp_high > temp_low) {
        g_temp_high = temp_high;
        g_temp_low = temp_low;
        CONTROL_LOG("Temp threshold set: high=%d, low=%d\r\n", temp_high, temp_low);
    }
}

/**
 * @brief 设置湿度阈值
 * @param humid_high: 湿度上限（放大10倍）
 * @param humid_low: 湿度下限（放大10倍）
 */
void Control_Task_SetHumidThreshold(int16_t humid_high, int16_t humid_low) {
    if (humid_high > humid_low) {
        g_humid_high = humid_high;
        g_humid_low = humid_low;
        CONTROL_LOG("Humid threshold set: high=%d, low=%d\r\n", humid_high, humid_low);
    }
}
