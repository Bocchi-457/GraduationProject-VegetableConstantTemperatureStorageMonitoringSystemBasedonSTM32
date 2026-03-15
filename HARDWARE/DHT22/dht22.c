/**
 * dht22.c
 * DHT22温湿度传感器驱动文件
 * 功能：实现DHT22传感器的初始化和温湿度数据读取
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-07
 */

#include "dht22.h"
#include "delay.h"

/**
 * 发送复位/起始信号
 * 遵循手册7.3节时序：拉低≥800us，典型值1ms
 */
void DHT22_Rst(void)
{
    DHT22_IO_OUT();   // 配置为开漏输出
    DHT22_DQ_OUT = 0; // 拉低总线
    delay_ms(1);      // 严格拉低1ms
    DHT22_DQ_OUT = 1; // 释放总线
    delay_us(30);     // 等待25~45us
    DHT22_IO_IN();    // 切换为输入模式，彻底释放总线
}

/**
 * 检测DHT22响应信号
 * 遵循手册7.3节时序：80us低电平 + 80us高电平
 * @retval 0=响应成功，1=响应失败
 */
static u8 DHT22_Check(void)
{
    u8 retry = 0;
    DHT22_IO_IN(); // 确保为输入模式
    
    // 1. 等待响应低电平 (Trel=75~85us)
    // 当DQ为高电平时等待，直到DQ变为低电平
    while (DHT22_DQ_IN && retry < 200)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 200) return 1; // 超时
    
    retry = 0;
    // 2. 等待响应高电平 (Treh=75~85us)
    // 当DQ为低电平时等待，直到DQ变为高电平
    while (!DHT22_DQ_IN && retry < 200)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 200) return 1; // 超时
    
    return 0; // 响应成功
}

/**
 * 读取单个位数据
 * 遵循手册7.3节位时序：
 * - 位0：50us低 + 22~30us高
 * - 位1：50us低 + 68~75us高
 * @retval 0/1=数据位，0xFF=读取失败
 */
static u8 DHT22_Read_Bit(void)
{
    u8 retry = 0;
    
    // 1. 等待位起始的低电平 (TLOW=48~55us)
    // 当DQ为高电平时等待，直到DQ变为低电平
    while (DHT22_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100) return 0xFF; // 失败
    
    retry = 0;
    // 2. 等待低电平结束
    // 当DQ为低电平时等待，直到DQ变为高电平
    while (!DHT22_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100) return 0xFF; // 失败
    
    // 3. 时序判断点：延时35us
    // 此时位0的高电平已结束，位1的高电平仍在持续
    delay_us(35);
    return DHT22_DQ_IN ? 1 : 0;
    
}

/**
 * 读取单个字节数据
 * @retval 读取到的字节，0xFF=读取失败
 */
static u8 DHT22_Read_Byte(void)
{
    u8 i, dat = 0;
    u8 bit;
    
    for (i = 0; i < 8; i++)
    {
        dat <<= 1; // 高位先出
        bit = DHT22_Read_Bit();
        if (bit == 0xFF) return 0xFF; // 位读取失败
        dat |= bit;
    }
    return dat;
}

/**
 * DHT22初始化
 * 遵循手册7.4节：上电后必须等待2s越过不稳定期
 * @retval 0=初始化成功，1=失败
 */
u8 DHT22_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 1. 使能GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // 2. 配置PB12为开漏输出
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD; // 开漏输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_12); // 初始拉高
    
    // 3. 手册强制要求：上电等待2s不稳定期
    // delay_ms(2000);
    
    // 4. 发送复位信号并检测响应
    DHT22_Rst();
    return DHT22_Check();
}

/**
 * 读取DHT22温湿度数据
 * 遵循手册7.2节数据格式：
 * 40位 = 湿度高8位 + 湿度低8位 + 温度高8位 + 温度低8位 + 校验和
 * @param data 数据结构体指针
 * @retval SUCCESS=成功，ERROR=失败
 */
uint8_t Read_DHT22(DHT22_Data_TypeDef *data)
{
    u8 buf[5] = {0};
    u8 i;
    u16 humi_raw, temp_raw;
    
    // 1. 发送起始信号并检测响应
    DHT22_Rst();
    if (DHT22_Check() != 0) return ERROR;
    
    // 2. 连续读取40位数据
    for (i = 0; i < 5; i++)
    {
        buf[i] = DHT22_Read_Byte();
        if (buf[i] == 0xFF) return ERROR; // 读取失败
    }
    
    // 3. 校验和检查
    if ((buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
    {
        return ERROR;
    }
    
    // 4. 湿度计算 (量程：0~99.9%RH)
    humi_raw = (buf[0] << 8) | buf[1];
    if (humi_raw > 999) return ERROR; // 量程校验
    data->humi_int  = humi_raw / 10;
    data->humi_deci = humi_raw % 10;
    
    // 5. 温度计算 (严格遵循手册7.2节)
    // 最高位(Bit15)=1表示负温度，低15位为绝对值的10倍
    temp_raw = (buf[2] << 8) | buf[3];
    
    // 正温度量程校验 (0~80.0℃)
    if ((temp_raw & 0x7FFF) > 800) return ERROR;
    
    if (temp_raw & 0x8000) // 负温度
    {
        // 负温度量程校验 (-40.0~0℃)
        if ((temp_raw & 0x7FFF) > 400) return ERROR;
        data->temp_int = -((temp_raw & 0x7FFF) / 10);
    }
    else // 正温度
    {
        data->temp_int = (temp_raw & 0x7FFF) / 10;
    }
    data->temp_deci = (temp_raw & 0x7FFF) % 10;
    data->check_sum = buf[4];
    
    return SUCCESS;
}

/**
 * 校验和验证
 * @param data DHT22数据结构体
 * @return 0: 校验成功, 1: 校验失败
 */
uint8_t DHT22_CheckSum(DHT22_Data_TypeDef *data)
{
    uint8_t sum = data->humi_int + data->humi_deci + data->temp_int + data->temp_deci;
    return (sum == data->check_sum) ? SUCCESS : ERROR;
}