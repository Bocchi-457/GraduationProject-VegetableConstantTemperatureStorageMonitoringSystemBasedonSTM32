/**
 * @file control_task.c
 * @brief 自动控制逻辑任务模块
 * @note 根据温湿度阈值自动控制执行器，每500ms执行一次
 */

#include "control_task.h"
#include "control.h"
#include "dht22.h"
#include "dht22_task.h"   // g_dht22_data_valid
#include "flash_config.h"  // ✅ 新增：Flash配置持久化
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

// ✅ 新增：Flash延迟保存机制（防止频繁擦写）
static uint32_t g_last_config_change_time = 0;  // 最后一次配置修改时间
static uint8_t s_config_dirty = 0;              // 配置是否已修改（待保存）
#define CONFIG_SAVE_DELAY_MS  5000  // 延迟5秒后保存到Flash

// ✅ 新增：超范围报警状态
static uint8_t s_beep_count = 0;      // 蜂鸣器计数
static uint8_t s_beep_state = 0;      // 蜂鸣器状态

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
    
    // ✅ 从Flash加载配置（如果无效则使用默认值）
    Flash_Config_Init();
    
    // 从Flash配置中读取参数
    SystemConfig_t *config = Flash_Config_Get();
    g_temp_high = config->temperature_high;
    g_temp_low = config->temperature_low;
    g_humid_high = config->humidity_high;
    g_humid_low = config->humidity_low;
    g_work_mode = config->work_mode;
    
    // 关闭所有执行器（低电平=关闭，高电平=打开）
    jiare = 0;
    zhileng = 0;
    chushi = 0;
    jiashi = 0;
    
    g_heater_state = 0;
    g_cooler_state = 0;
    g_dehumid_state = 0;
    g_humidifier_state = 0;
    
    // ✅ 新增：初始化蜂鸣器为关闭状态（高电平=关）
    beep = 1;
    s_beep_state = 0;
    s_beep_count = 0;
    
    g_last_control_time = sys_tick_ms;
    
    CONTROL_LOG("Control task initialized (Mode=%d)\r\n", g_work_mode);
}

/**
 * @brief 自动控制逻辑（非阻塞）
 * @note 每500ms执行一次控制判断
 */
void Control_Task_Run(void) {
    // ✅ 新增：检查是否需要延迟保存配置到Flash
    if (s_config_dirty && (sys_tick_ms - g_last_config_change_time >= CONFIG_SAVE_DELAY_MS)) {
        // 同步内存中的配置到Flash结构体
        SystemConfig_t *config = Flash_Config_Get();
        config->temperature_high = g_temp_high;
        config->temperature_low = g_temp_low;
        config->humidity_high = g_humid_high;
        config->humidity_low = g_humid_low;
        config->work_mode = g_work_mode;
        
        // 保存到Flash
        Flash_Config_Save();
        
        // 清除脏数据标记
        s_config_dirty = 0;
        
        CONTROL_LOG("[FLASH] Config saved (delayed)\r\n");
    }
    
    // 限流：每500ms执行一次
    if (sys_tick_ms - g_last_control_time < 500) {
        return;
    }
    g_last_control_time = sys_tick_ms;
    
    // 检查DHT22数据有效性
    if (!g_dht22_data_valid) {
        CONTROL_LOG("DHT22 data invalid, skip control\r\n");
        return;
    }
    
    int16_t temp = DHT22_Data.temperature;
    int16_t humid = DHT22_Data.humidity;
    
    // 超范围报警逻辑（自动/手动模式均生效）
    float current_temp = temp / 10.0f;
    float current_humi = humid / 10.0f;
    float temp_high_f = g_temp_high / 10.0f;
    float temp_low_f = g_temp_low / 10.0f;
    float humi_high_f = g_humid_high / 10.0f;
    float humi_low_f = g_humid_low / 10.0f;
    
    float temp_diff = 0;
    if (current_temp > temp_high_f)
        temp_diff = current_temp - temp_high_f;
    else if (current_temp < temp_low_f)
        temp_diff = temp_low_f - current_temp;
    
    float humi_diff = 0;
    if (current_humi > humi_high_f)
        humi_diff = current_humi - humi_high_f;
    else if (current_humi < humi_low_f)
        humi_diff = humi_low_f - current_humi;
    
    if (temp_diff >= 2.0f || humi_diff >= 5.0f) {
        s_beep_count++;
        if (s_beep_count >= 1) {
            s_beep_count = 0;
            s_beep_state = !s_beep_state;
            beep = s_beep_state ? 0 : 1;
        }
    } else {
        beep = 1;
        s_beep_state = 0;
        s_beep_count = 0;
    }
    
    // 仅在自动模式下执行控制
    if (g_work_mode != 1) {
        return;
    }
    
    // 温度控制逻辑（滞回比较防止抖动）
    if (temp > g_temp_high) {
        // 温度过高 → 开启制冷（高电平=开）
        zhileng = 1;  // ✅ 高电平开启
        g_cooler_state = 1;
        jiare = 0;    // ✅ 低电平关闭加热
        g_heater_state = 0;
        CONTROL_LOG("Temp high: %d.%d > %d.%d, cooler ON\r\n",
                   temp / 10, abs(temp % 10),
                   g_temp_high / 10, abs(g_temp_high % 10));
    } else if (temp < g_temp_low) {
        // 温度过低 → 开启加热（高电平=开）
        jiare = 1;    // ✅ 高电平开启
        g_heater_state = 1;
        zhileng = 0;  // ✅ 低电平关闭制冷
        g_cooler_state = 0;
        CONTROL_LOG("Temp low: %d.%d < %d.%d, heater ON\r\n",
                   temp / 10, abs(temp % 10),
                   g_temp_low / 10, abs(g_temp_low % 10));
    } else {
        // 温度正常 → 关闭加热和制冷（低电平=关）
        jiare = 0;    // ✅ 低电平关闭
        zhileng = 0;  // ✅ 低电平关闭
        g_heater_state = 0;
        g_cooler_state = 0;
    }
    
    // 湿度控制逻辑
    if (humid > g_humid_high) {
        // 湿度过高 → 开启除湿（高电平=开）
        chushi = 1;   // ✅ 高电平开启
        g_dehumid_state = 1;
        jiashi = 0;   // ✅ 低电平关闭加湿
        g_humidifier_state = 0;
        CONTROL_LOG("Humid high: %d.%d > %d.%d, dehumid ON\r\n",
                   humid / 10, abs(humid % 10),
                   g_humid_high / 10, abs(g_humid_high % 10));
    } else if (humid < g_humid_low) {
        // 湿度过低 → 开启加湿（高电平=开）
        jiashi = 1;   // ✅ 高电平开启
        g_humidifier_state = 1;
        chushi = 0;   // ✅ 低电平关闭除湿
        g_dehumid_state = 0;
        CONTROL_LOG("Humid low: %d.%d < %d.%d, humidifier ON\r\n",
                   humid / 10, abs(humid % 10),
                   g_humid_low / 10, abs(g_humid_low % 10));
    } else {
        // 湿度正常 → 关闭除湿和加湿（低电平=关）
        chushi = 0;   // ✅ 低电平关闭
        jiashi = 0;   // ✅ 低电平关闭
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
        
        // ✅ 标记配置已修改，延迟保存（防止频繁擦写Flash）
        s_config_dirty = 1;
        g_last_config_change_time = sys_tick_ms;
        
        CONTROL_LOG("Mode set to: %s (pending save)\r\n", mode == 1 ? "Auto" : "Manual");
        
        // ✅ 关键修复：切换到手动模式时，关闭所有执行器（低电平=关）
        if (mode == 2) {
            jiare = 0;      // ✅ 低电平关闭
            zhileng = 0;    // ✅ 低电平关闭
            chushi = 0;     // ✅ 低电平关闭
            jiashi = 0;     // ✅ 低电平关闭
            
            g_heater_state = 0;
            g_cooler_state = 0;
            g_dehumid_state = 0;
            g_humidifier_state = 0;
            
            CONTROL_LOG("[MODE] All actuators OFF in manual mode\r\n");
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
            if (state == 1) {
                // ✅ 开启加热时，强制关闭制冷（互斥）
                jiare = 1;
                zhileng = 0;
                g_heater_state = 1;
                g_cooler_state = 0;
                CONTROL_LOG("[MANUAL] Heater ON, Cooler OFF (mutex)\r\n");
            } else {
                jiare = 0;
                g_heater_state = 0;
                CONTROL_LOG("[MANUAL] Heater OFF\r\n");
            }
            break;
            
        case CTRL_COOLER:
            if (state == 1) {
                // ✅ 开启制冷时，强制关闭加热（互斥）
                zhileng = 1;
                jiare = 0;
                g_cooler_state = 1;
                g_heater_state = 0;
                CONTROL_LOG("[MANUAL] Cooler ON, Heater OFF (mutex)\r\n");
            } else {
                zhileng = 0;
                g_cooler_state = 0;
                CONTROL_LOG("[MANUAL] Cooler OFF\r\n");
            }
            break;
            
        case CTRL_DEHUMID:
            if (state == 1) {
                // ✅ 开启除湿时，强制关闭加湿（互斥）
                chushi = 1;
                jiashi = 0;
                g_dehumid_state = 1;
                g_humidifier_state = 0;
                CONTROL_LOG("[MANUAL] Dehumidifier ON, Humidifier OFF (mutex)\r\n");
            } else {
                chushi = 0;
                g_dehumid_state = 0;
                CONTROL_LOG("[MANUAL] Dehumidifier OFF\r\n");
            }
            break;
            
        case CTRL_HUMIDIFIER:
            if (state == 1) {
                // ✅ 开启加湿时，强制关闭除湿（互斥）
                jiashi = 1;
                chushi = 0;
                g_humidifier_state = 1;
                g_dehumid_state = 0;
                CONTROL_LOG("[MANUAL] Humidifier ON, Dehumidifier OFF (mutex)\r\n");
            } else {
                jiashi = 0;
                g_humidifier_state = 0;
                CONTROL_LOG("[MANUAL] Humidifier OFF\r\n");
            }
            break;
            
        default:
            CONTROL_LOG("[ERR] Invalid actuator: %d\r\n", actuator);
            break;
    }
}

/**
 * @brief 设置温度阈值
 * @param temp_high: 温度上限（放大10倍，如250表示25.0℃）
 * @param temp_low: 温度下限（放大10倍，如200表示20.0℃）
 * @note 范围限制：-40.0℃ ~ 80.0℃（即-400 ~ 800）
 */
void Control_Task_SetTempThreshold(int16_t temp_high, int16_t temp_low) {
    // ✅ 双重保护：校验范围（-400 ~ 800）
    if (temp_high < -400 || temp_high > 800 || temp_low < -400 || temp_low > 800) {
        CONTROL_LOG("[ERR] Temp threshold out of range: high=%d, low=%d\r\n", temp_high, temp_low);
        return;
    }
    
    // 校验逻辑合理性
    if (temp_high > temp_low) {
        g_temp_high = temp_high;
        g_temp_low = temp_low;
        
        // ✅ 标记配置已修改，延迟保存（防止频繁擦写Flash）
        s_config_dirty = 1;
        g_last_config_change_time = sys_tick_ms;
        
        CONTROL_LOG("[OK] Temp threshold set (pending save)\r\n");
    } else {
        CONTROL_LOG("[WARN] Temp high <= low, skip update\r\n");
    }
}

/**
 * @brief 设置湿度阈值
 * @param humid_high: 湿度上限（放大10倍，如650表示65.0%）
 * @param humid_low: 湿度下限（放大10倍，如500表示50.0%）
 * @note 范围限制：0.0% ~ 100.0%（即0 ~ 1000）
 */
void Control_Task_SetHumidThreshold(int16_t humid_high, int16_t humid_low) {
    // ✅ 双重保护：校验范围（0 ~ 1000）
    if (humid_high < 0 || humid_high > 1000 || humid_low < 0 || humid_low > 1000) {
        CONTROL_LOG("[ERR] Humid threshold out of range: high=%d, low=%d\r\n", humid_high, humid_low);
        return;
    }
    
    // 校验逻辑合理性
    if (humid_high > humid_low) {
        g_humid_high = humid_high;
        g_humid_low = humid_low;
        
        // ✅ 标记配置已修改，延迟保存（防止频繁擦写Flash）
        s_config_dirty = 1;
        g_last_config_change_time = sys_tick_ms;
        
        CONTROL_LOG("[OK] Humid threshold set (pending save)\r\n");
    } else {
        CONTROL_LOG("[WARN] Humid high <= low, skip update\r\n");
    }
}
