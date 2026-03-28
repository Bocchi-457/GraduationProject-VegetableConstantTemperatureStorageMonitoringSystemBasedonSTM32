/**
 * stmflash.c
 * STM32 FLASH操作驱动实现文件
 * 功能：实现FLASH的读写、擦除等操作函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 */

#include "stmflash.h"
#include "delay.h"
#include "sys.h"

/**
 * @brief Flash相关数组
 */
u16 A_Parameter[10], B_Parameter[10], Flash_Parameter[10];  // Flash相关数组

/**
 * @brief 解锁STM32的FLASH
 * @param 无
 * @return 无
 */
void STMFLASH_Unlock(void)
{
  FLASH->KEYR = FLASH_KEY1;  // 写入解锁序列
  FLASH->KEYR = FLASH_KEY2;
}

/**
 * @brief FLASH上锁
 * @param 无
 * @return 无
 */
void STMFLASH_Lock(void)
{
  FLASH->CR |= 1 << 7;  // 上锁
}

/**
 * @brief 得到FLASH状态
 * @param 无
 * @return 状态值：0-操作完成，1-忙，2-编程错误，3-写保护错误
 */
u8 STMFLASH_GetStatus(void)
{	
	u32 res;		
	res = FLASH->SR; 
	if(res & (1 << 0)) return 1;  // 忙
	else if(res & (1 << 2)) return 2;  // 编程错误
	else if(res & (1 << 4)) return 3;  // 写保护错误
	return 0;  // 操作完成
}
/**
 * @brief 等待FLASH操作结束
 * @param time: 超时时间
 * @return 状态值：0-操作完成，1-忙，2-编程错误，3-写保护错误，0xff-超时
 */
u8 STMFLASH_WaitDone(u16 time)
{
	u8 res;
	do
	{
		res = STMFLASH_GetStatus();
		if(res != 1) break;  // 非忙,无需等待了,直接退出
		delay_us(1);
		time--;
	 }while(time);
	 if(time == 0) res = 0xff;  // TIMEOUT
	 return res;
}
/**
 * @brief 擦除FLASH页
 * @param paddr: 页地址
 * @return 执行情况：0-成功，其他-错误码
 */
u8 STMFLASH_ErasePage(u32 paddr)
{
	u8 res = 0;
	res = STMFLASH_WaitDone(0X5FFF);  // 等待上次操作结束,>20ms    
	if(res == 0)
	{
		FLASH->CR |= 1 << 1;  // 页擦除
		FLASH->AR = paddr;  // 设置页地址 
		FLASH->CR |= 1 << 6;  // 开始擦除		  
		res = STMFLASH_WaitDone(0X5FFF);  // 等待操作结束,>20ms  
		if(res != 1)  // 非忙
		{
			FLASH->CR &= ~(1 << 1);  // 清除页擦除标志
		}
	}
	return res;
}

/**
 * @brief 读取指定地址的半字(16位数据)
 * @param faddr: 读地址(此地址必须为2的倍数!!)
 * @return 对应数据
 */
u16 STMFLASH_ReadHalfWord(u32 faddr)
{
	return *(vu16*)faddr; 
}
/**
 * @brief 不检查的写入
 * @param WriteAddr: 起始地址
 * @param pBuffer: 数据指针
 * @param NumToWrite: 半字(16位)数
 * @return 无
 */
void STMFLASH_Write_NoCheck(u32 WriteAddr, u16 *pBuffer, u16 NumToWrite)   
{  		  
	u16 i;
	for(i = 0; i < NumToWrite; i++)
	{
		FLASH_ProgramHalfWord(WriteAddr, pBuffer[i]);
	    WriteAddr += 2;  // 地址增加2
	}  
}

/**
 * @brief 从指定地址开始写入指定长度的数据
 * @param WriteAddr: 起始地址(此地址必须为2的倍数!!)
 * @param pBuffer: 数据指针
 * @param NumToWrite: 半字(16位)数(就是要写入的16位数据的个数.)
 * @return 无
 */
#if STM32_FLASH_SIZE < 256
#define STM_SECTOR_SIZE 1024 // 字节
#else 
#define STM_SECTOR_SIZE	2048
#endif		 
u16 STMFLASH_BUF[STM_SECTOR_SIZE / 2];  // 最多是2K字节

void STMFLASH_Write(u32 WriteAddr, u16 *pBuffer, u16 NumToWrite)	
{
	u32 secpos;   // 扇区地址
	u16 secoff;   // 扇区内偏移地址(16位字计算)
	u16 secremain; // 扇区内剩余地址(16位字计算)	   
  	u16 i;    
	u32 offaddr;   // 去掉0X08000000后的地址
	
	// 检查地址是否合法
	if(WriteAddr < STM32_FLASH_BASE || (WriteAddr >= (STM32_FLASH_BASE + 1024 * STM32_FLASH_SIZE)))
		return;  // 非法地址
	
	FLASH_Unlock();					// 解锁
	offaddr = WriteAddr - STM32_FLASH_BASE;  // 实际偏移地址
	secpos = offaddr / STM_SECTOR_SIZE;  // 扇区地址  0~127 for STM32F103RBT6
	secoff = (offaddr % STM_SECTOR_SIZE) / 2;  // 在扇区内的偏移(2个字节为基本单位)
	secremain = STM_SECTOR_SIZE / 2 - secoff;  // 扇区剩余空间大小   
	
	if(NumToWrite <= secremain) secremain = NumToWrite;  // 不大于该扇区范围
	
	while(1) 
	{	
		// 读出整个扇区的内容
		STMFLASH_Read(secpos * STM_SECTOR_SIZE + STM32_FLASH_BASE, STMFLASH_BUF, STM_SECTOR_SIZE / 2);
		
		// 校验数据
		for(i = 0; i < secremain; i++)
		{
			if(STMFLASH_BUF[secoff + i] != 0XFFFF) break;  // 需要擦除  	  
		}
		
		if(i < secremain)  // 需要擦除
		{
			// 擦除这个扇区
			FLASH_ErasePage(secpos * STM_SECTOR_SIZE + STM32_FLASH_BASE);
			
			// 复制数据
			for(i = 0; i < secremain; i++)
			{
				STMFLASH_BUF[i + secoff] = pBuffer[i];	  
			}
			
			// 写入整个扇区
			STMFLASH_Write_NoCheck(secpos * STM_SECTOR_SIZE + STM32_FLASH_BASE, STMFLASH_BUF, STM_SECTOR_SIZE / 2);
		}
		else
		{
			// 写已经擦除了的,直接写入扇区剩余区间
			STMFLASH_Write_NoCheck(WriteAddr, pBuffer, secremain);
		}
		
		if(NumToWrite == secremain) break;  // 写入结束了
		else  // 写入未结束
		{
			secpos++;  // 扇区地址增1
			secoff = 0;  // 偏移位置为0 	 
		    pBuffer += secremain;   // 指针偏移
			WriteAddr += secremain;  // 写地址偏移	   
		    NumToWrite -= secremain;  // 字节(16位)数递减
			
			if(NumToWrite > (STM_SECTOR_SIZE / 2))
				secremain = STM_SECTOR_SIZE / 2;  // 下一个扇区还是写不完
			else
				secremain = NumToWrite;  // 下一个扇区可以写完了
		} 	 
	};	
	
	FLASH_Lock();  // 上锁
}

/**
 * @brief 从指定地址开始读出指定长度的数据
 * @param ReadAddr: 起始地址
 * @param pBuffer: 数据指针
 * @param NumToRead: 半字(16位)数
 * @return 无
 */
void STMFLASH_Read(u32 ReadAddr, u16 *pBuffer, u16 NumToRead)    	
{
	u16 i;
	for(i = 0; i < NumToRead; i++)
	{
		pBuffer[i] = STMFLASH_ReadHalfWord(ReadAddr);  // 读取2个字节
		ReadAddr += 2;  // 偏移2个字节	
	}
}

/**
 * @brief 测试写入
 * @param WriteAddr: 起始地址
 * @param WriteData: 要写入的数据
 * @return 无
 */
void Test_Write(u32 WriteAddr, u16 WriteData)    	
{
	STMFLASH_Write(WriteAddr, &WriteData, 1);  // 写入一个字 
}


/**
 * @brief 从Flash读取指定数据
 * @param 无
 * @return 无
 */
void Flash_Read(void)
{
	// 从FLASH_SAVE_ADDR地址读取10个半字到A_Parameter数组
	STMFLASH_Read(FLASH_SAVE_ADDR, (u16*)A_Parameter, 10);
	
	// Flash未写入数据的时候，数据为65535
	if(A_Parameter[0] == 65535 && A_Parameter[1] == 65535 && A_Parameter[2] == 65535 && A_Parameter[3] == 65535)
	{
		// printf("FLASH暂时为空"); 
	}
	else  // Flash内有数据，进行读取
	{		
		// printf("%d,%d,%d,%d\r\n",A_Parameter[0],A_Parameter[1],A_Parameter[2],A_Parameter[3]);
	}
}	

/**
 * @brief 向Flash写入数据
 * @param 无
 * @return 无
 */
void Flash_Write(void)
{
	static unsigned int date = 0;  // 静态数据，不随函数进入把数据清除
	if(++date == 65535) date = 0;  // 数据自加，循环计数
	
	// 填充B_Parameter数组
	B_Parameter[0] = date; 		
	B_Parameter[1] = 2; 	
	B_Parameter[2] = 3; 	
	B_Parameter[3] = 4; 	
	
	// 写入数据到FLASH_SAVE_ADDR地址
	STMFLASH_Write(FLASH_SAVE_ADDR, (u16*)B_Parameter, 4);	
}

/***************************** 结束 *****************************/

