/**
 * cloud_protocol.h
 * 云端通信协议头文件
 * 版本：V1.0
 * MCU：STM32F103C8T6
 */

#ifndef __CLOUD_PROTOCOL_H__
#define __CLOUD_PROTOCOL_H__

#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* 云端指令类型 */
typedef enum {
    CMD_NONE = 0,
    CMD_AUTO_MODE,      // ZD - 自动模式
    CMD_MANUAL_MODE,    // SD - 手动模式
    CMD_HEATER_ON,      // KJR - 加热器开
    CMD_HEATER_OFF,     // GJR - 加热器关
    CMD_COOLER_ON,      // KZL - 制冷器开
    CMD_COOLER_OFF,     // GZL - 制冷器关
    CMD_DEHUMIDIFIER_ON,  // KCS - 除湿器开
    CMD_DEHUMIDIFIER_OFF, // GCS - 除湿器关
    CMD_HUMIDIFIER_ON,    // KJS - 加湿器开
    CMD_HUMIDIFIER_OFF,   // GJS - 加湿器关
    CMD_THRESHOLD       // 阈值设置
} CloudCommandType_t;

/* 云端指令结构 */
typedef struct {
    CloudCommandType_t type;
    int16_t value;           // 对于开关指令：0=关, 1=开
    int16_t temp_high;       // 温度上限（×10）
    int16_t temp_low;        // 温度下限（×10）
    int16_t humid_high;      // 湿度上限（×10）
    int16_t humid_low;       // 湿度下限（×10）
} CloudCommand_t;

/**
 * @brief 构建上传数据包
 * @param buffer 输出缓冲区
 * @param buf_size 缓冲区大小
 * @param mode 工作模式
 * @param temp_high 温度上限（×10）
 * @param temp_low 温度下限（×10）
 * @param humid_high 湿度上限（×10）
 * @param humid_low 湿度下限（×10）
 * @param heater 加热状态
 * @param cooler 制冷状态
 * @param dehumidifier 除湿状态
 * @param humidifier 加湿状态
 * @param temp_x10 当前温度（×10）
 * @param hum_x10 当前湿度（×10）
 * @return 数据包长度
 */
uint16_t Cloud_Build_Upload_Packet(
    char *buffer, 
    uint16_t buf_size,
    uint8_t mode,
    int16_t temp_high, int16_t temp_low,
    int16_t humid_high, int16_t humid_low,
    uint8_t heater, uint8_t cooler,
    uint8_t dehumidifier, uint8_t humidifier,
    int16_t temp_x10, int16_t hum_x10
);

/**
 * @brief 解析云端下发的指令
 * @param data 接收到的数据
 * @param len 数据长度
 * @param cmd 输出的指令结构
 * @return 1=解析成功, 0=无有效指令
 */
uint8_t Cloud_Parse_Command(const uint8_t *data, uint16_t len, CloudCommand_t *cmd);

#endif /* __CLOUD_PROTOCOL_H__ */
