/**
 * @file key_task.c
 * @brief 按键任务模块实现
 * @note 处理按键扫描和页面切换，每50ms扫描一次
 */

#include "key_task.h"
#include "Key.h"
#include "Timer.h"
#include "control_task.h"     // 访问Control_Task_SetMode等函数
#include "oled_display_task.h"  // ✅ 访问s_page2_index/s_page3_index/g_temp_high等
#include <stdio.h>

#if DEBUG_KEY_TASK
  #include "Usart.h"
  #define KEY_LOG(fmt, ...) Serial_Printf("[KEY] " fmt, ##__VA_ARGS__)
#else
  #define KEY_LOG(fmt, ...)
#endif

// 当前页面
Page_t g_current_page = PAGE_MAIN;

// 上次扫描时间
static uint32_t g_last_scan_time = 0;

// 按键返回值
uint8_t g_key_num = 0;

/**
 * @brief 按键任务初始化
 */
void Key_Task_Init(void) {
    g_current_page = PAGE_MAIN;
    g_last_scan_time = sys_tick_ms;
    g_key_num = 0;
    
    KEY_LOG("Key task initialized\r\n");
}

/**
 * @brief 按键扫描任务（非阻塞）
 * @note 此函数被调度器调用，返回值为void
 */
void Key_Task_Scan(void) {
    // 限流：每50ms扫描一次
    if (sys_tick_ms - g_last_scan_time < 50) {
        return;
    }
    g_last_scan_time = sys_tick_ms;
    
    // 扫描按键
    g_key_num = KEY_Scan(0);
    
    if (g_key_num > 0) {
        KEY_LOG("Key pressed: %d, Page: %d\r\n", g_key_num, g_current_page);
        
        // ✅ 根据当前页面处理不同按键
        switch (g_current_page) {
            case PAGE_MAIN:  // 页面1：主页
                if (g_key_num == 1) {
                    // 切换到下一页
                    g_current_page = PAGE_CONTROL;
                    KEY_LOG("Page switched to: CONTROL\r\n");
                } else if (g_key_num == 3) {
                    // 切换到自动模式
                    Control_Task_SetMode(1);
                    KEY_LOG("Mode set to: AUTO\r\n");
                } else if (g_key_num == 4) {
                    // 切换到手动模式
                    Control_Task_SetMode(2);
                    KEY_LOG("Mode set to: MANUAL\r\n");
                }
                break;
                
            case PAGE_CONTROL:  // 页面2：控制页
                if (g_key_num == 1) {
                    // 切换到下一页
                    g_current_page = PAGE_SETTING;
                    KEY_LOG("Page switched to: SETTING\r\n");
                } else if (g_key_num == 2) {
                    // 切换控制项索引（1-加热 2-制冷 3-除湿 4-加湿）
                    s_page2_index++;
                    if (s_page2_index > 4) s_page2_index = 1;
                    KEY_LOG("Control item index: %d\r\n", s_page2_index);
                } else if (g_key_num == 3) {
                    // 开启当前选中的设备
                    ControlDevice_t device;
                    switch (s_page2_index) {
                        case 1: device = CTRL_HEATER; break;
                        case 2: device = CTRL_COOLER; break;
                        case 3: device = CTRL_DEHUMID; break;
                        case 4: device = CTRL_HUMIDIFIER; break;
                        default: device = CTRL_HEATER; break;
                    }
                    Control_Task_ManualControl(device, 1);  // 开启
                    KEY_LOG("Device ON: %d\r\n", device);
                } else if (g_key_num == 4) {
                    // 关闭当前选中的设备
                    ControlDevice_t device;
                    switch (s_page2_index) {
                        case 1: device = CTRL_HEATER; break;
                        case 2: device = CTRL_COOLER; break;
                        case 3: device = CTRL_DEHUMID; break;
                        case 4: device = CTRL_HUMIDIFIER; break;
                        default: device = CTRL_HEATER; break;
                    }
                    Control_Task_ManualControl(device, 0);  // 关闭
                    KEY_LOG("Device OFF: %d\r\n", device);
                }
                break;
                
            case PAGE_SETTING:  // 页面3：设置页
                if (g_key_num == 1) {
                    // 切换到主页
                    g_current_page = PAGE_MAIN;
                    KEY_LOG("Page switched to: MAIN\r\n");
                } else if (g_key_num == 2) {
                    // 切换设置项索引（1-温度上限 2-温度下限 3-湿度上限 4-湿度下限）
                    s_page3_index++;
                    if (s_page3_index > 4) s_page3_index = 1;
                    KEY_LOG("Setting item index: %d\r\n", s_page3_index);
                } else if (g_key_num == 3) {
                    // 增加当前选中项的数值
                    switch (s_page3_index) {
                        case 1:  // 温度上限 +1℃
                            if (g_temp_high < 800) {  // 最大80.0℃
                                Control_Task_SetTempThreshold(g_temp_high + 10, g_temp_low);
                            }
                            break;
                        case 2:  // 温度下限 +1℃
                            if (g_temp_low < 800) {
                                Control_Task_SetTempThreshold(g_temp_high, g_temp_low + 10);
                            }
                            break;
                        case 3:  // 湿度上限 +1%
                            if (g_humid_high < 1000) {  // 最大100.0%
                                Control_Task_SetHumidThreshold(g_humid_high + 10, g_humid_low);
                            }
                            break;
                        case 4:  // 湿度下限 +1%
                            if (g_humid_low < 1000) {
                                Control_Task_SetHumidThreshold(g_humid_high, g_humid_low + 10);
                            }
                            break;
                    }
                    KEY_LOG("Value increased, index: %d\r\n", s_page3_index);
                } else if (g_key_num == 4) {
                    // 减少当前选中项的数值
                    switch (s_page3_index) {
                        case 1:  // 温度上限 -1℃
                            if (g_temp_high > -400) {  // 最小-40.0℃
                                Control_Task_SetTempThreshold(g_temp_high - 10, g_temp_low);
                            }
                            break;
                        case 2:  // 温度下限 -1℃
                            if (g_temp_low > -400) {
                                Control_Task_SetTempThreshold(g_temp_high, g_temp_low - 10);
                            }
                            break;
                        case 3:  // 湿度上限 -1%
                            if (g_humid_high > 0) {  // 最小0.0%
                                Control_Task_SetHumidThreshold(g_humid_high - 10, g_humid_low);
                            }
                            break;
                        case 4:  // 湿度下限 -1%
                            if (g_humid_low > 0) {
                                Control_Task_SetHumidThreshold(g_humid_high, g_humid_low - 10);
                            }
                            break;
                    }
                    KEY_LOG("Value decreased, index: %d\r\n", s_page3_index);
                }
                break;
                
            default:
                break;
        }
    }
}

/**
 * @brief 获取当前页面
 * @return 当前页面枚举值
 */
Page_t Key_Task_GetPage(void) {
    return g_current_page;
}

/**
 * @brief 切换页面
 * @param page: 目标页面
 */
void Key_Task_SetPage(Page_t page) {
    if (page >= PAGE_MAIN && page <= PAGE_SETTING) {
        g_current_page = page;
        KEY_LOG("Page set to: %d\r\n", page);
    }
}