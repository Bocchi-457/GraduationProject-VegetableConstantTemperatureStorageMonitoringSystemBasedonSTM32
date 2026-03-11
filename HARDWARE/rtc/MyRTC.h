/**
 * MyRTC.h
 * RTC实时时钟驱动头文件
 * 功能：提供RTC初始化、时间设置和读取函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 * 作者：蔬菜恒温库监控系统开发团队
 * 创建日期：2026-03-08
 * 项目：蔬菜恒温库监控系统
 */

#ifndef __MYRTC_H
#define __MYRTC_H

#include "sys.h"

/**
 * @brief 时间设置数组
 * @note 格式：[年, 月, 日, 时, 分, 秒]
 */
extern uint16_t Set_time[6];

/**
 * @brief 时间读取数组
 * @note 格式：[年, 月, 日, 时, 分, 秒]
 */
extern uint16_t Read_time[6];

/**
 * @brief 日历结构体
 * @note 包含时间和日期信息
 */
typedef struct 
{
	vu8 hour;    // 小时
	vu8 min;     // 分钟
	vu8 sec;     // 秒钟          
	//公历日月年周
	vu16 w_year;  // 年份
	vu8  w_month; // 月份
	vu8  w_date;  // 日期
	vu8  week;    // 星期         
}_calendar_obj;	

/**
 * @brief 日历结构体实例
 * @note 存储当前时间和日期
 */
extern _calendar_obj calendar;   //日历结构体

/**
 * @brief 闹钟1结构体
 * @note 存储闹钟1的时间设置
 */
extern _calendar_obj Alarm1;   //闹钟结构体

/**
 * @brief 闹钟2结构体
 * @note 存储闹钟2的时间设置
 */
extern _calendar_obj Alarm2;   //闹钟结构体

/**
 * @brief 闹钟3结构体
 * @note 存储闹钟3的时间设置
 */
extern _calendar_obj Alarm3;   //闹钟结构体

/**
 * @brief 初始化RTC
 * @param 无
 * @return 无
 * @note 使用LSE作为RTC时钟源
 */
void MyRTC_Init(void);

/**
 * @brief 设置RTC时间
 * @param 无
 * @return 无
 * @note 使用Set_time数组中的值设置时间
 */
void MyRTC_SetTime(void);

/**
 * @brief 读取RTC时间
 * @param 无
 * @return 无
 * @note 读取的时间存储在Read_time数组中
 */
void MyRTC_ReadTime(void);

#endif

/***************************** 结束 *****************************/
