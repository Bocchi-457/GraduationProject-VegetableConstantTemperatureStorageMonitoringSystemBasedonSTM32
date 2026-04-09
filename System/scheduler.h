#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include <stdint.h>

/**
 * 最大任务数量
 */
#define MAX_TASKS 10

/**
 * 任务函数指针类型
 */
typedef void (*TaskFunc_t)(void);

/**
 * 任务控制块
 */
typedef struct {
    TaskFunc_t func;        // 任务函数指针
    uint32_t interval;      // 执行间隔（ms）
    uint32_t last_run;      // 上次执行时间戳
    uint8_t enabled;        // 是否启用（1=启用，0=禁用）
} Task_t;

/**
 * @brief 初始化调度器
 * @note 必须在系统初始化后、注册任务前调用
 */
void Scheduler_Init(void);

/**
 * @brief 注册任务到调度器
 * @param func: 任务函数指针
 * @param interval: 执行间隔（毫秒）
 * @return 任务ID（0-9），失败返回0xFF
 * @note 任务函数必须是非阻塞的，执行时间应尽可能短
 */
uint8_t Scheduler_Register(TaskFunc_t func, uint32_t interval);

/**
 * @brief 注销任务
 * @param task_id: 任务ID（由Scheduler_Register返回）
 */
void Scheduler_Unregister(uint8_t task_id);

/**
 * @brief 暂停任务执行
 * @param task_id: 任务ID
 */
void Scheduler_Suspend(uint8_t task_id);

/**
 * @brief 恢复任务执行
 * @param task_id: 任务ID
 */
void Scheduler_Resume(uint8_t task_id);

/**
 * @brief 调度器主循环（需在while(1)中周期性调用）
 * @note 每次调用会检查所有已注册任务，执行到期的任务
 *       单次调度循环最大执行时间不超过20ms
 */
void Scheduler_Run(void);

#endif
