/**
 * dht22.c
 * DHT22温湿度传感器驱动文件
 * 功能：实现DHT22传感器的初始化和温湿度数据读取
 * 版本：V1.1
 * MCU：STM32F103C8T6
 */

#include "dht22.h"
#include "delay.h"

/**
 * DHT22数据结构全局变量（唯一实例）
 * @note 其他模块通过extern访问，遵循单一定义原则
 */
DHT22_Data_TypeDef DHT22_Data = {0};

/**
 * 发送复位/起始信号
 * 时序：拉低≥800us，典型值1ms
 */
void DHT22_Rst(void) {
  DHT22_IO_OUT();   // 配置为开漏输出
  DHT22_DQ_OUT = 0; // 拉低总线
  delay_ms(1);      // 拉低1ms
  DHT22_DQ_OUT = 1; // 释放总线
  delay_us(30);     // 等待25~45us
  DHT22_IO_IN();    // 切换为输入模式，彻底释放总线
}

/**
 * 检测DHT22响应信号
 * 时序：80us低电平 + 80us高电平
 * @retval 0=响应成功，1=响应失败
 */
static u8 DHT22_Check(void) {
  u8 retry = 0;
  DHT22_IO_IN(); // 确保为输入模式

  // 1. 等待响应低电平 (Trel=75~85us)
  // 当DQ为高电平时等待，直到DQ变为低电平
  while (DHT22_DQ_IN && retry < 200) {
    retry++;
    delay_us(1);
  }
  if (retry >= 200)
    return 1; // 超时

  retry = 0;
  // 2. 等待响应高电平 (Treh=75~85us)
  // 当DQ为低电平时等待，直到DQ变为高电平
  while (!DHT22_DQ_IN && retry < 200) {
    retry++;
    delay_us(1);
  }
  if (retry >= 200)
    return 1; // 超时

  return 0; // 响应成功
}

/**
 * 读取单个位数据
 * 位时序：
 * - 位0：50us低 + 22~30us高
 * - 位1：50us低 + 68~75us高
 * @retval 0/1=数据位，0xFF=读取失败
 */
static u8 DHT22_Read_Bit(void) {
  u8 retry = 0;

  // 1. 等待位起始的低电平 (TLOW=48~55us)
  // 当DQ为高电平时等待，直到DQ变为低电平
  while (DHT22_DQ_IN && retry < 100) {
    retry++;
    delay_us(1);
  }
  if (retry >= 100)
    return 0xFF; // 失败

  retry = 0;
  // 2. 等待低电平结束
  // 当DQ为低电平时等待，直到DQ变为高电平
  while (!DHT22_DQ_IN && retry < 100) {
    retry++;
    delay_us(1);
  }
  if (retry >= 100)
    return 0xFF; // 失败

  // 3. 时序判断点：延时35us
  // 此时位0的高电平已结束，位1的高电平仍在持续
  delay_us(35);
  return DHT22_DQ_IN ? 1 : 0;
}

/**
 * 读取单个字节数据
 * @retval 读取到的字节，0xFF=读取失败
 */
static u8 DHT22_Read_Byte(void) {
  u8 i, dat = 0;
  u8 bit;

  for (i = 0; i < 8; i++) {
    dat <<= 1; // 高位先出
    bit = DHT22_Read_Bit();
    if (bit == 0xFF)
      return 0xFF; // 位读取失败
    dat |= bit;
  }
  return dat;
}

/**
 * DHT22初始化
 * 上电后必须等待2s越过不稳定期
 * @retval 0=初始化成功，1=失败
 */
u8 DHT22_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;

  // 1. 使能GPIOB时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

  // 2. 配置PB12为开漏输出
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_12); // 初始拉高

  // 3.上电等待2s不稳定期
  // delay_ms(2000);

  // 4. 发送复位信号并检测响应
  DHT22_Rst();
  return DHT22_Check();
}

uint8_t Read_DHT22(DHT22_Data_TypeDef *data) {
  u8 buf[5] = {0};
  u8 i;
  u16 temp_raw;

  DHT22_Rst();
  if (DHT22_Check() != 0)
    return ERROR;

  // 关中断，保护微秒级时序
  __disable_irq();

  for (i = 0; i < 5; i++) {
    buf[i] = DHT22_Read_Byte();
    if (buf[i] == 0xFF) {
      __enable_irq(); // 失败退出前务必恢复中断
      return ERROR;
    }
  }

  __enable_irq(); // 读取完毕，恢复中断
  // ----------------------------------------

  // 强制转为8位，防止隐式整数提升导致校验失败
  if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) {
    return ERROR;
  }

  // 湿度计算
  data->humidity = (buf[0] << 8) | buf[1];

  // 温度计算，保留负号
  temp_raw = (buf[2] << 8) | buf[3];
  if (temp_raw & 0x8000) // 判断最高位，如果是1代表负温度
  {
    data->temperature = -(temp_raw & 0x7FFF); // 提取低15位并加负号
  } else {
    data->temperature = (temp_raw & 0x7FFF);
  }

  data->check_sum = buf[4];
  return SUCCESS;
}

