/**
 * @file main_new.c
 * @brief 蔬菜恒温库监控系统 - 重构版主程序
 * @note 采用调度器架构，彻底消除阻塞延时
 * @version V2.0 (重构版)
 */

#include "stm32f10x.h"
#include "scheduler.h"
#include "network_manager.h"
#include "dht22_task.h"
#include "control_task.h"
#include "key_task.h"              // ✅ 添加按键任务
#include "oled_display_task.h"     // ✅ 添加OLED显示任务
#include "dht22.h"                 // DHT22数据结构定义
#include "control.h"               // 执行器GPIO定义（jiare/zhileng等）
#include "OLED.h"
#include "Key.h"
#include "Timer.h"
#include "Usart.h"
#include "delay.h"
#include <stdio.h>
#include <stdlib.h>                // abs()函数

// C标准库
#include <string.h>

    /**
     * 全局变量声明
     */
    // extern DHT22_Data_TypeDef DHT22_Data;   // 已通过#include
    // "dht22.h"包含，无需在此重复extern声明
    extern uint8_t
        g_dht22_data_valid; // DHT22数据有效性（在dht22_task.c中定义）

/***
 * 函数声明
 */
void System_Init(void);           // 系统初始化
// void OLED_Task_Run(void);      // ❌ 已删除：已被OLED_Display_Task_Run替代
void Network_Upload_Task(void);   // 网络上传任务
void Handle_Cloud_Command(void);  // 处理云端指令

/**
 * @brief 系统初始化
 */
void System_Init(void) {
    // 设置中断优先级分组
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    // 初始化各模块
    Serial_Iint(115200);  // 串口初始化（调试日志）
    Serial_Printf("\r\n=== Vegetable Warehouse Monitoring System V2.0 ===\r\n");
    
    beep_init();          // 蜂鸣器初始化
    Key_Init();           // 按键初始化
    OLED_Init();          // OLED显示初始化
    OLED_Clear(0);        // 清屏
    
    Timer_Init();         // 定时器初始化（TIM2: 10ms心跳）
    
    // 上电延时，确保DHT22稳定（2秒）
    OLED_ShowCHinese(0, 3, 19);  // 系
    OLED_ShowCHinese(18, 3, 20); // 统
    OLED_ShowCHinese(36, 3, 21); // 正
    OLED_ShowCHinese(54, 3, 22); // 在
    OLED_ShowCHinese(72, 3, 0);  // 初
    OLED_ShowCHinese(90, 3, 1);  // 始
    OLED_ShowCHinese(108, 3, 2); // 化
    delay_ms(2000);
    
    // 初始化DHT22任务
    DHT22_Task_Init();
    
    // 初始化控制任务
    Control_Task_Init();
    
    // ✅ 初始化按键任务
    Key_Task_Init();
    
    // ✅ 初始化OLED显示任务
    OLED_Display_Task_Init();
    
    // 初始化网络管理模块
    Network_Manager_Init();
    
    Serial_Printf("System initialization completed\r\n");
}

/**
 * @brief 网络数据上传任务（非阻塞，每2秒上传一次）
 */
void Network_Upload_Task(void) {
    static uint32_t last_upload = 0;
    
    // 限流：每2秒上传一次
    if (sys_tick_ms - last_upload < 2000) {
        return;
    }
    last_upload = sys_tick_ms;
    
    // 仅在联网模式下上传
    if (Network_GetState() != NET_CONNECTED) {
        return;
    }
    
    // 检查DHT22数据有效性
    if (!g_dht22_data_valid) {
        return;
    }
    
    // 上传传感器数据
    Network_UploadSensorData(
        DHT22_Data.temperature,      // 温度（放大10倍）
        DHT22_Data.humidity,         // 湿度（放大10倍）
        g_work_mode,                 // 工作模式
        g_temp_high, g_temp_low,     // 温度阈值
        g_humid_high, g_humid_low,   // 湿度阈值
        g_heater_state,              // 加热状态
        g_cooler_state,              // 制冷状态
        g_dehumid_state,             // 除湿状态
        g_humidifier_state           // 加湿状态
    );
}

/**
 * @brief 处理云端指令
 */
void Handle_Cloud_Command(void) {
    CloudCommand_t cmd;
    
    if (Network_ProcessCloudCommand(&cmd) != 0) {
        return;  // 无指令
    }
    
    // 解析并执行指令
    switch (cmd.type) {
        case CMD_AUTO_MODE:
            if (cmd.value == 1) {
                Control_Task_SetMode(1);
            }
            break;
            
        case CMD_MANUAL_MODE:
            if (cmd.value == 1) {
                Control_Task_SetMode(2);
            }
            break;
            
        case CMD_HEATER:
            if (g_work_mode == 2) {  // 仅手动模式允许
                Control_Task_ManualControl(CTRL_HEATER, cmd.value);
            }
            break;
            
        case CMD_COOLER:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_COOLER, cmd.value);
            }
            break;
            
        case CMD_DEHUMIDIFIER:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_DEHUMID, cmd.value);
            }
            break;
            
        case CMD_HUMIDIFIER:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_HUMIDIFIER, cmd.value);
            }
            break;
            
        case CMD_TEMP_HIGH:
            Control_Task_SetTempThreshold(cmd.value, g_temp_low);
            break;
            
        case CMD_TEMP_LOW:
            Control_Task_SetTempThreshold(g_temp_high, cmd.value);
            break;
            
        case CMD_HUM_HIGH:
            Control_Task_SetHumidThreshold(cmd.value, g_humid_low);
            break;
            
        case CMD_HUM_LOW:
            Control_Task_SetHumidThreshold(g_humid_high, cmd.value);
            break;
            
        default:
            break;
    }
}

/**
 * @brief 主函数
 */
int main(void) {
    // 系统初始化
    System_Init();
    
    // 初始化调度器
    Scheduler_Init();
    
    // 注册周期性任务
    Scheduler_Register(OLED_Display_Task_Run, 100);   // ✅ OLED显示：100ms（完整3页面）
    Scheduler_Register(Key_Task_Scan, 50);            // ✅ 按键扫描：50ms
    Scheduler_Register(DHT22_Task_Run, 2000);         // DHT22读取：2s
    Scheduler_Register(Control_Task_Run, 500);        // 控制逻辑：500ms
    Scheduler_Register(Network_Task, 50);             // 网络任务：50ms（处理重连+指令解析）
    Scheduler_Register(Network_Upload_Task, 2000);    // 数据上传：2s
    
    Serial_Printf("Scheduler started, entering main loop\r\n");
    
    // 主循环
    while (1) {
        // 调度器运行（非阻塞）
        Scheduler_Run();
        
        // 处理云端指令
        Handle_Cloud_Command();
        
        // 定期发送心跳（每60秒）
        static uint32_t last_heartbeat = 0;
        if (sys_tick_ms - last_heartbeat > 60000) {
            Network_SendHeartbeat();
            last_heartbeat = sys_tick_ms;
        }
    }
}
