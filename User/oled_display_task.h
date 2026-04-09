/**
 * @file oled_display_task.h
 * @brief OLED显示任务模块（完整迁移自main_backup.c）
 * @note 包含3个页面：主页面、控制页面、设置页面
 */

#ifndef __OLED_DISPLAY_TASK_H__
#define __OLED_DISPLAY_TASK_H__

#include "stdint.h"

// 外部可访问的变量
extern uint8_t g_page_clear_flag;  // 页面清除标志

/**
 * @brief OLED显示任务初始化
 */
void OLED_Display_Task_Init(void);

/**
 * @brief OLED显示任务（非阻塞，每100ms执行一次）
 * @note 根据当前页面显示不同内容
 */
void OLED_Display_Task_Run(void);

/**
 * @brief 显示温湿度数据
 */
void Display_ShowWendu(void);

/**
 * @brief 显示当前模式
 */
void Display_ShowMode(void);

/**
 * @brief 显示时间
 */
void Display_ShowTime(void);

#endif // __OLED_DISPLAY_TASK_H__
