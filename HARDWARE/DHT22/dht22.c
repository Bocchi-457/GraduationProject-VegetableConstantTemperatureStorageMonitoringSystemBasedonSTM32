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

/**
 * 复位DHT22传感器
 * 功能：发送复位信号，使DHT22进入起始状态
 * @param 无
 * @retval 无
 */
void DHT22_Rst(void)
{
    DHT22_IO_OUT();  	    // 设置为输出模式
    DHT22_DQ_OUT = 0;  	    // 拉低DQ引脚
    delay_ms(3);         	// 拉低3ms，确保DHT22正确响应
    DHT22_DQ_OUT = 1;  	    // 拉高DQ引脚
    delay_us(30);          	// 主机拉高20~40us
}

/**
 * 等待DHT22的回应
 * 功能：检测DHT22是否存在并响应
 * @param 无
 * @retval 1: 未检测到DHT22的存在
 * @retval 0: DHT22存在
 */
u8 DHT22_Check(void)
{
    u8 retry = 0;
    DHT22_IO_IN();  // 设置为输入模式
    
    // 等待DHT22拉低（80us左右）
    while (DHT22_DQ_IN && retry < 200)
    {
        retry++;
        delay_us(1);
    }
    
    if (retry >= 200) return 1;  // 超时，未检测到DHT22
    else retry = 0;
    
    // 等待DHT22拉高（80us左右）
    while (!DHT22_DQ_IN && retry < 200)
    {
        retry++;
        delay_us(1);
    }
    
    if (retry >= 200) return 1;  // 超时，未检测到DHT22
    return 0;  // DHT22存在
}

/**
 * 从DHT22读取一个位
 * 功能：读取DHT22发送的单个位数据（区分0/1时序）
 * @param 无
 * @retval 1: 读取到逻辑1
 * @retval 0: 读取到逻辑0
 */
u8 DHT22_Read_Bit(void)
{
    u8 retry = 0;
    
    // 等待变为低电平（DHT22拉低50us）
    while (DHT22_DQ_IN && retry < 200)
    {
        retry++;
        delay_us(1);
    }
    retry = 0;
    
    // 等待变高电平
    while (!DHT22_DQ_IN && retry < 200)
    {
        retry++;
        delay_us(1);
    }
    
    delay_us(40);  // 等待40us后判断：0码高电平≤30us，1码≥60us
    if (DHT22_DQ_IN) return 1;  // 高电平为1
    else return 0;               // 低电平为0
}

/**
 * 从DHT22读取一个字节
 * 功能：读取DHT22发送的8位数据
 * @param 无
 * @retval u8 读到的数据
 */
u8 DHT22_Read_Byte(void)
{
    u8 i, dat;
    dat = 0;
    
    // 读取8位数据，高位先传
    for (i = 0; i < 8; i++)
    {
        dat <<= 1;  // 左移一位
        dat |= DHT22_Read_Bit();  // 读取一位并或运算
    }
    
    return dat;
}

/**
 * 初始化DHT22的IO口并检测DHT22的存在
 * 功能：配置GPIO并检测DHT22是否存在
 * @param 无
 * @retval 1: DHT22不存在
 * @retval 0: DHT22存在
 */
u8 DHT22_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 使能GPIOB端口时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // 配置PB12端口
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;                // PB12
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;          // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;         // GPIO速度50MHz
    GPIO_Init(GPIOB, &GPIO_InitStructure);                    // 初始化IO口
    GPIO_SetBits(GPIOB, GPIO_Pin_12);                         // PB12输出高
    
    DHT22_Rst();  // 复位DHT22
    return DHT22_Check();  // 等待DHT22的回应
}

/**
 * 读取DHT22温湿度数据
 * 功能：从DHT22读取温湿度数据并存储到结构体中，增加重试机制
 * @param DHT22_Data 温湿度数据结构体指针
 * @retval SUCCESS: 读取成功
 * @retval ERROR: 读取失败
 */
uint8_t Read_DHT22(DHT22_Data_TypeDef *DHT22_Data)
{
    u8 retry = 3; // 最多重试3次
    
    while (retry--)
    {
        u8 buf[5] = {0};  // 初始化buf数组，避免未定义行为
        u8 i;
        u16 humi, temp;
        
        DHT22_Rst();  // 复位DHT22
        
        if (DHT22_Check() == 0)  // DHT22响应
        {
            for (i = 0; i < 5; i++)  // 读取40位数据
            {
                buf[i] = DHT22_Read_Byte();
            }
            
            // 检查校验和（湿度高+湿度低+温度高+温度低 = 校验和）
            if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
            {
                // 计算湿度：(buf[0]<<8 | buf[1]) / 10 → 整数+小数
                humi = (buf[0] << 8) | buf[1];
                DHT22_Data->humi_int = humi / 10;
                DHT22_Data->humi_deci = humi % 10;
                
                // 计算温度：处理符号位（最高位为1表示负温度）
                temp = (buf[2] << 8) | buf[3];
                if(temp & 0x8000) // 负温度
                {
                    temp = ~temp + 1; // 补码转原码
                    DHT22_Data->temp_int = -(temp / 10);
                }
                else // 正温度
                {
                    DHT22_Data->temp_int = temp / 10;
                }
                DHT22_Data->temp_deci = temp % 10;
                
                DHT22_Data->check_sum = buf[4];    // 校验和
                
                // 数据范围检查，确保数据在合理范围内
                if (DHT22_Data->humi_int >= 100)  // 湿度范围0-100%
                    continue; // 数据异常，重试
                if (DHT22_Data->temp_int < -40 || DHT22_Data->temp_int > 80)  // 温度范围-40~80℃
                    continue; // 数据异常，重试
                
                return SUCCESS;  // 读取成功
            }
        }
        
        delay_ms(100); // 重试间隔
    }
    
    return ERROR;  // 读取失败
}