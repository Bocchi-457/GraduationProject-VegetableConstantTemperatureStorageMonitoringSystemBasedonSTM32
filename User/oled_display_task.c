/**
 * @file oled_display_task.c
 * @brief OLED显示任务模块（完整迁移自main_backup.c）
 * @note 包含3个页面：主页面、控制页面、设置页面
 */

#include "oled_display_task.h"
#include "OLED.h"
#include "Key.h"
#include "Timer.h"
#include "dht22.h"
#include "control.h"
#include "control_task.h"
#include "key_task.h"
#include <stdio.h>
#include <stdlib.h>

// 外部变量声明（从main_backup.c迁移）
extern uint8_t g_work_mode;           // 工作模式（1=自动，2=手动）
extern int16_t g_temp_high;           // 温度上限
extern int16_t g_temp_low;            // 温度下限
extern int16_t g_humid_high;          // 湿度上限
extern int16_t g_humid_low;           // 湿度下限
extern uint8_t g_heater_state;        // 加热状态
extern uint8_t g_cooler_state;        // 制冷状态
extern uint8_t g_dehumid_state;       // 除湿状态
extern uint8_t g_humidifier_state;    // 加湿状态
extern uint8_t g_key_num;             // ✅ 按键返回值（来自key_task模块）

// 页面清除标志
uint8_t g_page_clear_flag = 0;

// 页面索引（用于控制页面和设置页面的光标位置）
static uint8_t s_page2_index = 1;     // 控制页面索引
static uint8_t s_page3_index = 1;     // 设置页面索引

// 显示缓冲区
static char s_oled_str[100];

// 上次执行时间
static uint32_t s_last_display_time = 0;

/**
 * @brief OLED显示任务初始化
 */
void OLED_Display_Task_Init(void) {
    g_page_clear_flag = 0;
    s_page2_index = 1;
    s_page3_index = 1;
    s_last_display_time = sys_tick_ms;
}

/**
 * @brief 显示温湿度数据
 */
void Display_ShowWendu(void) {
    // 提取绝对值用于求模计算
    int abs_temp = (DHT22_Data.temperature < 0) ? -DHT22_Data.temperature : DHT22_Data.temperature;
    char temp_sign = (DHT22_Data.temperature < 0) ? '-' : ' ';  // 负数显示减号，正数补空格对齐
    
    sprintf(s_oled_str, "T:%c%d.%dC H:%d.%d%%", 
            temp_sign, 
            abs_temp / 10, abs_temp % 10, 
            DHT22_Data.humidity / 10, DHT22_Data.humidity % 10);
    
    OLED_ShowString(0, 6, (uint8_t *)s_oled_str, 16);
}

/**
 * @brief 显示当前模式
 */
void Display_ShowMode(void) {
    if (g_work_mode == 1) {  // 自动模式
        OLED_ShowCHinese(0, 0, 31);   // 自
        OLED_ShowCHinese(16, 0, 33);  // 动
        OLED_ShowCHinese(32, 0, 108); // 模
        OLED_ShowCHinese(48, 0, 109); // 式
    } else if (g_work_mode == 2) {  // 手动模式
        OLED_ShowCHinese(0, 0, 32);   // 手
        OLED_ShowCHinese(16, 0, 33);  // 动
        OLED_ShowCHinese(32, 0, 108); // 模
        OLED_ShowCHinese(48, 0, 109); // 式
    }
}

/**
 * @brief 显示时间（简化版，实际需要RTC支持）
 * @note TODO: 需要集成RTC或网络时间同步
 */
void Display_ShowTime(void) {
    // 简化显示：显示固定时间格式
    // TODO: 后续集成RTC_Get()获取真实时间
    OLED_ShowString(0, 4, (uint8_t *)"2024-01-01", 16);
    OLED_ShowString(0, 5, (uint8_t *)"00:00:00", 16);
}

/**
 * @brief OLED显示任务（非阻塞，每100ms执行一次）
 */
void OLED_Display_Task_Run(void) {
    // 限流：每100ms执行一次
    if (sys_tick_ms - s_last_display_time < 100) {
        return;
    }
    s_last_display_time = sys_tick_ms;
    
    Page_t current_page = Key_Task_GetPage();
    uint8_t key_num = g_key_num;  // ✅ 直接读取全局按键变量
    
    // 页面切换时清屏
    static Page_t last_page = PAGE_MAIN;
    if (current_page != last_page) {
        OLED_Clear(0);
        g_page_clear_flag = 0;
        last_page = current_page;
    }
    
    // ===== 页面1：主页面 =====
    if (current_page == PAGE_MAIN) {
        if (g_page_clear_flag == 0) {
            g_page_clear_flag = 1;
            OLED_Clear(0);
        }
        
        Display_ShowMode();    // 显示模式（第0行）
        Display_ShowTime();    // 显示时间（第4-5行）
        Display_ShowWendu();   // 显示温湿度（第6行）
        
        // 按键处理：切换模式
        if (key_num == 4) {  // 切换到手动模式
            Control_Task_SetMode(2);
        }
        if (key_num == 3) {  // 切换到自动模式
            Control_Task_SetMode(1);
        }
    }
    
    // ===== 页面2：控制页面 =====
    else if (current_page == PAGE_CONTROL) {
        if (g_page_clear_flag == 1) {
            g_page_clear_flag = 2;
            OLED_Clear(0);
        }
        
        // 按键2：切换控制项
        if (key_num == 2) {
            s_page2_index++;
            if (s_page2_index > 4) s_page2_index = 1;
        }
        
        // 显示加热控制
        OLED_ShowCHinese(0, 0, 116);  // 加
        OLED_ShowCHinese(16, 0, 117); // 热
        OLED_ShowString(32, 0, (uint8_t *)":", 16);
        
        // 显示制冷控制
        OLED_ShowCHinese(0, 2, 118);  // 制
        OLED_ShowCHinese(16, 2, 119); // 冷
        OLED_ShowString(32, 2, (uint8_t *)":", 16);
        
        // 显示除湿控制
        OLED_ShowCHinese(0, 4, 120);  // 除
        OLED_ShowCHinese(16, 4, 121); // 湿
        OLED_ShowString(32, 4, (uint8_t *)":", 16);
        
        // 显示加湿控制
        OLED_ShowCHinese(0, 6, 116);  // 加
        OLED_ShowCHinese(16, 6, 121); // 湿
        OLED_ShowString(32, 6, (uint8_t *)":", 16);
        
        // 光标位置指示
        if (s_page2_index == 1) {
            OLED_ShowString(60, 0, (uint8_t *)"<", 16);
            OLED_ShowString(60, 2, (uint8_t *)" ", 16);
            OLED_ShowString(60, 4, (uint8_t *)" ", 16);
            OLED_ShowString(60, 6, (uint8_t *)" ", 16);
        } else if (s_page2_index == 2) {
            OLED_ShowString(60, 0, (uint8_t *)" ", 16);
            OLED_ShowString(60, 2, (uint8_t *)"<", 16);
            OLED_ShowString(60, 4, (uint8_t *)" ", 16);
            OLED_ShowString(60, 6, (uint8_t *)" ", 16);
        } else if (s_page2_index == 3) {
            OLED_ShowString(60, 0, (uint8_t *)" ", 16);
            OLED_ShowString(60, 2, (uint8_t *)" ", 16);
            OLED_ShowString(60, 4, (uint8_t *)"<", 16);
            OLED_ShowString(60, 6, (uint8_t *)" ", 16);
        } else if (s_page2_index == 4) {
            OLED_ShowString(60, 0, (uint8_t *)" ", 16);
            OLED_ShowString(60, 2, (uint8_t *)" ", 16);
            OLED_ShowString(60, 4, (uint8_t *)" ", 16);
            OLED_ShowString(60, 6, (uint8_t *)"<", 16);
        }
        
        // 手动模式下的按键控制
        if (g_work_mode == 2) {
            // 加热控制
            if (s_page2_index == 1) {
                if (key_num == 3) {  // 开启加热
                    Control_Task_ManualControl(CTRL_HEATER, 1);
                    Control_Task_ManualControl(CTRL_COOLER, 0);
                } else if (key_num == 4) {  // 关闭加热
                    Control_Task_ManualControl(CTRL_HEATER, 0);
                }
            }
            // 制冷控制
            else if (s_page2_index == 2) {
                if (key_num == 3) {  // 开启制冷
                    Control_Task_ManualControl(CTRL_COOLER, 1);
                    Control_Task_ManualControl(CTRL_HEATER, 0);
                } else if (key_num == 4) {  // 关闭制冷
                    Control_Task_ManualControl(CTRL_COOLER, 0);
                }
            }
            // 除湿控制
            else if (s_page2_index == 3) {
                if (key_num == 3) {  // 开启除湿
                    Control_Task_ManualControl(CTRL_DEHUMID, 1);
                    Control_Task_ManualControl(CTRL_HUMIDIFIER, 0);
                } else if (key_num == 4) {  // 关闭除湿
                    Control_Task_ManualControl(CTRL_DEHUMID, 0);
                }
            }
            // 加湿控制
            else if (s_page2_index == 4) {
                if (key_num == 3) {  // 开启加湿
                    Control_Task_ManualControl(CTRL_HUMIDIFIER, 1);
                    Control_Task_ManualControl(CTRL_DEHUMID, 0);
                } else if (key_num == 4) {  // 关闭加湿
                    Control_Task_ManualControl(CTRL_HUMIDIFIER, 0);
                }
            }
        }
        
        // 显示执行器状态
        if (g_heater_state) {
            OLED_ShowCHinese(40, 0, 122);  // 开
        } else {
            OLED_ShowCHinese(40, 0, 123);  // 关
        }
        
        if (g_cooler_state) {
            OLED_ShowCHinese(40, 2, 122);  // 开
        } else {
            OLED_ShowCHinese(40, 2, 123);  // 关
        }
        
        if (g_dehumid_state) {
            OLED_ShowCHinese(40, 4, 122);  // 开
        } else {
            OLED_ShowCHinese(40, 4, 123);  // 关
        }
        
        if (g_humidifier_state) {
            OLED_ShowCHinese(40, 6, 122);  // 开
        } else {
            OLED_ShowCHinese(40, 6, 123);  // 关
        }
    }
    
    // ===== 页面3：设置页面 =====
    else if (current_page == PAGE_SETTING) {
        if (g_page_clear_flag == 2) {
            g_page_clear_flag = 1;
            OLED_Clear(0);
        }
        
        // 按键2：切换设置项
        if (key_num == 2) {
            s_page3_index++;
            if (s_page3_index > 4) s_page3_index = 1;
        }
        
        // 显示温度上限
        OLED_ShowCHinese(0, 0, 10);   // 温
        OLED_ShowCHinese(16, 0, 12);  // 度
        OLED_ShowCHinese(32, 0, 124); // 上
        OLED_ShowCHinese(48, 0, 125); // 限
        OLED_ShowChar(64, 0, ':', 16);
        OLED_ShowNum(72, 0, g_temp_high, 2, 16);
        
        // 显示温度下限
        OLED_ShowCHinese(0, 2, 10);   // 温
        OLED_ShowCHinese(16, 2, 12);  // 度
        OLED_ShowCHinese(32, 2, 126); // 下
        OLED_ShowCHinese(48, 2, 127); // 限
        OLED_ShowChar(64, 2, ':', 16);
        OLED_ShowNum(72, 2, g_temp_low, 2, 16);
        
        // 显示湿度上限
        OLED_ShowCHinese(0, 4, 11);   // 湿
        OLED_ShowCHinese(16, 4, 12);  // 度
        OLED_ShowCHinese(32, 4, 124); // 上
        OLED_ShowCHinese(48, 4, 125); // 限
        OLED_ShowChar(64, 4, ':', 16);
        OLED_ShowNum(72, 4, g_humid_high, 2, 16);
        
        // 显示湿度下限
        OLED_ShowCHinese(0, 6, 11);   // 湿
        OLED_ShowCHinese(16, 6, 12);  // 度
        OLED_ShowCHinese(32, 6, 126); // 下
        OLED_ShowCHinese(48, 6, 127); // 限
        OLED_ShowChar(64, 6, ':', 16);
        OLED_ShowNum(72, 6, g_humid_low, 2, 16);
        
        // 光标位置指示和参数调整
        if (s_page3_index == 1) {  // 温度上限
            OLED_ShowString(100, 0, (uint8_t *)"<", 16);
            OLED_ShowString(100, 2, (uint8_t *)" ", 16);
            OLED_ShowString(100, 4, (uint8_t *)" ", 16);
            OLED_ShowString(100, 6, (uint8_t *)" ", 16);
            
            if (key_num == 3) {  // 增加
                Control_Task_SetTempThreshold(g_temp_high + 1, g_temp_low);
            } else if (key_num == 4) {  // 减少
                if (g_temp_high > g_temp_low + 1) {
                    Control_Task_SetTempThreshold(g_temp_high - 1, g_temp_low);
                }
            }
        } else if (s_page3_index == 2) {  // 温度下限
            OLED_ShowString(100, 0, (uint8_t *)" ", 16);
            OLED_ShowString(100, 2, (uint8_t *)"<", 16);
            OLED_ShowString(100, 4, (uint8_t *)" ", 16);
            OLED_ShowString(100, 6, (uint8_t *)" ", 16);
            
            if (key_num == 3) {  // 增加
                if (g_temp_low < g_temp_high - 1) {
                    Control_Task_SetTempThreshold(g_temp_high, g_temp_low + 1);
                }
            } else if (key_num == 4) {  // 减少
                Control_Task_SetTempThreshold(g_temp_high, g_temp_low - 1);
            }
        } else if (s_page3_index == 3) {  // 湿度上限
            OLED_ShowString(100, 0, (uint8_t *)" ", 16);
            OLED_ShowString(100, 2, (uint8_t *)" ", 16);
            OLED_ShowString(100, 4, (uint8_t *)"<", 16);
            OLED_ShowString(100, 6, (uint8_t *)" ", 16);
            
            if (key_num == 3) {  // 增加
                Control_Task_SetHumidThreshold(g_humid_high + 1, g_humid_low);
            } else if (key_num == 4) {  // 减少
                if (g_humid_high > g_humid_low + 1) {
                    Control_Task_SetHumidThreshold(g_humid_high - 1, g_humid_low);
                }
            }
        } else if (s_page3_index == 4) {  // 湿度下限
            OLED_ShowString(100, 0, (uint8_t *)" ", 16);
            OLED_ShowString(100, 2, (uint8_t *)" ", 16);
            OLED_ShowString(100, 4, (uint8_t *)" ", 16);
            OLED_ShowString(100, 6, (uint8_t *)"<", 16);
            
            if (key_num == 3) {  // 增加
                if (g_humid_low < g_humid_high - 1) {
                    Control_Task_SetHumidThreshold(g_humid_high, g_humid_low + 1);
                }
            } else if (key_num == 4) {  // 减少
                Control_Task_SetHumidThreshold(g_humid_high, g_humid_low - 1);
            }
        }
    }
}
