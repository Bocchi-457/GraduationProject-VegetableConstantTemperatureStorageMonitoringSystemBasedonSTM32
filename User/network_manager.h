#ifndef __NETWORK_MANAGER_H
#define __NETWORK_MANAGER_H

#include <stdint.h>
#include "../Hardware/esp8266.h"

/**
 * 网络状态枚举
 */
typedef enum {
    NET_DISCONNECTED = 0,   // 未连接
    NET_CONNECTING,         // 连接中
    NET_CONNECTED,          // 已连接（正常）
    NET_RECONNECTING,       // 重连中
    NET_OFFLINE             // 离线模式
} NetworkState_t;

/**
 * 云端指令结构体
 */
typedef struct {
    uint8_t mode;           // 模式：1=自动, 2=手动
    int16_t wendu_high;     // 温度上限
    int16_t wendu_low;      // 温度下限
    int16_t shidu_high;     // 湿度上限
    int16_t shidu_low;      // 湿度下限
    uint8_t jiare;          // 加热：0=关, 1=开
    uint8_t zhileng;        // 制冷：0=关, 1=开
    uint8_t jiangshi;       // 降湿：0=关, 1=开
    uint8_t zhaoming;       // 照明：0=关, 1=开
    uint8_t valid;          // 指令有效标志
} CloudCommand_t;

/**
 * @brief 初始化网络管理模块
 * @note 必须在ESP8266_Init之后调用
 */
void Network_Manager_Init(void);

/**
 * @brief 网络任务调度（需在主循环中周期性调用）
 * @return 当前网络状态
 * @note 非阻塞，每次调用只执行少量工作
 */
NetworkState_t Network_Task(void);

/**
 * @brief 获取当前网络状态
 */
NetworkState_t Network_GetState(void);

/**
 * @brief 上传传感器数据到云端
 * @param temp_x10: 温度值（放大10倍，如25.6℃传256）
 * @param hum_x10: 湿度值（放大10倍，如60.5%传605）
 * @param mode: 工作模式（1=自动, 2=手动）
 * @param wendu_high: 温度上限（放大10倍）
 * @param wendu_low: 温度下限（放大10倍）
 * @param shidu_high: 湿度上限（放大10倍）
 * @param shidu_low: 湿度下限（放大10倍）
 * @param jiare: 加热状态（0=关, 1=开）
 * @param zhileng: 制冷状态（0=关, 1=开）
 * @param chushi: 除湿状态（0=关, 1=开）
 * @param jiashi: 加湿状态（0=关, 1=开）
 * @return 0=成功, 1=失败
 * @note 上传格式与旧代码完全兼容：Mode:X th:X tl:X hh:X hl:X jr:X zl:X cs:X js:X temp:X.X humi:X.X
 */
uint8_t Network_UploadSensorData(int16_t temp_x10, int16_t hum_x10, 
                                  uint8_t mode,
                                  int16_t wendu_high, int16_t wendu_low,
                                  int16_t shidu_high, int16_t shidu_low,
                                  uint8_t jiare, uint8_t zhileng,
                                  uint8_t chushi, uint8_t jiashi);

/**
 * @brief 处理云端下发的指令
 * @param cmd: 输出参数，接收解析后的指令
 * @return 0=有新指令, 1=无指令
 */
uint8_t Network_ProcessCloudCommand(CloudCommand_t *cmd);

/**
 * @brief 强制触发重连
 * @note 用于手动测试或紧急情况
 */
void Network_ForceReconnect(void);

/**
 * @brief 发送心跳包（保持在线）
 * @note 每60秒调用一次，由应用层控制调用时机
 */
void Network_SendHeartbeat(void);

/**
 * @brief 请求时间同步
 * @note 异步操作，需多次调用Network_Task等待完成
 * @return 0=请求已发送, 1=未连接
 */
uint8_t Network_RequestTimeSync(void);

/**
 * @brief 检查时间同步是否完成并获取结果
 * @param time_str: 输出参数，格式"YYYY-MM-DD HH:MM:SS"（至少20字节）
 * @return 0=已完成且有数据, 1=进行中或无数据
 */
uint8_t Network_GetSyncedTime(char *time_str);

#endif
