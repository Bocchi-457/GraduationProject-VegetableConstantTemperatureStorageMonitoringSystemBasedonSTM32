#ifndef __NETWORK_CORE_H__
#define __NETWORK_CORE_H__

#include <stdint.h>
#include "cloud_protocol.h"

/* 网络状态 */
typedef enum {
    NET_STATE_DISCONNECTED = 0,
    NET_STATE_INITIALIZING,
    NET_STATE_CONNECTED,
    NET_STATE_RECONNECTING,
    NET_STATE_WIFI_DISCONNECTED  // WiFi已断开，等待恢复
} NetworkState_t;

/**
 * @brief 初始化网络模块
 */
void Network_Core_Init(void);

/**
 * @brief 网络任务（每50ms调用一次）
 */
void Network_Core_Task(void);

/**
 * @brief 上传传感器数据
 * @return 0=成功启动上传, 1=忙或失败
 */
uint8_t Network_Core_Upload(
    uint8_t mode,
    int16_t temp_high, int16_t temp_low,
    int16_t humid_high, int16_t humid_low,
    uint8_t heater, uint8_t cooler,
    uint8_t dehumidifier, uint8_t humidifier,
    int16_t temp_x10, int16_t hum_x10
);

/**
 * @brief 获取云端指令
 * @param cmd 输出的指令结构
 * @return 1=有指令, 0=无指令
 */
uint8_t Network_Core_Get_Command(CloudCommand_t *cmd);

/**
 * @brief 获取当前网络状态
 */
NetworkState_t Network_Core_Get_State(void);

/**
 * @brief 检查OLED是否被锁定（联网初始化期间）
 * @return 1=锁定，0=未锁定
 */
uint8_t Network_Core_Is_OLED_Locked(void);

/**
 * @brief 重置上传监控状态（TCP重连成功后调用）
 */
void Network_Core_Reset_Upload_Monitor(void);

/**
 * @brief 获取WiFi状态（供OLED显示使用）
 * @return 0=断开, 1=已连接, 2=重连中
 */
uint8_t Get_WiFi_State(void);

/**
 * @brief 在右上角显示WiFi状态图标（不清屏）
 * @param state: 0=未连接, 1=已连接, 2=重连中
 */
void Show_WiFi_Status_Icon(uint8_t state);

#endif /* __NETWORK_CORE_H__ */
