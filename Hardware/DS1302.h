/**
 * DS1302.h
 * DS1302实时时钟驱动头文件
 * 版本：V1.0
 * MCU：STM32F103C8T6
 */

#ifndef __DS1302_H
#define __DS1302_H
#include "sys.h"

// IO方向设置
#define DS1302_IO_IN()                                                         \
  {                                                                            \
    GPIOC->CRH &= 0xF0FFFFFF;                                                  \
    GPIOC->CRH |= 0x08000000;                                                  \
  } // 低八位引脚的PC14脚定义为输入

#define DS1302_IO_OUT()                                                        \
  {                                                                            \
    GPIOC->CRH &= 0xF0FFFFFF;                                                  \
    GPIOC->CRH |= 0x03000000;                                                  \
  } // 低八位引脚的PC14脚定义为输出

// IO操作函数
#define DIO_OUT PCout(14) // 数据端口	PC14
#define DIO_IN PCin(14)   // 数据端口	PC14
#define DIO PCout(14)     // PC14

#define CE PCout(15)   // PC15
#define SCLK PCout(13) // PC13

/**
 * 下一个时间结构体
 */
typedef struct {
  u8 sec;   // 秒
  u8 min;   // 分
  u8 hour;  // 时
  u8 day;   // 日
  u8 mon;   // 月
  u16 year; // 年
  u8 week;  // 周
} _next_obj;

extern _next_obj next; // 下一个时间结构体实例

/**
 * 日历结构体
 */
typedef struct {
  vu8 hour; // 时
  vu8 min;  // 分
  vu8 sec;  // 秒

  // 公历日月年周
  short int w_year; // 年
  vu8 w_month;      // 月
  vu8 w_date;       // 日
  vu8 week;         // 周
} _calendar_obj;

/**
 * 全局变量声明
 */
extern _calendar_obj calendar; // 日历结构体
extern _calendar_obj Alarm1;   // 闹钟1结构体
extern _calendar_obj Alarm2;   // 闹钟2结构体
extern _calendar_obj Alarm3;   // 闹钟3结构体
extern u16 rtctime;            // 时间变量

extern u32 RTC_sec_sum;     // 当前时间的总秒值
extern u32 Program_sec_sum; // 当前编程任务的总秒值,与RTC_sec_sum进行比较

/**
 * 函数声明
 */
void RTC_Set(u16 year, u8 mon, u8 day, u8 hour, u8 min, u8 sec); // 设置时间
void RTC_Get(void);                                              // 获取时间
void NEXT_Date(u8 day); // 计算下一个日期
void IO_Init(void);     // IO初始化
void TIME(void);        // 显示时间
u8 RTC_Pro_count(u16 syear, u8 smon, u8 sday, u8 hour, u8 min, u8 sec,
                 u8 mode); // 编程任务时间计算
u8 Pro_Get_time(u32 ttt);  // 编程模式无效时间时计算下次开始的日期
void Ds1302_Init(void);    // DS1302初始化

#endif
