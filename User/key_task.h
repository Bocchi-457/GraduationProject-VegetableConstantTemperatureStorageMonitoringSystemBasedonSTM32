/**
 * @file key_task.h
 * @brief 按键任务模块头文件
 * @note 处理按键扫描和页面切换
 */

#ifndef __KEY_TASK_H__
#define __KEY_TASK_H__

#include "stdint.h"

/**
 * 【调试开关】按键任务日志控制
 */
#ifndef DEBUG_KEY_TASK
  #define DEBUG_KEY_TASK  0  // 默认关闭，节省Flash
#endif

// 页面枚举
typedef enum {
    PAGE_MAIN = 1,      // 主页面（温湿度+时间）
    PAGE_CONTROL = 2,   // 控制页面（手动控制执行器）
    PAGE_SETTING = 3    // 设置页面（阈值设置）
} Page_t;

// 外部可访问的变量
extern Page_t g_current_page;
extern uint8_t g_key_num;  // 按键返回值

/**
 * @brief 按键任务初始化
 */
void Key_Task_Init(void);

/**
 * @brief 按键扫描任务（非阻塞，每50ms扫描一次）
 * @note 此函数被调度器调用，返回值为void
 */
void Key_Task_Scan(void);

/**
 * @brief 获取当前页面
 * @return 当前页面枚举值
 */
Page_t Key_Task_GetPage(void);

/**
 * @brief 切换页面
 * @param page: 目标页面
 */
void Key_Task_SetPage(Page_t page);

#endif // __KEY_TASK_H__
