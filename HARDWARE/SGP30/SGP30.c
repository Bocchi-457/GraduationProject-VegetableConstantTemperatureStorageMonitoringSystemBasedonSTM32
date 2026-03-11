/**
 * SGP30.c
 * SGP30气体传感器驱动实现文件
 * 功能：实现SGP30传感器的IIC通信和数据读写函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 * 作者：蔬菜恒温库监控系统开发团队
 * 创建日期：2026-03-08
 * 项目：蔬菜恒温库监控系统
 */

#include "sgp30.h"
#include "delay.h"

/**
 * @brief 初始化SGP30的GPIO引脚
 * @param 无
 * @return 无
 * @note 配置PB10和PB11为推挽输出
 */
void SGP30_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  // 使能GPIOB时钟
  RCC_APB2PeriphClockCmd(SGP30_SCL_GPIO_CLK | SGP30_SDA_GPIO_SDA, ENABLE);

  // 配置SCL引脚（PB10）
  GPIO_InitStructure.GPIO_Pin = SGP30_SCL_GPIO_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;      // 推挽输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;    // 速度50MHz
  GPIO_Init(SGP30_SCL_GPIO_PORT, &GPIO_InitStructure);

  // 配置SDA引脚（PB11）
  GPIO_InitStructure.GPIO_Pin = SGP30_SDA_GPIO_PIN;
  GPIO_Init(SGP30_SDA_GPIO_PORT, &GPIO_InitStructure);
}


/**
 * @brief 将SDA引脚设置为输出模式
 * @param 无
 * @return 无
 */
void SDA_OUT(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Pin = SGP30_SDA_GPIO_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;      // 推挽输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;    // 速度50MHz
  GPIO_Init(SGP30_SDA_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief 将SDA引脚设置为输入模式
 * @param 无
 * @return 无
 */
void SDA_IN(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Pin = SGP30_SDA_GPIO_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;    // 速度10MHz
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 浮空输入
  GPIO_Init(SGP30_SDA_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief 产生IIC起始信号
 * @param 无
 * @return 无
 * @note 当CLK为高电平时，DATA从高电平变为低电平
 */
void SGP30_IIC_Start(void)
{
  SDA_OUT();           // 设置SDA为输出
  SGP30_SDA = 1;       // SDA置高
  SGP30_SCL = 1;       // SCL置高
  delay_us(20);         // 延时

  SGP30_SDA = 0;       // START:当CLK为高电平时，DATA从高电平变为低电平
  delay_us(20);         // 延时
  SGP30_SCL = 0;       // 钳住I2C总线，准备发送或接收数据
}

/**
 * @brief 产生IIC停止信号
 * @param 无
 * @return 无
 * @note 当CLK为高电平时，DATA从低电平变为高电平
 */
void SGP30_IIC_Stop(void)
{
  SDA_OUT();           // 设置SDA为输出
  SGP30_SCL = 0;       // SCL置低
  SGP30_SDA = 0;       // STOP:当CLK为高电平时，DATA从低电平变为高电平
  delay_us(20);         // 延时
  SGP30_SCL = 1;       // SCL置高
  SGP30_SDA = 1;       // 发送I2C总线结束信号
  delay_us(20);         // 延时
}

/**
 * @brief 等待应答信号到来
 * @param 无
 * @return 0表示成功，1表示失败
 */
u8 SGP30_IIC_Wait_Ack(void)
{
  u8 ucErrTime = 0;
  SDA_IN();            // 设置SDA为输入
  SGP30_SDA = 1;       // SDA置高
  delay_us(10);         // 延时
  SGP30_SCL = 1;       // SCL置高
  delay_us(10);         // 延时
  while(SGP30_SDA_READ()) // 等待SDA变为低电平
  {
    ucErrTime++;        // 错误计数
    if(ucErrTime > 250) // 超时
    {
      SGP30_IIC_Stop(); // 发送停止信号
      return 1;         // 返回错误
    }
  }
  SGP30_SCL = 0;        // 时钟输出0
  return 0;             // 返回成功
}

/**
 * @brief 产生ACK应答
 * @param 无
 * @return 无
 */
void SGP30_IIC_Ack(void)
{
  SGP30_SCL = 0;        // SCL置低
  SDA_OUT();            // 设置SDA为输出
  SGP30_SDA = 0;        // SDA置低
  delay_us(20);         // 延时
  SGP30_SCL = 1;        // SCL置高
  delay_us(20);         // 延时
  SGP30_SCL = 0;        // SCL置低
}

/**
 * @brief 不产生ACK应答
 * @param 无
 * @return 无
 */
void SGP30_IIC_NAck(void)
{
  SGP30_SCL = 0;        // SCL置低
  SDA_OUT();            // 设置SDA为输出
  SGP30_SDA = 1;        // SDA置高
  delay_us(20);         // 延时
  SGP30_SCL = 1;        // SCL置高
  delay_us(20);         // 延时
  SGP30_SCL = 0;        // SCL置低
}

/**
 * @brief IIC发送一个字节
 * @param txd: 要发送的字节
 * @return 无
 */
void SGP30_IIC_Send_Byte(u8 txd)
{
  u8 t;
  SDA_OUT();            // 设置SDA为输出
  SGP30_SCL = 0;        // 拉低时钟开始数据传输
  for(t = 0; t < 8; t++) // 发送8位数据
  {
    if((txd & 0x80) >> 7) // 发送最高位
      SGP30_SDA = 1;
    else
      SGP30_SDA = 0;
    txd <<= 1;           // 左移一位
    delay_us(20);         // 延时
    SGP30_SCL = 1;        // SCL置高，数据有效
    delay_us(20);         // 延时
    SGP30_SCL = 0;        // SCL置低，准备下一位
    delay_us(20);         // 延时
  }
  delay_us(20);           // 延时
}

/**
 * @brief IIC读取一个字节
 * @param ack: 应答标志，1表示发送ACK，0表示发送NACK
 * @return 读取的字节
 */
u16 SGP30_IIC_Read_Byte(u8 ack)
{
  u8 i;
  u16 receive = 0;
  SDA_IN();            // 设置SDA为输入
  for(i = 0; i < 8; i++ ) // 读取8位数据
  {
    SGP30_SCL = 0;        // SCL置低
    delay_us(20);         // 延时
    SGP30_SCL = 1;        // SCL置高，数据有效
    receive <<= 1;        // 左移一位
    if(SGP30_SDA_READ())  // 读取SDA状态
      receive++;
    delay_us(20);         // 延时
  }
  if (!ack)
    SGP30_IIC_NAck();     // 发送nACK
  else
    SGP30_IIC_Ack();      // 发送ACK
  return receive;
}


/**
 * @brief 初始化SGP30传感器
 * @param 无
 * @return 无
 * @note 初始化GPIO并发送初始化命令
 */
void SGP30_Init(void)
{
  SGP30_GPIO_Init();       // 初始化GPIO
  SGP30_Write(0x20, 0x03); // 发送初始化命令（IAQ_Init）
  // SGP30_ad_write(0x20,0x61); // 预留命令
  // SGP30_ad_write(0x01,0x00); // 预留命令
}


/**
 * @brief 向SGP30写入命令
 * @param a: 命令高字节
 * @param b: 命令低字节
 * @return 无
 */
void SGP30_Write(u8 a, u8 b)
{
  SGP30_IIC_Start();                  // 发送开始信号
  SGP30_IIC_Send_Byte(SGP30_write);   // 发送器件地址+写指令
  SGP30_IIC_Wait_Ack();               // 等待应答
  SGP30_IIC_Send_Byte(a);             // 发送控制字节高8位
  SGP30_IIC_Wait_Ack();               // 等待应答
  SGP30_IIC_Send_Byte(b);             // 发送控制字节低8位
  SGP30_IIC_Wait_Ack();               // 等待应答
  SGP30_IIC_Stop();                   // 发送停止信号
  delay_ms(100);                      // 延时100ms
}

/**
 * @brief 从SGP30读取数据
 * @param 无
 * @return 读取的数据（32位）
 * @note 读取的数据包含eCO2和TVOC值
 */
u32 SGP30_Read(void)
{
  u32 dat;      // 存储读取的数据
  u8 crc;       // 存储CRC校验值
  SGP30_IIC_Start();                  // 发送开始信号
  SGP30_IIC_Send_Byte(SGP30_read);    // 发送器件地址+读指令
  SGP30_IIC_Wait_Ack();               // 等待应答
  dat = SGP30_IIC_Read_Byte(1);       // 读取第一个字节（eCO2高8位）
  dat <<= 8;                          // 左移8位
  dat += SGP30_IIC_Read_Byte(1);      // 读取第二个字节（eCO2低8位）
  crc = SGP30_IIC_Read_Byte(1);       // 读取CRC校验值，舍去
  crc = crc;                          // 为了不让出现编译警告
  dat <<= 8;                          // 左移8位
  dat += SGP30_IIC_Read_Byte(1);      // 读取第三个字节（TVOC高8位）
  dat <<= 8;                          // 左移8位
  dat += SGP30_IIC_Read_Byte(0);      // 读取第四个字节（TVOC低8位），发送NACK
  SGP30_IIC_Stop();                   // 发送停止信号
  return(dat);                        // 返回读取的数据
}

/***************************** 结束 *****************************/




