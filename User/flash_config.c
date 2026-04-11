/**
 * @file flash_config.c
 * @brief Flash配置持久化模块实现
 * @note 使用STM32内部Flash存储系统配置参数
 */

#include "flash_config.h"
#include "stmflash.h"  // STM32 Flash读写驱动
#include <stdio.h>
#include <stdlib.h>    // abs()函数

// 全局配置变量
static SystemConfig_t g_system_config;

/**
 * @brief 计算配置校验和
 * @param config 配置结构体指针
 * @return 校验和（所有字节之和，排除checksum字段）
 */
static uint16_t CalculateChecksum(SystemConfig_t *config) {
    uint16_t sum = 0;
    uint8_t *data = (uint8_t *)config;
    
    // 计算除checksum字段外的所有字节的和
    for (uint16_t i = 0; i < sizeof(SystemConfig_t) - sizeof(uint16_t); i++) {
        sum += data[i];
    }
    
    return sum;
}

/**
 * @brief 验证配置数据有效性
 * @param config 配置结构体指针
 * @return 1:有效, 0:无效
 */
static uint8_t ValidateConfig(SystemConfig_t *config) {
    // 检查温度范围 (-40.0℃ ~ 80.0℃，放大10倍：-400 ~ 800)
    if (config->temperature_low < -400 || config->temperature_low > 800 ||
        config->temperature_high < -400 || config->temperature_high > 800) {
        return 0;
    }
    
    // 检查温度上下限关系
    if (config->temperature_low >= config->temperature_high) {
        return 0;
    }
    
    // 检查湿度范围 (0.0% ~ 100.0%，放大10倍：0 ~ 1000)
    if (config->humidity_low > 1000 || config->humidity_high > 1000) {
        return 0;
    }
    
    // 检查湿度上下限关系
    if (config->humidity_low >= config->humidity_high) {
        return 0;
    }
    
    // 检查工作模式有效性
    if (config->work_mode != 1 && config->work_mode != 2) {
        return 0;
    }
    
    // 检查校验和
    if (config->checksum != CalculateChecksum(config)) {
        return 0;
    }
    
    return 1;
}

/**
 * @brief 系统配置初始化
 * @note 从Flash读取配置，如果无效则使用默认配置
 */
void Flash_Config_Init(void) {
    SystemConfig_t flash_config;
    
    // 从Flash读取配置数据
    STMFLASH_Read(CONFIG_FLASH_ADDR, (uint16_t *)&flash_config, 
                  sizeof(SystemConfig_t) / 2);
    
    // 验证Flash中的数据有效性
    if (ValidateConfig(&flash_config)) {
        // 使用Flash中的配置
        g_system_config = flash_config;
        
        printf("[FLASH] Loaded from Flash\r\n");
    } else {
        // 使用默认配置
        g_system_config.temperature_high = DEFAULT_TEMP_HIGH;
        g_system_config.temperature_low = DEFAULT_TEMP_LOW;
        g_system_config.humidity_high = DEFAULT_HUMID_HIGH;
        g_system_config.humidity_low = DEFAULT_HUMID_LOW;
        g_system_config.work_mode = DEFAULT_WORK_MODE;
        g_system_config.checksum = CalculateChecksum(&g_system_config);
        
        printf("[FLASH] Using default config\r\n");
    }
}

/**
 * @brief 保存系统配置到Flash
 * @note 在阈值或模式修改后调用此函数
 */
void Flash_Config_Save(void) {
    // 更新校验和
    g_system_config.checksum = CalculateChecksum(&g_system_config);
    
    // 擦除Flash页
    STMFLASH_ErasePage(CONFIG_FLASH_ADDR);
    
    // 写入配置数据
    STMFLASH_Write(CONFIG_FLASH_ADDR, (uint16_t *)&g_system_config, 
                   sizeof(SystemConfig_t) / 2);
    
    printf("[FLASH] Config saved to Flash\r\n");
}

/**
 * @brief 获取当前系统配置
 * @return 系统配置结构体指针
 */
SystemConfig_t* Flash_Config_Get(void) {
    return &g_system_config;
}
