/**
 * stmflash.h
 * STM32 FLASH操作驱动头文件
 * 功能：提供FLASH的读写、擦除等操作函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 */

#ifndef __STMFLASH_H__
#define __STMFLASH_H__

#include "sys.h"  

/**
 * @brief Flash相关数组
 */
extern u16 A_Parameter[10], B_Parameter[10], Flash_Parameter[10];

/**
 * @brief FLASH配置参数
 */
#define STM32_FLASH_SIZE 64  			// 所选STM32的FLASH容量大小(单位为K)
#define STM32_FLASH_WREN 1              // 使能FLASH写入(0，不使能;1，使能)

/**
 * @brief FLASH地址定义
 */
#define STM32_FLASH_BASE 0x08000000  	// STM32 FLASH的起始地址
#define FLASH_SAVE_ADDR  0X0800FC00  	// 设置FLASH 保存地址(必须为偶数，且其值要大于本代码所占用FLASH的大小+0X08000000)
#define FLASH_SAVE_ADDRD 0X0800EC00    // 备用FLASH保存地址

/**
 * @brief FLASH解锁键值
 */
#define FLASH_KEY1               0X45670123
#define FLASH_KEY2               0XCDEF89AB

/**
 * @brief FLASH解锁
 * @param 无
 * @return 无
 */
void STMFLASH_Unlock(void);			   	// FLASH解锁

/**
 * @brief FLASH上锁
 * @param 无
 * @return 无
 */
void STMFLASH_Lock(void);			   	// FLASH上锁

/**
 * @brief 获得FLASH状态
 * @param 无
 * @return 状态值：0-操作完成，1-忙，2-编程错误，3-写保护错误
 */
u8 STMFLASH_GetStatus(void);		   	// 获得状态

/**
 * @brief 等待FLASH操作结束
 * @param time: 超时时间
 * @return 状态值：0-操作完成，1-忙，2-编程错误，3-写保护错误，0xff-超时
 */
u8 STMFLASH_WaitDone(u16 time);		   	// 等待操作结束

/**
 * @brief 擦除FLASH页
 * @param paddr: 页地址
 * @return 执行情况：0-成功，其他-错误码
 */
u8 STMFLASH_ErasePage(u32 paddr);	   	// 擦除页

/**
 * @brief 写入半字(16位)
 * @param faddr: 写入地址
 * @param dat: 要写入的数据
 * @return 执行情况：0-成功，其他-错误码
 */
u8 STMFLASH_WriteHalfWord(u32 faddr, u16 dat);	// 写入半字

/**
 * @brief 读取半字(16位)
 * @param faddr: 读取地址
 * @return 读取的数据
 */
u16 STMFLASH_ReadHalfWord(u32 faddr);	    // 读出半字  

/**
 * @brief 指定地址开始写入指定长度的数据
 * @param WriteAddr: 写入起始地址
 * @param DataToWrite: 要写入的数据
 * @param Len: 数据长度
 * @return 无
 */
void STMFLASH_WriteLenByte(u32 WriteAddr, u32 DataToWrite, u16 Len);	// 指定地址开始写入指定长度的数据

/**
 * @brief 指定地址开始读取指定长度数据
 * @param ReadAddr: 读取起始地址
 * @param Len: 数据长度
 * @return 读取的数据
 */
u32 STMFLASH_ReadLenByte(u32 ReadAddr, u16 Len);				        // 指定地址开始读取指定长度数据

/**
 * @brief 从指定地址开始写入指定长度的数据
 * @param WriteAddr: 写入起始地址
 * @param pBuffer: 数据指针
 * @param NumToWrite: 半字(16位)数
 * @return 无
 */
void STMFLASH_Write(u32 WriteAddr, u16 *pBuffer, u16 NumToWrite);		// 从指定地址开始写入指定长度的数据

/**
 * @brief 从指定地址开始读出指定长度的数据
 * @param ReadAddr: 读取起始地址
 * @param pBuffer: 数据指针
 * @param NumToRead: 半字(16位)数
 * @return 无
 */
void STMFLASH_Read(u32 ReadAddr, u16 *pBuffer, u16 NumToRead);    		// 从指定地址开始读出指定长度的数据

/**
 * @brief 测试写入
 * @param WriteAddr: 写入地址
 * @param WriteData: 要写入的数据
 * @return 无
 */
void Test_Write(u32 WriteAddr, u16 WriteData);	// 测试写入

/**
 * @brief 读取Flash数据
 * @param 无
 * @return 无
 */
void Flash_Read(void);	// 读取Flash数据

/**
 * @brief 写入Flash数据
 * @param 无
 * @return 无
 */
void Flash_Write(void);	// 写入Flash数据

#endif

/***************************** 结束 *****************************/


