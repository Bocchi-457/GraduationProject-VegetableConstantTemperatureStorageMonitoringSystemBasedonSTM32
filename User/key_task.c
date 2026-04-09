/**
 * @file key_task.c
 * @brief 按键任务模块实现
 * @note 处理按键扫描和页面切换，每50ms扫描一次
 */

#include "key_task.h"
#include "Key.h"
#include "Timer.h"
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
        KEY_LOG("Key pressed: %d\r\n", g_key_num);
        
        // 页面切换逻辑
        if (g_key_num == 1) {
            // 按键1：切换到下一页
            if (g_current_page < PAGE_SETTING) {
                g_current_page = (Page_t)(g_current_page + 1);
            } else {
                g_current_page = PAGE_MAIN;
            }
            KEY_LOG("Page switched to: %d\r\n", g_current_page);
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
