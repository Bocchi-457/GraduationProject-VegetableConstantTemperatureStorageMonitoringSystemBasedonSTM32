/**
 * dht11.c
 * DHT11温湿度传感器驱动文件
 * 功能：实现DHT11传感器的初始化和温湿度数据读取
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 * 日期：2026-03-07
 */

#include "dht11.h"

/**
 * 复位DHT11传感器
 * 功能：发送复位信号，使DHT11进入起始状态
 * @param 无
 * @retval 无
 */
void DHT11_Rst(void)
{
    DHT11_IO_OUT();  	// 设置为输出模式
    DHT11_DQ_OUT = 0;  	// 拉低DQ引脚
    delay_ms(20);     	// 拉低至少18ms
    DHT11_DQ_OUT = 1;  	// 拉高DQ引脚
    delay_us(30);      	// 主机拉高20~40us
}

/**
 * 等待DHT11的回应
 * 功能：检测DHT11是否存在并响应
 * @param 无
 * @retval 1: 未检测到DHT11的存在
 * @retval 0: DHT11存在
 */
u8 DHT11_Check(void)
{
    u8 retry = 0;
    DHT11_IO_IN();  // 设置为输入模式
    
    // 等待DHT11拉低（40~80us）
    while (DHT11_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    
    if (retry >= 100) return 1;  // 超时，未检测到DHT11
    else retry = 0;
    
    // 等待DHT11拉高（40~80us）
    while (!DHT11_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    
    if (retry >= 100) return 1;  // 超时，未检测到DHT11
    return 0;  // DHT11存在
}

/**
 * 从DHT11读取一个位
 * 功能：读取DHT11发送的单个位数据
 * @param 无
 * @retval 1: 读取到逻辑1
 * @retval 0: 读取到逻辑0
 */
u8 DHT11_Read_Bit(void)
{
    u8 retry = 0;
    
    // 等待变为低电平
    while (DHT11_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    
    retry = 0;
    
    // 等待变高电平
    while (!DHT11_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    
    delay_us(40);  // 等待40us，判断电平状态
    if (DHT11_DQ_IN) return 1;  // 高电平为1
    else return 0;               // 低电平为0
}

/**
 * 从DHT11读取一个字节
 * 功能：读取DHT11发送的8位数据
 * @param 无
 * @retval u8 读到的数据
 */
u8 DHT11_Read_Byte(void)
{
    u8 i, dat;
    dat = 0;
    
    // 读取8位数据，高位先传
    for (i = 0; i < 8; i++)
    {
        dat <<= 1;  // 左移一位
        dat |= DHT11_Read_Bit();  // 读取一位并或运算
    }
    
    return dat;
}

/**
 * 初始化DHT11的IO口并检测DHT11的存在
 * 功能：配置GPIO并检测DHT11是否存在
 * @param 无
 * @retval 1: DHT11不存在
 * @retval 0: DHT11存在
 */
u8 DHT11_Init(void)
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
    
    DHT11_Rst();  // 复位DHT11
    return DHT11_Check();  // 等待DHT11的回应
}

/**
 * 读取DHT11温湿度数据
 * 功能：从DHT11读取温湿度数据并存储到结构体中
 * @param DHT11_Data 温湿度数据结构体指针
 * @retval SUCCESS: 读取成功
 * @retval ERROR: 读取失败
 */
uint8_t Read_DHT11(DHT11_Data_TypeDef *DHT11_Data)
{
    u8 buf[5];
    u8 i;
    
    DHT11_Rst();  // 复位DHT11
    
    if (DHT11_Check() == 0)  // DHT11响应
    {
        for (i = 0; i < 5; i++)  // 读取40位数据
        {
            buf[i] = DHT11_Read_Byte();
        }
        
        // 检查校验和
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
        {
            DHT11_Data->humi_int = buf[0];     // 湿度整数部分
            DHT11_Data->humi_deci = buf[1];    // 湿度小数部分
            DHT11_Data->temp_int = buf[2];     // 温度整数部分
            DHT11_Data->temp_deci = buf[3];    // 温度小数部分
            DHT11_Data->check_sum = buf[4];    // 校验和
        }
    } else return 1;  // 未检测到DHT11
    
    /* 检查读取的数据是否正确 */
    if (DHT11_Data->check_sum == DHT11_Data->humi_int + DHT11_Data->humi_deci + DHT11_Data->temp_int + DHT11_Data->temp_deci)
        return SUCCESS;  // 校验成功
    else
        return ERROR;     // 校验失败
}

/*************************************END OF FILE******************************/
