/**
 * @file oled_display_task.c
 * @brief OLED显示任务模块（完整迁移自main_backup.c）
 * @note 包含3个页面：主页面、控制页面、设置页面
 */

#include "oled_display_task.h"
#include "OLED.h"
#include "Key.h"
#include "Timer.h"
#include "DS1302.h"              // ✅ 新增：DS1302 RTC驱动
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

// 页面清除标志
uint8_t g_page_clear_flag = 0;

// ✅ 页面索引（移除static，使key_task可以访问）
uint8_t s_page2_index = 1;     // 控制页面索引
uint8_t s_page3_index = 1;     // 设置页面索引

// ✅ 显示缓冲区（全局变量，照搬旧系统）
char oled_str[100];

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
 * @brief 显示温湿度数据（完全照搬旧系统show_wendu函数）
 */
void Display_ShowWendu(void) {
    // 提取绝对值用于求模计算
    int abs_temp = (DHT22_Data.temperature < 0) ? -DHT22_Data.temperature 
                                                : DHT22_Data.temperature;
    char temp_sign = (DHT22_Data.temperature < 0) ? '-' : ' ';  // 负数显示减号，正数补空格对齐
    
    sprintf(oled_str, "T:%c%d.%dC H:%d.%d%%", temp_sign, abs_temp / 10,
            abs_temp % 10, DHT22_Data.humidity / 10, DHT22_Data.humidity % 10);
    
    OLED_ShowString(0, 6, (u8 *)oled_str, 16);
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
 * @brief 显示时间（直接调用DS1302驱动中的TIME函数）
 */
void Display_ShowTime(void) {
    // ✅ 直接调用DS1302.c中已有的TIME函数，包含日期、时间、星期几的完整显示
    // TIME()使用y=3显示日期和星期，y=4显示时间
    TIME();
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
        
        Display_ShowMode();    // 显示模式（y=0，第0-1页）
        Display_ShowTime();    // 显示时间（TIME函数使用y=3和y=4，第3-5页）
        Display_ShowWendu();   // 显示温湿度（y=6，第6-7页）
        
        // ✅ 删除：按键处理已移至key_task.c，此处只负责显示
    }
    
    // ===== 页面2：控制页面 =====
    else if (current_page == PAGE_CONTROL) {
        if (g_page_clear_flag == 1) {
            g_page_clear_flag = 2;
            OLED_Clear(0);
        }
        
        // ✅ 删除：按键2/3/4处理已移至key_task.c，此处只负责显示
        
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
        
        // ✅ 删除：手动模式下的按键控制已移至key_task.c
        
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
        
        // ✅ 新增：显示缓冲区（支持小数和负数显示）
        static char temp_str[8];
        static char humid_str[8];
        
        // 显示温度上限
        OLED_ShowCHinese(0, 0, 10);   // 温
        OLED_ShowCHinese(16, 0, 12);  // 度
        OLED_ShowCHinese(32, 0, 124); // 上
        OLED_ShowCHinese(48, 0, 125); // 限
        OLED_ShowChar(64, 0, ':', 16);
        
        // ✅ 修改：格式化显示（支持负数和小数，如"25.0"或"-5.0"）
        sprintf(temp_str, "%d.%d", 
                g_temp_high / 10, 
                abs(g_temp_high % 10));
        OLED_ShowString(72, 0, (uint8_t *)temp_str, 16);
        
        // 显示温度下限
        OLED_ShowCHinese(0, 2, 10);   // 温
        OLED_ShowCHinese(16, 2, 12);  // 度
        OLED_ShowCHinese(32, 2, 126); // 下
        OLED_ShowCHinese(48, 2, 127); // 限
        OLED_ShowChar(64, 2, ':', 16);
        
        // ✅ 修改：格式化显示（支持负数和小数）
        sprintf(temp_str, "%d.%d", 
                g_temp_low / 10, 
                abs(g_temp_low % 10));
        OLED_ShowString(72, 2, (uint8_t *)temp_str, 16);
        
        // 显示湿度上限
        OLED_ShowCHinese(0, 4, 11);   // 湿
        OLED_ShowCHinese(16, 4, 12);  // 度
        OLED_ShowCHinese(32, 4, 124); // 上
        OLED_ShowCHinese(48, 4, 125); // 限
        OLED_ShowChar(64, 4, ':', 16);
        
        // ✅ 修改：格式化显示（湿度始终为正数）
        sprintf(humid_str, "%d.%d", 
                g_humid_high / 10, 
                g_humid_high % 10);
        OLED_ShowString(72, 4, (uint8_t *)humid_str, 16);
        
        // 显示湿度下限
        OLED_ShowCHinese(0, 6, 11);   // 湿
        OLED_ShowCHinese(16, 6, 12);  // 度
        OLED_ShowCHinese(32, 6, 126); // 下
        OLED_ShowCHinese(48, 6, 127); // 限
        OLED_ShowChar(64, 6, ':', 16);
        
        // ✅ 修改：格式化显示
        sprintf(humid_str, "%d.%d", 
                g_humid_low / 10, 
                g_humid_low % 10);
        OLED_ShowString(72, 6, (uint8_t *)humid_str, 16);
        
        // ✅ 优化：光标位置指示右移，避免遮挡负号（从x=100移到x=116）
        if (s_page3_index == 1) {  // 温度上限
            OLED_ShowString(116, 0, (uint8_t *)"<", 16);
            OLED_ShowString(116, 2, (uint8_t *)" ", 16);
            OLED_ShowString(116, 4, (uint8_t *)" ", 16);
            OLED_ShowString(116, 6, (uint8_t *)" ", 16);
        } else if (s_page3_index == 2) {  // 温度下限
            OLED_ShowString(116, 0, (uint8_t *)" ", 16);
            OLED_ShowString(116, 2, (uint8_t *)"<", 16);
            OLED_ShowString(116, 4, (uint8_t *)" ", 16);
            OLED_ShowString(116, 6, (uint8_t *)" ", 16);
        } else if (s_page3_index == 3) {  // 湿度上限
            OLED_ShowString(116, 0, (uint8_t *)" ", 16);
            OLED_ShowString(116, 2, (uint8_t *)" ", 16);
            OLED_ShowString(116, 4, (uint8_t *)"<", 16);
            OLED_ShowString(116, 6, (uint8_t *)" ", 16);
        } else if (s_page3_index == 4) {  // 湿度下限
            OLED_ShowString(116, 0, (uint8_t *)" ", 16);
            OLED_ShowString(116, 2, (uint8_t *)" ", 16);
            OLED_ShowString(116, 4, (uint8_t *)" ", 16);
            OLED_ShowString(116, 6, (uint8_t *)"<", 16);
        }
    }
}
