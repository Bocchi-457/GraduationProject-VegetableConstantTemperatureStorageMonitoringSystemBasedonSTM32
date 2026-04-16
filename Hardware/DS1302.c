/**
 * DS1302.c
 * DS1302实时时钟驱动文件
 * 实现DS1302实时时钟的初始化、读写操作和时间显示
 * 版本：V1.0
 * MCU：STM32F103C8T6
 */

#include "DS1302.h"
#include "OLED.h"
#include "delay.h"
#include "stdio.h"

u16 rtctime; // 时间变量

/**
 * DS1302控制命令定义
 */
#define WRITE_SECOND 0x80     // 写入秒
#define WRITE_MINUTE 0x82     // 写入分
#define WRITE_HOUR 0x84       // 写入时
#define WRITE_DAY 0x86        // 写入日
#define WRITE_MONTH 0x88      // 写入月
#define WRITE_WEEK 0x8A       // 写入周
#define WRITE_YEAR 0x8C       // 写入年
#define WRITE_TIMER_FLAG 0xC0 // 写入定时器标志

#define READ_SECOND 0x81     // 读取秒
#define READ_MINUTE 0x83     // 读取分
#define READ_HOUR 0x85       // 读取时
#define READ_DAY 0x87        // 读取日
#define READ_MONTH 0x89      // 读取月
#define READ_WEEK 0x8B       // 读取周
#define READ_YEAR 0x8D       // 读取年
#define READ_TIMER_FLAG 0xC1 // 读取定时器标志
#define WRITE_PROTECT 0x8E   // 写入保护

/**
 * 全局变量定义
 */
_calendar_obj calendar;             // 时钟结构体
_calendar_obj Alarm1 = {10, 48, 0}; // 闹钟1结构体
_calendar_obj Alarm2 = {13, 00, 0}; // 闹钟2结构体
_calendar_obj Alarm3 = {20, 0, 0};  // 闹钟3结构体

_next_obj next; // 下一个时间结构体

// 月份数据表，用于计算星期
u8 const table_week[12] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5}; // 月修正数据表

/**
 * 全局变量声明
 */
extern u16 rtctime;     // 时间变量
char time_str[50];      // 时间字符串缓冲区
u8 BCD2HEX(u8 bcd_data) // BCDtoHEX
{
  u8 temp;
  temp = (bcd_data / 16 * 10 + bcd_data % 16);
  return temp;
}
u8 HEX2BCD(u8 hex_data) // HEXtoBCD
{
  u8 temp;
  temp = (hex_data / 10 * 16 + hex_data % 10);
  return temp;
}
/**
 * @brief DS1302写一个字节
 * @param addr 地址
 * @param dat 数据
 * @note 串行发送地址、数据，先发低位，在上升沿发送
 */
void Ds1302_Write_Byte(u8 addr, u8 dat) {
  u8 i;
  DS1302_IO_OUT(); // 数据端口定义为输出
  CE = 1;
  delay_us(10);
  SCLK = 0;
  delay_us(10);
  for (i = 0; i < 8; i++) {
    if (addr & 0x01) {
      DIO = 1;
    } else {
      DIO = 0;
    }
    addr = addr >> 1;
    SCLK = 1;
    delay_us(10);
    SCLK = 0;
    delay_us(10);
  }
  for (i = 0; i < 8; i++) // 写入数据：dat
  {
    if (dat & 0x01) {
      DIO = 1;
    } else {
      DIO = 0;
    }
    dat = dat >> 1;
    SCLK = 1;
    delay_us(10);
    SCLK = 0;
    delay_us(10);
  }
  CE = 0;
  ; // 停止DS1302总线
  delay_us(10);
}
/**
 * @brief DS1302读一个字节
 * @param addr 地址
 * @return 读取的数据
 * @note 串行读取数据，先发低位，在下降沿发送
 */
u8 Ds1302_Read_Byte(u8 addr) {
  u8 i;
  u8 temp = 1;
  CE = 1;
  delay_us(10);
  for (i = 0; i < 8; i++) {
    SCLK = 0;
    delay_us(10);
    if (addr & 0x01) {
      DIO = 1;
    } else {
      DIO = 0;
    }
    addr = addr >> 1;
    SCLK = 1;
    delay_us(10);
  }
  DS1302_IO_IN(); // 数据端口定义为输入
  for (i = 0; i < 8; i++) {
    temp = temp >> 1; // 输出数据：temp
    SCLK = 0;
    delay_us(10);
    if (DIO_IN) {
      temp |= 0x80;
    }
    SCLK = 1;
    delay_us(10);
  }
  DS1302_IO_OUT(); // 数据端口定义为输出
  SCLK = 0;
  delay_us(10);
  CE = 0; // 停止DS1302总线
  delay_us(10);
  return temp;
}
/**
 * @brief 计算星期几
 * @param year 年
 * @param month 月
 * @param day 日
 * @return 星期号（0-6）
 * @note 输入公历日期得到星期，只允许1901-2099年
 */
u8 RTC_Get_Week(u16 year, u8 month, u8 day) {
  u16 temp2;
  u8 yearH, yearL;
  yearH = year / 100;
  yearL = year % 100; // 如果为21世纪,年份数加100
  if (yearH > 19)
    yearL += 100; // 所过闰年数只算1900年之后的
  temp2 = yearL + yearL / 4;
  temp2 = temp2 % 7;
  temp2 = temp2 + day + table_week[month - 1];
  if (yearL % 4 == 0 && month < 3)
    temp2--;
  return (temp2 % 7);
}
/**
 * @brief 向DS1302写入时钟数据
 * @param year 年
 * @param mon 月
 * @param day 日
 * @param hour 时
 * @param min 分
 * @param sec 秒
 */
void RTC_Set(u16 year, u8 mon, u8 day, u8 hour, u8 min, u8 sec) {
  u8 WR_week;
  u8 WR_yearL = 0;
  if (year >= 2000) {
    WR_yearL = year - 2000;
  }
  WR_week = RTC_Get_Week(year, mon, day); // 根据写入的日期算星期几
  Ds1302_Write_Byte(WRITE_PROTECT, 0x00); // 关闭写保护
  Ds1302_Write_Byte(WRITE_SECOND, 0x80);  // 暂停
  Ds1302_Write_Byte(WRITE_YEAR, HEX2BCD(WR_yearL)); // 年
  Ds1302_Write_Byte(WRITE_MONTH, HEX2BCD(mon));     // 月
  Ds1302_Write_Byte(WRITE_DAY, HEX2BCD(day));       // 日
  Ds1302_Write_Byte(WRITE_HOUR, HEX2BCD(hour));     // 时
  Ds1302_Write_Byte(WRITE_MINUTE, HEX2BCD(min));    // 分
  Ds1302_Write_Byte(WRITE_SECOND, HEX2BCD(sec));    // 秒
  Ds1302_Write_Byte(WRITE_WEEK, HEX2BCD(WR_week));  // 周
  Ds1302_Write_Byte(WRITE_PROTECT, 0x80);           // 打开写保护
}

/**
 * @brief 从DS1302读出时钟数据
 */
void RTC_Get(void) {
  //	u8  i,tmp;
  calendar.w_year = BCD2HEX(Ds1302_Read_Byte(READ_YEAR));       // 年
  calendar.w_month = BCD2HEX(Ds1302_Read_Byte(READ_MONTH));     // 月
  calendar.w_date = BCD2HEX(Ds1302_Read_Byte(READ_DAY));        // 日
  calendar.hour = BCD2HEX(Ds1302_Read_Byte(READ_HOUR));         // 时
  calendar.min = BCD2HEX(Ds1302_Read_Byte(READ_MINUTE));        // 分
  calendar.sec = BCD2HEX(Ds1302_Read_Byte(READ_SECOND) & 0x7F); // 秒
  calendar.week = BCD2HEX(Ds1302_Read_Byte(READ_WEEK));         // 周
  calendar.w_year = calendar.w_year + 2000;
}

/**
 * @brief DS1302初始化
 */
void Ds1302_Init(void) {
  IO_Init();                             // GPIO初始化
  CE = 0;                                // RST脚置低
  SCLK = 0;                              // SCK脚置低
  Ds1302_Write_Byte(WRITE_SECOND, 0x00); // 开始
  // 取消注释以下行来设置时间，设置完成后重新注释
  // RTC_Set(2026, 4, 3, 13, 43, 0);  // 年月日时分秒
}

//*******************以下UTC时间计算部分函数*****************
/**
 * @brief 判断是否是闰年
 * @param year 年份
 * @return 1=是闰年，0=不是闰年
 */
u8 Is_Leap_Year(u16 year) {
  if (year % 4 == 0) // 必须能被4整除
  {
    if (year % 100 == 0) {
      if (year % 400 == 0)
        return 1; // 如果以00结尾,还要能被400整除
      else
        return 0;
    } else
      return 1;
  } else
    return 0;
}

void IO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                             RCC_APB2Periph_GPIOC,
                         ENABLE);                  // 使能PA,PB,PC端口时钟
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为50MHz
  // PC端口初始化
  GPIO_InitStructure.GPIO_Pin =
      GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15; // 设置PC10~PC12端口推挽输出
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
  GPIO_Init(GPIOC, &GPIO_InitStructure);
}
// void TIME(void)
// {
//   RTC_Get();	//获取时间
// 	if(rtctime!=calendar.sec)
// //一分钟打印一次
// 	{
// 		rtctime=calendar.sec;
// 		if((calendar.hour<=24)&&(calendar.min<=60)&&(calendar.week<=7)&&(calendar.w_date<=31)&&(calendar.w_month<=12))//检测成功
// 	  {
// 				sprintf(time_str,"%04d-%02d-%02d
// ",calendar.w_year,calendar.w_month,calendar.w_date); 				OLED_ShowString(0,2,(u8
// *)time_str,16);

// 				sprintf(time_str,"%02d:%02d:%02d",calendar.hour,calendar.min,calendar.sec);
// 				OLED_ShowString(0,4,(u8 *)time_str,16);
// 				switch(calendar.week)
// 			 {
// 					case 0:  OLED_ShowCHinese(48+32,
// 4,102),OLED_ShowCHinese(48+16+32, 4,94),OLED_ShowCHinese(96+16, 4,101);
// break;//周日 					case 1:  OLED_ShowCHinese(48+32,
// 4,102),OLED_ShowCHinese(48+16+32, 4,94),OLED_ShowCHinese(96+16, 4,95);
// break;//周一 					case 2:  OLED_ShowCHinese(48+32,
// 4,102),OLED_ShowCHinese(48+16+32, 4,94),OLED_ShowCHinese(96+16, 4,96);
// break;//周二 					case 3:  OLED_ShowCHinese(48+32,
// 4,102),OLED_ShowCHinese(48+16+32, 4,94),OLED_ShowCHinese(96+16, 4,97);
// break;//周三 					case 4:  OLED_ShowCHinese(48+32,
// 4,102),OLED_ShowCHinese(48+16+32, 4,94),OLED_ShowCHinese(96+16, 4,98);
// break;//周四 					case 5:  OLED_ShowCHinese(48+32,
// 4,102),OLED_ShowCHinese(48+16+32, 4,94),OLED_ShowCHinese(96+16, 4,99);
// break;//周五 					case 6:  OLED_ShowCHinese(48+32,
// 4,102),OLED_ShowCHinese(48+16+32, 4,94),OLED_ShowCHinese(96+16, 4,100);
// break;//周六
// 			 }
// 		}
//         else OLED_ShowString(0,6,(u8 *)"shibai",16);

// 	 }
// }

void TIME(void) {
  RTC_Get(); // 获取时间
  // 每次都更新时间显示，不使用条件判断
  // 确保时间数据有效
  if ((calendar.hour <= 24) && (calendar.min <= 60) && (calendar.week <= 7) &&
      (calendar.w_date <= 31) && (calendar.w_month <= 12)) // 检测成功
  {
    sprintf(time_str, "%04d-%02d-%02d", calendar.w_year, calendar.w_month,
            calendar.w_date);
    OLED_ShowString(0, 3, (u8 *)time_str, 12); // 使用F6x8字体显示日期

    sprintf(time_str, "%02d:%02d:%02d", calendar.hour, calendar.min,
            calendar.sec);
    OLED_ShowString(0, 4, (u8 *)time_str, 12); // 使用F6x8字体显示时间
    switch (calendar.week) {
    case 0:
      OLED_ShowCHinese(80, 3, 102), OLED_ShowCHinese(96, 3, 94),
          OLED_ShowCHinese(112, 3, 101);
      break; // 周日
    case 1:
      OLED_ShowCHinese(80, 3, 102), OLED_ShowCHinese(96, 3, 94),
          OLED_ShowCHinese(112, 3, 95);
      break; // 周一
    case 2:
      OLED_ShowCHinese(80, 3, 102), OLED_ShowCHinese(96, 3, 94),
          OLED_ShowCHinese(112, 3, 96);
      break; // 周二
    case 3:
      OLED_ShowCHinese(80, 3, 102), OLED_ShowCHinese(96, 3, 94),
          OLED_ShowCHinese(112, 3, 97);
      break; // 周三
    case 4:
      OLED_ShowCHinese(80, 3, 102), OLED_ShowCHinese(96, 3, 94),
          OLED_ShowCHinese(112, 3, 98);
      break; // 周四
    case 5:
      OLED_ShowCHinese(80, 3, 102), OLED_ShowCHinese(96, 3, 94),
          OLED_ShowCHinese(112, 3, 99);
      break; // 周五
    case 6:
      OLED_ShowCHinese(80, 3, 102), OLED_ShowCHinese(96, 3, 94),
          OLED_ShowCHinese(112, 3, 100);
      break; // 周六
    }
  } else
    OLED_ShowString(0, 6, (u8 *)"shibai", 16);
}
