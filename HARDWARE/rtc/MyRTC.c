/**
 * MyRTC.c
 * RTC实时时钟驱动实现文件
 * 功能：实现RTC初始化、时间设置和读取函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 * 作者：蔬菜恒温库监控系统开发团队
 * 创建日期：2026-03-08
 * 项目：蔬菜恒温库监控系统
 */

#include "stm32f10x.h"                  // Device header
#include "MyRTC.h"
#include <time.h>

/**
 * @brief 时间设置数组
 * @note 格式：[年, 月, 日, 时, 分, 秒]
 * @value 初始值：2025年2月11日 22:00:54
 */
uint16_t Set_time[6] = {2025, 2, 11, 22, 00, 54};

/**
 * @brief 时间读取数组
 * @note 格式：[年, 月, 日, 时, 分, 秒]
 * @value 初始值：全0
 */
uint16_t Read_time[6] = {0000, 00, 00, 00, 00, 00};

/**
 * @brief 日历结构体实例
 * @note 存储当前时间和日期
 */
_calendar_obj calendar;                            //时钟结构体 

/**
 * @brief 闹钟1结构体
 * @note 存储闹钟1的时间设置，初始小时为8点
 */
_calendar_obj Alarm1 = {8};  //闹钟结构体

/**
 * @brief 闹钟2结构体
 * @note 存储闹钟2的时间设置，初始小时为12点
 */
_calendar_obj Alarm2 = {12};  //闹钟结构体

/**
 * @brief 闹钟3结构体
 * @note 存储闹钟3的时间设置，初始小时为20点
 */
_calendar_obj Alarm3 = {20};  //闹钟结构体

/**
 * @brief 初始化RTC
 * @param 无
 * @return 无
 * @note 使用LSE作为RTC时钟源
 *       首次初始化时设置时间，之后从备份寄存器恢复
 */
void MyRTC_Init(void)
{
	// 使能PWR和BKP时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);
	
	// 允许访问备份寄存器
	PWR_BackupAccessCmd(ENABLE);
	
	// 检查是否首次初始化
	if (BKP_ReadBackupRegister(BKP_DR1) != 0xA5A5)
	{
		// 复位备份寄存器
		BKP_DeInit();
	
		// 使能LSE（外部低速晶振）
		RCC_LSEConfig(RCC_LSE_ON);
		// 等待LSE稳定
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
		
		// 设置RTC时钟源为LSE
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
		// 使能RTC时钟
		RCC_RTCCLKCmd(ENABLE);
		
		// 等待RTC寄存器同步
		RTC_WaitForSynchro();
		// 等待上一次操作完成
		RTC_WaitForLastTask();
		
		// 设置RTC预分频器（32768Hz / 32768 = 1Hz）
		RTC_SetPrescaler(32767);
		// 等待上一次操作完成
		RTC_WaitForLastTask();
		
		// 设置时间
		MyRTC_SetTime();
		
		// 写入备份寄存器，标记已初始化
		BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);
	}
	else
	{
		// 等待RTC寄存器同步
		RTC_WaitForSynchro();
		// 等待上一次操作完成
		RTC_WaitForLastTask();
	}
}

/**
 * @brief 初始化RTC（使用LSI时钟源）
 * @param 无
 * @return 无
 * @note 此函数已被注释，使用LSE作为时钟源
 */
/*
void MyRTC_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);
	
	PWR_BackupAccessCmd(ENABLE);
	
	if (BKP_ReadBackupRegister(BKP_DR1) != 0xA5A5)
	{
		BKP_DeInit();
	
		RCC_LSICmd(ENABLE);
		while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
		
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
		RCC_RTCCLKCmd(ENABLE);
		
		RTC_WaitForSynchro();
		RTC_WaitForLastTask();
		
		RTC_SetPrescaler(40000 - 1);
		RTC_WaitForLastTask();
		
		MyRTC_SetTime();
		
		BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);
	}
	else
	{
		RCC_LSICmd(ENABLE);
		while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
		
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
		RCC_RTCCLKCmd(ENABLE);
		
		RTC_WaitForSynchro();
		RTC_WaitForLastTask();
	}
}
*/

/**
 * @brief 设置RTC时间
 * @param 无
 * @return 无
 * @note 使用Set_time数组中的值设置时间
 *       计算从1970年1月1日到设置时间的秒数，并减去8小时的时区偏移
 */
void MyRTC_SetTime(void)
{
	time_t time_cnt;        // 时间计数器
	struct tm time_date;    // 时间结构体
	
	// 填充时间结构体
	time_date.tm_year = Set_time[0] - 1900;  // 年份从1900开始
	time_date.tm_mon = Set_time[1] - 1;      // 月份从0开始
	time_date.tm_mday = Set_time[2];         // 日期
	time_date.tm_hour = Set_time[3];         // 小时
	time_date.tm_min = Set_time[4];          // 分钟
	time_date.tm_sec = Set_time[5];          // 秒钟
	
	// 计算时间戳并减去8小时时区偏移
	time_cnt = mktime(&time_date) - 8 * 60 * 60;
	
	// 设置RTC计数器
	RTC_SetCounter(time_cnt);
	// 等待操作完成
	RTC_WaitForLastTask();
}

/**
 * @brief 读取RTC时间
 * @param 无
 * @return 无
 * @note 读取RTC计数器的值，加上8小时时区偏移，转换为本地时间
 *       结果存储在Read_time数组中
 */
void MyRTC_ReadTime(void)
{
	time_t time_cnt;        // 时间计数器
	struct tm time_date;    // 时间结构体
	
	// 读取RTC计数器并加上8小时时区偏移
	time_cnt = RTC_GetCounter() + 8 * 60 * 60;
	
	// 转换为本地时间
	time_date = *localtime(&time_cnt);
	
	// 填充Read_time数组
	Read_time[0] = time_date.tm_year + 1900;  // 年份
	Read_time[1] = time_date.tm_mon + 1;      // 月份
	Read_time[2] = time_date.tm_mday;         // 日期
	Read_time[3] = time_date.tm_hour;         // 小时
	Read_time[4] = time_date.tm_min;          // 分钟
	Read_time[5] = time_date.tm_sec;          // 秒钟
}

/***************************** 结束 *****************************/
