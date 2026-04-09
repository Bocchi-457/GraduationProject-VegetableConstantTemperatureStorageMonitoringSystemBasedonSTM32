#ifndef __CONTROL_TASK_H__
#define __CONTROL_TASK_H__

#include "stdint.h"

/**
 * 【调试开关】控制任务日志控制
 */
#ifndef DEBUG_CONTROL_TASK
  #define DEBUG_CONTROL_TASK  0  // ✅ 默认关闭，防止日志刷屏
#endif

// 设备类型枚举
typedef enum {
    CTRL_HEATER = 0,     // 加热
    CTRL_COOLER = 1,     // 制冷
    CTRL_DEHUMID = 2,    // 除湿
    CTRL_HUMIDIFIER = 3  // 加湿
} ControlDevice_t;

// 外部可访问的变量
extern uint8_t g_work_mode;
extern int16_t g_temp_high;
extern int16_t g_temp_low;
extern int16_t g_humid_high;
extern int16_t g_humid_low;
extern uint8_t g_heater_state;
extern uint8_t g_cooler_state;
extern uint8_t g_dehumid_state;
extern uint8_t g_humidifier_state;

/**
 * @brief 控制任务初始化
 */
void Control_Task_Init(void);

/**
 * @brief 自动控制逻辑任务（非阻塞，每500ms执行一次）
 */
void Control_Task_Run(void);

/**
 * @brief 设置工作模式
 * @param mode: 1=自动模式, 2=手动模式
 */
void Control_Task_SetMode(uint8_t mode);

/**
 * @brief 手动控制执行器
 * @param actuator: 执行器类型（CTRL_HEATER/CTRL_COOLER等）
 * @param state: 状态（0=关, 1=开）
 */
void Control_Task_ManualControl(ControlDevice_t actuator, uint8_t state);

/**
 * @brief 设置温度阈值
 * @param temp_high: 温度上限（放大10倍）
 * @param temp_low: 温度下限（放大10倍）
 */
void Control_Task_SetTempThreshold(int16_t temp_high, int16_t temp_low);

/**
 * @brief 设置湿度阈值
 * @param humid_high: 湿度上限（放大10倍）
 * @param humid_low: 湿度下限（放大10倍）
 */
void Control_Task_SetHumidThreshold(int16_t humid_high, int16_t humid_low);

#endif // __CONTROL_TASK_H__
