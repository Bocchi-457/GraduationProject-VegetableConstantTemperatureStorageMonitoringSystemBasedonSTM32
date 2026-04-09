/**
 * scheduler.c
 * 轻量级协作式调度器实现
 * 功能：基于时间戳轮询的任务调度，确保主循环流畅运行
 * 版本：V1.0
 */

#include "scheduler.h"
#include "Timer.h"  // sys_tick_ms
#include <string.h>

/**
 * 任务列表
 */
static Task_t tasks[MAX_TASKS];

/**
 * 已注册任务数量
 */
static uint8_t task_count = 0;

/**
 * @brief 初始化调度器
 */
void Scheduler_Init(void) {
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
}

/**
 * @brief 注册任务到调度器
 */
uint8_t Scheduler_Register(TaskFunc_t func, uint32_t interval) {
    // 参数校验
    if (func == NULL || interval == 0) {
        return 0xFF;
    }
    
    // 检查任务列表是否已满
    if (task_count >= MAX_TASKS) {
        return 0xFF;
    }
    
    // 注册新任务
    uint8_t task_id = task_count;
    tasks[task_id].func = func;
    tasks[task_id].interval = interval;
    tasks[task_id].last_run = sys_tick_ms;  // 初始化时间为当前时间
    tasks[task_id].enabled = 1;
    
    task_count++;
    
    return task_id;
}

/**
 * @brief 注销任务
 */
void Scheduler_Unregister(uint8_t task_id) {
    if (task_id >= MAX_TASKS) {  // 改为检查最大容量
        return;
    }
    if (tasks[task_id].func == NULL) {
        return;  // 任务已注销或从未注册
    }
    
    // 标记为禁用
    tasks[task_id].enabled = 0;
    tasks[task_id].func = NULL;
}

/**
 * @brief 暂停任务执行
 */
void Scheduler_Suspend(uint8_t task_id) {
    if (task_id < task_count) {
        tasks[task_id].enabled = 0;
    }
}

/**
 * @brief 恢复任务执行
 */
void Scheduler_Resume(uint8_t task_id) {
    if (task_id < task_count) {
        tasks[task_id].enabled = 1;
        tasks[task_id].last_run = sys_tick_ms;  // 重置计时，立即执行
    }
}

/**
 * @brief 调度器主循环
 */
void Scheduler_Run(void) {
    uint32_t current_time = sys_tick_ms;
    uint32_t loop_start_time = current_time;
    
    // 遍历所有任务
    for (uint8_t i = 0; i < task_count; i++) {
        // 跳过未启用的任务
        if (!tasks[i].enabled || tasks[i].func == NULL) {
            continue;
        }
        
        // 检查是否到达执行时间
        if (current_time - tasks[i].last_run >= tasks[i].interval) {
            // 执行任务
            tasks[i].func();
            
            // 更新上次执行时间
            tasks[i].last_run = current_time;
            
            // 防止单次调度时间过长（超过20ms则退出）
            if (sys_tick_ms - loop_start_time > 20) {
                break;
            }
        }
    }
}
