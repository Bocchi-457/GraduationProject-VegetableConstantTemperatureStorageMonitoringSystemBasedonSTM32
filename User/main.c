/**
 * main.c
 * 蔬菜恒温库监控系统主程序
 * 实现系统初始化、任务调度和联网控制功能
 * 版本：V2.0
 * MCU：STM32F103C8T6
 */


#include "stm32f10x.h"
#include "scheduler.h"
#include "network_core.h"        // 【重构】新的网络核心模块
#include "cloud_protocol.h"      // 【重构】云端协议模块
#include "dht22_task.h"
#include "control_task.h"
#include "key_task.h"
#include "oled_display_task.h"
#include "DS1302.h"
#include "dht22.h"
#include "control.h"
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

    // 联网模式标志（1=联网模式，0=离线模式）
    uint8_t g_network_enabled = 0;

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
    
    Ds1302_Init();        // DS1302 RTC初始化
    Timer_Init();         // 定时器初始化（TIM2: 10ms心跳）
    
    // 关键改进：OLED初始化后立即显示提示，避免用户面对黑屏等待
    OLED_ShowCHinese(0, 3, 19);  // 系
    OLED_ShowCHinese(18, 3, 20); // 统
    OLED_ShowCHinese(36, 3, 21); // 正
    OLED_ShowCHinese(54, 3, 22); // 在
    OLED_ShowCHinese(72, 3, 0);  // 初
    OLED_ShowCHinese(90, 3, 1);  // 始
    OLED_ShowCHinese(108, 3, 2); // 化
    
    // 核心任务初始化（属于系统初始化的一部分，必须在开机引导前完成）
    DHT22_Task_Init();    // DHT22任务初始化
    Control_Task_Init();  // 控制任务初始化
    Key_Task_Init();      // 按键任务初始化
    OLED_Display_Task_Init();  // OLED显示任务初始化
    
    // 上电延时，确保DHT22稳定（2秒）
    delay_ms(2000);
    
    // 系统初始化完成，蜂鸣器鸣响提示
    beep = 0;
    delay_ms(100);
    beep = 1;
    
    // 开机引导界面 - 询问是否联网
    OLED_Clear(0);
    
    // 显示标题："是否需要联网？"
    OLED_ShowCHinese(0, 0, 52);   // 是
    OLED_ShowCHinese(18, 0, 53);  // 否
    OLED_ShowCHinese(36, 0, 54);  // 需
    OLED_ShowCHinese(52, 0, 55);  // 要
    OLED_ShowCHinese(68, 0, 56);  // 联
    OLED_ShowCHinese(84, 0, 57);  // 网
    OLED_ShowCHinese(100, 0, 58); // ？
    
    // 显示菜单选项
    OLED_ShowString(8, 4, "1.", 16);
    OLED_ShowCHinese(24, 4, 52); // 是
    OLED_ShowString(62, 4, "2.", 16);
    OLED_ShowCHinese(78, 4, 53); // 否
    
    // 等待用户选择
    uint8_t key_choice = 0;
    while (key_choice != 1 && key_choice != 2) {
        key_choice = KEY_Scan(0);  // 按键扫描
        delay_ms(50);  // 防止按键抖动
    }
    
    OLED_Clear(0);  // 清屏，准备进入主界面
    
    // 根据用户选择决定是否初始化网络模块
    if (key_choice == 1) {
        // 用户选择“是”，初始化网络模块
        g_network_enabled = 1;
        Serial_Printf("Initializing network module...\r\n");
        Network_Core_Init();  // 【重构】使用新的网络核心模块
    } else {
        // 用户选择"否"，跳过网络初始化
        g_network_enabled = 0;
        Serial_Printf("Network module skipped (offline mode)\r\n");
    }
    
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
    if (Network_Core_Get_State() != NET_STATE_CONNECTED) {
        return;
    }
    
    // 检查DHT22数据有效性
    if (!g_dht22_data_valid) {
        return;
    }
    
    // 【重构】使用新的网络核心模块上传数据
    Network_Core_Upload(
        g_work_mode,
        g_temp_high, g_temp_low,
        g_humid_high, g_humid_low,
        g_heater_state,
        g_cooler_state,
        g_dehumid_state,
        g_humidifier_state,
        DHT22_Data.temperature,
        DHT22_Data.humidity
    );
}

/**
 * @brief 处理云端指令
 */
void Handle_Cloud_Command(void) {
    CloudCommand_t cmd;
    
    // 【重构】使用新的网络核心模块获取指令
    if (!Network_Core_Get_Command(&cmd)) {
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
            
        case CMD_HEATER_ON:
            if (g_work_mode == 2) {  // 仅手动模式允许
                Control_Task_ManualControl(CTRL_HEATER, 1);
            }
            break;
            
        case CMD_HEATER_OFF:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_HEATER, 0);
            }
            break;
            
        case CMD_COOLER_ON:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_COOLER, 1);
            }
            break;
            
        case CMD_COOLER_OFF:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_COOLER, 0);
            }
            break;
            
        case CMD_DEHUMIDIFIER_ON:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_DEHUMID, 1);
            }
            break;
            
        case CMD_DEHUMIDIFIER_OFF:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_DEHUMID, 0);
            }
            break;
            
        case CMD_HUMIDIFIER_ON:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_HUMIDIFIER, 1);
            }
            break;
            
        case CMD_HUMIDIFIER_OFF:
            if (g_work_mode == 2) {
                Control_Task_ManualControl(CTRL_HUMIDIFIER, 0);
            }
            break;
            
        case CMD_THRESHOLD:
            // 设置温度阈值
            if (cmd.temp_high != 0 && cmd.temp_low != 0) {
                Control_Task_SetTempThreshold(cmd.temp_high, cmd.temp_low);
            }
            // 设置湿度阈值
            if (cmd.humid_high != 0 && cmd.humid_low != 0) {
                Control_Task_SetHumidThreshold(cmd.humid_high, cmd.humid_low);
            }
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
    Scheduler_Register(OLED_Display_Task_Run, 100);   // OLED显示：100ms（完整3页面）
    Scheduler_Register(Key_Task_Scan, 50);            // 按键扫描：50ms
    Scheduler_Register(DHT22_Task_Run, 2000);         // DHT22读取：2s
    Scheduler_Register(Control_Task_Run, 500);        // 控制逻辑：500ms
    
    // 根据联网模式决定是否注册网络任务
    if (g_network_enabled) {
        Scheduler_Register(Network_Core_Task, 50);             // 【重构】网络任务：50ms
        // Scheduler_Register(Network_Upload_Task, 2000);      // 数据上传已移至DHT22任务中
        Scheduler_Register(Handle_Cloud_Command, 100);         // 【重构】云端指令处理：100ms
        Serial_Printf("Network tasks registered\r\n");
    } else {
        Serial_Printf("Running in offline mode\r\n");
    }
    
    Serial_Printf("Scheduler started, entering main loop\r\n");
    
    // 主循环
    while (1) {
        // 调度器运行（非阻塞）
        Scheduler_Run();
        
        // 仅在联网模式下处理云端指令
        if (g_network_enabled) {
            Handle_Cloud_Command();
            
            // 删除：独立心跳发送逻辑
            // 根据巴法云协议，每次成功的数据上传即视为心跳，无需单独发送
            // 这样可以减少AT指令竞争，简化代码逻辑
        }
    }
}
