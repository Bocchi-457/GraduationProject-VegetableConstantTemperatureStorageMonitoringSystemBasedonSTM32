/**
 * Usart.c
 * 串口驱动实现文件
 * 功能：实现串口初始化和数据收发功能
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 */

#include "Usart.h"
#include "OLED.h"
#include "sys.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


//////////////////////////////////////////////////////////////////////////////////
// 如果使用ucos,则包括下面的头文件即可.
#if SYSTEM_SUPPORT_UCOS
#include "includes.h" //ucos 使用
#endif

/**
 * @brief 全局变量定义
 */
unsigned short data_cnt = 0; // 数据计数器
uint8_t Seria1_TxPacket[Packet_Len] = {0xFF, 0x01, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0xEF}; // 发送数据包
char Serial_RxPacket[50];                                       // 接收数据包
uint8_t Serial_RxFlag;                                          // 接收标志位
/**
 * @brief 支持printf函数的实现
 * @note 不需要选择use MicroLIB
 */
#if 1
#pragma import(__use_no_semihosting)
// 标准库需要的支持函数
struct __FILE {
  int handle;
};

FILE __stdout;
// 定义_sys_exit()以避免使用半主机模式
void _sys_exit(int x) { x = x; }
// 重定义fputc函数
int fputc(int ch, FILE *f) {
  while ((USART1->SR & 0X40) == 0) // 循环发送,直到发送完毕
    USART1->DR = (u8)ch;
  return ch;
}
#endif

/**
 * @brief 使用microLib的方法
 * @note 此部分代码被注释，如需使用请取消注释
 */
/*
int fputc(int ch, FILE *f)
{
       USART_SendData(USART1, (uint8_t) ch);

       while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {}

   return ch;
}
int GetKey (void)  {

   while (!(USART1->SR & USART_FLAG_RXNE));

   return ((int)(USART1->DR & 0x1FF));
}
*/

/**
 * @brief 串口初始化函数
 * @param bound: 波特率
 * @return 无
 * @note 根据USART1_ENABLE、USART2_ENABLE、USART3_ENABLE宏定义选择初始化对应串口
 */
void Serial_Iint(u32 bound) {
#if USART1_ENABLE
  // GPIO端口设置
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  // 使能USART1，GPIOA时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

  // USART1_TX   GPIOA.9
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; // PA.9
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);          // 初始化GPIOA.9

  // USART1_RX  GPIOA.10初始化
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;            // PA10
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 浮空输入
  GPIO_Init(GPIOA, &GPIO_InitStructure);                // 初始化GPIOA.10

  // Usart1 NVIC 配置
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 抢占优先级1
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;        // 子优先级0
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           // IRQ通道使能
  NVIC_Init(&NVIC_InitStructure); // 根据指定的参数初始化VIC寄存器

  // USART 初始化设置

  USART_InitStructure.USART_BaudRate = bound; // 串口波特率
  USART_InitStructure.USART_WordLength =
      USART_WordLength_8b;                               // 字长为8位数据格式
  USART_InitStructure.USART_StopBits = USART_StopBits_1; // 一个停止位
  USART_InitStructure.USART_Parity = USART_Parity_No;    // 无奇偶校验位
  USART_InitStructure.USART_HardwareFlowControl =
      USART_HardwareFlowControl_None; // 无硬件数据流控制
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 收发模式

  USART_Init(USART1, &USART_InitStructure);      // 初始化串口1
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // 开启串口接受中断
  USART_Cmd(USART1, ENABLE);                     // 使能串口1

#elif USART2_ENABLE
  GPIO_InitTypeDef gpio_initstruct;
  USART_InitTypeDef usart_initstruct;
  NVIC_InitTypeDef nvic_initstruct;

  // 使能GPIOA和USART2时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

  // PA2 TXD
  gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_PP;
  gpio_initstruct.GPIO_Pin = GPIO_Pin_2;
  gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &gpio_initstruct);

  // PA3 RXD
  gpio_initstruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  gpio_initstruct.GPIO_Pin = GPIO_Pin_3;
  gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &gpio_initstruct);

  // USART2初始化
  usart_initstruct.USART_BaudRate = bound;
  usart_initstruct.USART_HardwareFlowControl =
      USART_HardwareFlowControl_None;                          // 无硬件流控
  usart_initstruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 接收和发送
  usart_initstruct.USART_Parity = USART_Parity_No;             // 无校验
  usart_initstruct.USART_StopBits = USART_StopBits_1;          // 1位停止位
  usart_initstruct.USART_WordLength = USART_WordLength_8b;     // 8位数据位
  USART_Init(USART2, &usart_initstruct);

  USART_Cmd(USART2, ENABLE); // 使能串口

  USART_ITConfig(USART2, USART_IT_RXNE, ENABLE); // 使能接收中断

  // NVIC配置
  nvic_initstruct.NVIC_IRQChannel = USART2_IRQn;
  nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
  nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 1;
  nvic_initstruct.NVIC_IRQChannelSubPriority = 0;
  NVIC_Init(&nvic_initstruct);

#elif USART3_ENABLE
  NVIC_InitTypeDef NVIC_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;

  // 使能GPIOB和USART3时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // GPIOB时钟
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

  USART_DeInit(USART3);                      // 复位串口
                                             // USART3_TX   PB10
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; // PB10
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);          // 初始化PB10

  // USART3_RX  PB11
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 浮空输入
  GPIO_Init(GPIOB, &GPIO_InitStructure);                // 初始化PB11

  // USART3初始化
  USART_InitStructure.USART_BaudRate = bound; // 一般设置为9600
  USART_InitStructure.USART_WordLength =
      USART_WordLength_8b;                               // 字长为8位数据格式
  USART_InitStructure.USART_StopBits = USART_StopBits_1; // 一个停止位
  USART_InitStructure.USART_Parity = USART_Parity_No;    // 无奇偶校验位
  USART_InitStructure.USART_HardwareFlowControl =
      USART_HardwareFlowControl_None; // 无硬件数据流控制
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 收发模式

  USART_Init(USART3, &USART_InitStructure); // 初始化串口3

  USART_Cmd(USART3, ENABLE);                     // 使能串口
                                                 // 使能接收中断
  USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); // 开启中断

  // NVIC配置
  NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 抢占优先级1
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        // 子优先级3
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           // IRQ通道使能
  NVIC_Init(&NVIC_InitStructure); // 根据指定的参数初始化VIC寄存器
#endif
}

/**
 * @brief 发送单个字节
 * @param Byte: 要发送的字节
 * @return 无
 */
void Serial_SendByte(uint8_t Byte) {
  USART_SendData(SEND_USART, Byte);
  while (USART_GetFlagStatus(SEND_USART, USART_FLAG_TXE) == RESET)
    ; // 等待发送完成
}

/**
 * @brief 发送数组
 * @param Array: 要发送的数组指针
 * @param Length: 数组长度
 * @return 无
 */
void Serial_SendArray(uint8_t *Array, uint16_t Length) {
  uint16_t i;
  for (i = 0; i < Length; i++) {
    Serial_SendByte(Array[i]);
  }
}

/**
 * @brief 发送字符串
 * @param String: 要发送的字符串指针
 * @return 无
 */
void Serial_SendString(char *String) {
  uint8_t i;
  for (i = 0; String[i] != '\0'; i++) {
    Serial_SendByte(String[i]);
  }
}

/**
 * @brief 计算幂函数
 * @param X: 底数
 * @param Y: 指数
 * @return 计算结果
 */
uint32_t Serial_Pow(uint32_t X, uint32_t Y) {
  uint32_t Result = 1;
  while (Y--) {
    Result *= X;
  }
  return Result;
}

/**
 * @brief 发送数字
 * @param Number: 要发送的数字
 * @param Length: 数字长度
 * @return 无
 */
void Serial_SendNumber(uint32_t Number, uint8_t Length) {
  uint8_t i;
  for (i = 0; i < Length; i++) {
    Serial_SendByte(Number / Serial_Pow(10, Length - i - 1) % 10 + '0');
  }
}

/**
 * @brief 格式化发送函数
 * @param format: 格式化字符串
 * @param ...: 可变参数
 * @return 无
 */
void Serial_Printf(char *format, ...) {
  char String[100];
  va_list arg;
  va_start(arg, format);
  vsprintf(String, format, arg);
  va_end(arg);
  Serial_SendString(String);
}

/**
 * @brief 发送数据包
 * @param 无
 * @return 无
 */
void Serial_SendPacket(void) { Serial_SendArray(Seria1_TxPacket, Packet_Len); }

/**
 * @brief 获取接收标志位
 * @param 无
 * @return 接收标志位状态
 */
uint8_t Serial_GetRxFlag(void) {
  if (Serial_RxFlag == 1) {
    Serial_RxFlag = 0;
    return 1;
  }
  return 0;
}

/**
 * @brief 在线计数器
 */
extern u16 online_count;

/**
 * @brief USART1中断处理函数
 * @param 无
 * @return 无
 * @note 处理USART1接收中断，实现数据包的接收
 */
#if USART1_ENABLE
void USART1_IRQHandler(void) {
  static uint8_t RxState = 0;   // 接收状态
  static uint8_t pRxPacket = 0; // 接收数据包指针
  if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
    uint8_t RxData = USART_ReceiveData(USART1); // 读取接收数据

    if (RxState == 0) // 等待起始符
    {
      if (RxData == '@' && Serial_RxFlag == 0) {
        RxState = 1;   // 进入接收数据状态
        pRxPacket = 0; // 重置数据包指针
      }
    } else if (RxState == 1) // 接收数据
    {
      if (RxData == '\r') // 收到回车符
      {
        RxState = 2; // 进入等待换行符状态
      } else {
        Serial_RxPacket[pRxPacket] = RxData; // 存储数据
        pRxPacket++;
      }
    } else if (RxState == 2) // 等待换行符
    {
      if (RxData == '\n') // 收到换行符
      {
        RxState = 0;                       // 重置状态
        Serial_RxPacket[pRxPacket] = '\0'; // 添加字符串结束符
        Serial_RxFlag = 1;                 // 设置接收标志位
      }
    }

    USART_ClearITPendingBit(USART1, USART_IT_RXNE); // 清除中断标志位
  }
}

/**
 * @brief USART2中断处理函数
 * @param 无
 * @return 无
 * @note 处理USART2接收中断，实现数据包的接收
 */
#elif USART2_ENABLE
void USART2_IRQHandler(void) {
  static uint8_t RxState = 0;   // 接收状态
  static uint8_t pRxPacket = 0; // 接收数据包指针
  if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET) {
    uint8_t RxData = USART_ReceiveData(USART2); // 读取接收数据

    if (RxState == 0) // 等待起始符
    {
      if (RxData == '@' && Serial_RxFlag == 0) {
        RxState = 1;   // 进入接收数据状态
        pRxPacket = 0; // 重置数据包指针
      }
    } else if (RxState == 1) // 接收数据
    {
      if (RxData == '\r') // 收到回车符
      {
        RxState = 2; // 进入等待换行符状态
      } else {
        Serial_RxPacket[pRxPacket] = RxData; // 存储数据
        pRxPacket++;
      }
    } else if (RxState == 2) // 等待换行符
    {
      if (RxData == '\n') // 收到换行符
      {
        RxState = 0;                       // 重置状态
        Serial_RxPacket[pRxPacket] = '\0'; // 添加字符串结束符
        Serial_RxFlag = 1;                 // 设置接收标志位
      }
    }

    USART_ClearITPendingBit(USART2, USART_IT_RXNE); // 清除中断标志位
  }
}

/**
 * @brief USART3中断处理函数
 * @param 无
 * @return 无
 * @note 处理USART3接收中断，实现数据包的接收
 */
#elif USART3_ENABLE
void USART3_IRQHandler(void) {
  static uint8_t RxState = 0;   // 接收状态
  static uint8_t pRxPacket = 0; // 接收数据包指针
  if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET) {
    uint8_t RxData = USART_ReceiveData(USART3); // 读取接收数据

    if (RxState == 0) // 等待起始符
    {
      if (RxData == '@' && Serial_RxFlag == 0) {
        RxState = 1;   // 进入接收数据状态
        pRxPacket = 0; // 重置数据包指针
      }
    } else if (RxState == 1) // 接收数据
    {
      if (RxData == '\r') // 收到回车符
      {
        RxState = 2; // 进入等待换行符状态
      } else {
        Serial_RxPacket[pRxPacket] = RxData; // 存储数据
        pRxPacket++;
      }
    } else if (RxState == 2) // 等待换行符
    {
      if (RxData == '\n') // 收到换行符
      {
        RxState = 0;                       // 重置状态
        Serial_RxPacket[pRxPacket] = '\0'; // 添加字符串结束符
        Serial_RxFlag = 1;                 // 设置接收标志位
      }
    }

    USART_ClearITPendingBit(USART3, USART_IT_RXNE); // 清除中断标志位
  }
}

#endif

/***************************** 结束 *****************************/