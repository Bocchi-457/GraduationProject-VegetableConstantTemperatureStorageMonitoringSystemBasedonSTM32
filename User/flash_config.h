/**
 * @file flash_config.h
 * @brief Flash配置持久化模块
 * @note 用于保存和读取系统配置参数（阈值、模式等）到STM32内部Flash
 */

#ifndef __FLASH_CONFIG_H__
#define __FLASH_CONFIG_H__

#include "stm32f10x.h"
#include "stdint.h"

/**
 * @brief 系统配置结构体
 * @note 所有数值均为放大10倍的整数
 */
typedef struct {
    int16_t temperature_high;   // 温度上限 (放大10倍，如250表示25.0℃，范围-400~800)
    int16_t temperature_low;    // 温度下限 (放大10倍)
    uint16_t humidity_high;     // 湿度上限 (放大10倍，如650表示65.0%，范围0~1000)
    uint16_t humidity_low;      // 湿度下限 (放大10倍)
    uint8_t work_mode;          // 工作模式 (1:自动, 2:手动)
    uint16_t checksum;          // 校验和，用于数据完整性验证
} SystemConfig_t;

/**
 * @brief Flash存储地址定义
 * @note 使用STM32F103C8T6的最后一页Flash（64KB中的最后2KB）
 */
#define CONFIG_FLASH_ADDR     0x0800FC00  // 配置数据存储地址
#define CONFIG_MAGIC_NUMBER   0xAA55      // 配置数据魔数，用于识别有效数据

/**
 * @brief 默认配置参数
 */
#define DEFAULT_TEMP_HIGH     270   // 27.0℃
#define DEFAULT_TEMP_LOW      210   // 21.0℃
#define DEFAULT_HUMID_HIGH    650   // 65.0%
#define DEFAULT_HUMID_LOW     500   // 50.0%
#define DEFAULT_WORK_MODE     1     // 自动模式

/**
 * @brief 系统配置初始化
 * @note 从Flash读取配置，如果无效则使用默认配置
 */
void Flash_Config_Init(void);

/**
 * @brief 保存系统配置到Flash
 * @note 在阈值或模式修改后调用此函数
 */
void Flash_Config_Save(void);

/**
 * @brief 获取当前系统配置
 * @return 系统配置结构体指针
 */
SystemConfig_t* Flash_Config_Get(void);

#endif // __FLASH_CONFIG_H__
