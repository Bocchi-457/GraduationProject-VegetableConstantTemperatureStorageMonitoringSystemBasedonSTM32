/**
 * cloud_protocol.c
 * 云端通信协议实现文件
 * 实现巴法云数据上传和指令解析功能
 * 版本：V1.0
 * MCU：STM32F103C8T6
 */

#include "cloud_protocol.h"
#include "wifi_driver.h"
#include <stdlib.h>  // 包含atof函数声明

/**
 * @brief 构建上传数据包
 */
uint16_t Cloud_Build_Upload_Packet(
    char *buffer, 
    uint16_t buf_size,
    uint8_t mode,
    int16_t temp_high, int16_t temp_low,
    int16_t humid_high, int16_t humid_low,
    uint8_t heater, uint8_t cooler,
    uint8_t dehumidifier, uint8_t humidifier,
    int16_t temp_x10, int16_t hum_x10)
{
    // 使用浮点数格式，支持小数和负数
    int n = snprintf(buffer, buf_size,
        "cmd=2&uid=%s&topic=data&msg=Mode:%d th:%.1f tl:%.1f hh:%.1f hl:%.1f "
        "jr:%d zl:%d cs:%d js:%d temp:%d.%d humi:%d.%d\r\n",
        BEMFA_UID,
        mode,
        (float)temp_high / 10.0f,   // th: 温度上限（浮点数，支持负数）
        (float)temp_low / 10.0f,    // tl: 温度下限（浮点数，支持负数）
        (float)humid_high / 10.0f,  // hh: 湿度上限（浮点数）
        (float)humid_low / 10.0f,   // hl: 湿度下限（浮点数）
        heater, cooler, dehumidifier, humidifier,
        temp_x10 / 10,
        (temp_x10 < 0 ? -temp_x10 : temp_x10) % 10,
        hum_x10 / 10,
        (hum_x10 < 0 ? -hum_x10 : hum_x10) % 10
    );
    
    return (n > 0 && n < buf_size) ? (uint16_t)n : 0;
}

/**
 * @brief 解析云端下发的指令
 */
uint8_t Cloud_Parse_Command(const uint8_t *data, uint16_t len, CloudCommand_t *cmd) {
    char *msg_start;
    char *payload;
    
    // 查找msg=位置
    msg_start = strstr((char *)data, "msg=");
    if (msg_start == NULL) {
        return 0;
    }
    
    payload = msg_start + 4;  // 跳过"msg="
    
    // 过滤自己上传的数据回显
    if (strstr(payload, "Mode:") != NULL && strstr(payload, "th:") != NULL) {
        return 0;  // 忽略自己的数据
    }
    
    // 初始化命令
    cmd->type = CMD_NONE;
    cmd->value = 0;
    cmd->temp_high = 0;
    cmd->temp_low = 0;
    cmd->humid_high = 0;
    cmd->humid_low = 0;
    
    // 解析阈值设置
    if (strstr(payload, "wendu_high=") != NULL || strstr(payload, "wendu_low=") != NULL) {
        char *pos;
        
        // 温度上限
        pos = strstr(payload, "wendu_high=");
        if (pos) {
            pos += 11;
            cmd->temp_high = (int16_t)(atof(pos) * 10);
        }
        
        // 温度下限
        pos = strstr(payload, "wendu_low=");
        if (pos) {
            pos += 10;
            cmd->temp_low = (int16_t)(atof(pos) * 10);
        }
        
        // 湿度上限
        pos = strstr(payload, "shidu_high=");
        if (pos) {
            pos += 11;
            cmd->humid_high = (int16_t)(atof(pos) * 10);
        }
        
        // 湿度下限
        pos = strstr(payload, "shidu_low=");
        if (pos) {
            pos += 10;
            cmd->humid_low = (int16_t)(atof(pos) * 10);
        }
        
        cmd->type = CMD_THRESHOLD;
        return 1;
    }
    
    // 解析单条指令
    if (strcmp(payload, "ZD") == 0 || strncmp(payload, "ZD\r\n", 4) == 0) {
        cmd->type = CMD_AUTO_MODE;
        cmd->value = 1;
        return 1;
    }
    
    if (strcmp(payload, "SD") == 0 || strncmp(payload, "SD\r\n", 4) == 0) {
        cmd->type = CMD_MANUAL_MODE;
        cmd->value = 1;
        return 1;
    }
    
    if (strcmp(payload, "KJR") == 0 || strncmp(payload, "KJR\r\n", 5) == 0) {
        cmd->type = CMD_HEATER_ON;
        cmd->value = 1;
        return 1;
    }
    
    if (strcmp(payload, "GJR") == 0 || strncmp(payload, "GJR\r\n", 5) == 0) {
        cmd->type = CMD_HEATER_OFF;
        cmd->value = 0;
        return 1;
    }
    
    if (strcmp(payload, "KZL") == 0 || strncmp(payload, "KZL\r\n", 5) == 0) {
        cmd->type = CMD_COOLER_ON;
        cmd->value = 1;
        return 1;
    }
    
    if (strcmp(payload, "GZL") == 0 || strncmp(payload, "GZL\r\n", 5) == 0) {
        cmd->type = CMD_COOLER_OFF;
        cmd->value = 0;
        return 1;
    }
    
    if (strcmp(payload, "KCS") == 0 || strncmp(payload, "KCS\r\n", 5) == 0) {
        cmd->type = CMD_DEHUMIDIFIER_ON;
        cmd->value = 1;
        return 1;
    }
    
    if (strcmp(payload, "GCS") == 0 || strncmp(payload, "GCS\r\n", 5) == 0) {
        cmd->type = CMD_DEHUMIDIFIER_OFF;
        cmd->value = 0;
        return 1;
    }
    
    if (strcmp(payload, "KJS") == 0 || strncmp(payload, "KJS\r\n", 5) == 0) {
        cmd->type = CMD_HUMIDIFIER_ON;
        cmd->value = 1;
        return 1;
    }
    
    if (strcmp(payload, "GJS") == 0 || strncmp(payload, "GJS\r\n", 5) == 0) {
        cmd->type = CMD_HUMIDIFIER_OFF;
        cmd->value = 0;
        return 1;
    }
    
    return 0;  // 未识别的指令
}
