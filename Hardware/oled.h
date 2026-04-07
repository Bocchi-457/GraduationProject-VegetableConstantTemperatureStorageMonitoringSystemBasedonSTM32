//////////////////////////////////////////////////////////////////////////////////
// 本程序只供学习使用，未经作者许可，不得用于其它任何用途
// 测试硬件：单片机STM32F103RCT6,晶振72M  单片机工作电压3.3V或5V
// QDtech-OLED液晶驱动 for STM32
// xiao冯@ShenZhen QDtech co.,LTD
// 公司网站:www.qdtech.net
// 淘宝网站：http://qdtech.taobao.com
// 我司提供技术支持，任何技术问题欢迎随时交流学习
// 邮箱:QDtech2008@gmail.com
// Skype:QDtech2008
// 技术交流QQ群:324828016
// 创建日期:2018/6/6
// 版本：V1.0
// 版权所有，盗版必究。
// Copyright(C) 深圳市全动电子技术有限公司 2009-2019
// All rights reserved
/****************************************************************************************************
//=========================================电源接线================================================//
//      5V  接DC 5V电源
//     GND  接地
//======================================OLED屏数据线接线==========================================//
//本模块数据总线类型为IIC
//     SCL  接PB8    // IIC时钟信号
//     SDA  接PB9    // IIC数据信号
//======================================OLED屏控制线接线==========================================//
//本模块数据总线类型为IIC，不需要接控制信号线
//=========================================触摸屏接线=========================================//
//本模块本身不带触摸，不需要接触摸屏线
//============================================================================================//
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, QD electronic SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
**************************************************************************************************/
#ifndef __OLED_H
#define __OLED_H
#include "stdlib.h"
#include "sys.h"

/**
 * @defgroup OLED_Constants OLED显示模块常量定义
 * @{
 */

#define OLED_MODE 0     /**< OLED模式 */
#define SIZE 8          /**< 字体大小 */
#define XLevelL 0x00    /**< X轴低电平 */
#define XLevelH 0x10    /**< X轴高电平 */
#define Max_Column 128  /**< 最大列数 */
#define Max_Row 64      /**< 最大行数 */
#define Brightness 0xFF /**< 亮度 */
#define X_WIDTH 128     /**< X轴宽度 */
#define Y_WIDTH 64      /**< Y轴宽度 */

/**
 * @defgroup OLED_IIC_Pins OLED IIC端口定义
 * @{
 */
#define OLED_SCLK_Clr() GPIO_ResetBits(GPIOB, GPIO_Pin_8) /**< 清除SCLK信号 */
#define OLED_SCLK_Set() GPIO_SetBits(GPIOB, GPIO_Pin_8)   /**< 设置SCLK信号 */
#define OLED_SDIN_Clr() GPIO_ResetBits(GPIOB, GPIO_Pin_9) /**< 清除SDIN信号 */
#define OLED_SDIN_Set() GPIO_SetBits(GPIOB, GPIO_Pin_9)   /**< 设置SDIN信号 */
/** @} */

#define OLED_CMD 0  /**< 写命令 */
#define OLED_DATA 1 /**< 写数据 */

#define IIC_SLAVE_ADDR 0x78 /**< IIC从设备地址 */

/** @} */

/**
 * @defgroup OLED_Functions OLED显示模块函数
 * @{
 */

/**
 * @brief 向OLED写入一个字节
 * @param dat: 要写入的数据
 * @param cmd: 写入类型，OLED_CMD表示命令，OLED_DATA表示数据
 */
void OLED_WR_Byte(unsigned dat, unsigned cmd);

/**
 * @brief 开启OLED显示
 */
void OLED_Display_On(void);

/**
 * @brief 关闭OLED显示
 */
void OLED_Display_Off(void);

/**
 * @brief 初始化OLED显示模块
 */
void OLED_Init(void);

/**
 * @brief 清屏函数
 * @param dat: 清屏数据，0为全黑，1为全白
 */
void OLED_Clear(unsigned dat);

/**
 * @brief 在指定位置绘制点
 * @param x: X坐标
 * @param y: Y坐标
 * @param t: 点的状态，0为熄灭，1为点亮
 */
void OLED_DrawPoint(u8 x, u8 y, u8 t);

/**
 * @brief 填充指定区域
 * @param x1: 起始X坐标
 * @param y1: 起始Y坐标
 * @param x2: 结束X坐标
 * @param y2: 结束Y坐标
 * @param dot: 填充数据
 */
void OLED_Fill(u8 x1, u8 y1, u8 x2, u8 y2, u8 dot);

/**
 * @brief 在指定位置显示一个字符
 * @param x: X坐标
 * @param y: Y坐标
 * @param chr: 要显示的字符
 * @param Char_Size: 字符大小，16为16x16，12为12x12
 */
void OLED_ShowChar(u8 x, u8 y, u8 chr, u8 Char_Size);

/**
 * @brief 在指定位置显示数字
 * @param x: X坐标
 * @param y: Y坐标
 * @param num: 要显示的数字
 * @param len: 数字的位数
 * @param size: 字体大小
 */
void OLED_ShowNum(u8 x, u8 y, u32 num, u8 len, u8 size);

/**
 * @brief 在指定位置显示字符串
 * @param x: X坐标
 * @param y: Y坐标
 * @param p: 要显示的字符串
 * @param Char_Size: 字符大小
 */
void OLED_ShowString(u8 x, u8 y, u8 *p, u8 Char_Size);

/**
 * @brief 设置OLED显示位置
 * @param x: X坐标
 * @param y: Y坐标
 */
void OLED_Set_Pos(unsigned char x, unsigned char y);

/**
 * @brief 显示汉字
 * @param x: X坐标
 * @param y: Y坐标
 * @param no: 汉字在字库中的索引
 */
void OLED_ShowCHinese(u8 x, u8 y, u8 no);

/**
 * @brief 显示BMP图片
 * @param x0: 起始X坐标
 * @param y0: 起始Y坐标
 * @param x1: 结束X坐标
 * @param y1: 结束Y坐标
 * @param BMP: 图片数据
 */
void OLED_DrawBMP(unsigned char x0, unsigned char y0, unsigned char x1,
                  unsigned char y1, unsigned char BMP[]);

/**
 * @brief 延时50ms
 * @param Del_50ms: 延时次数
 */
void Delay_50ms(unsigned int Del_50ms);

/**
 * @brief 延时1ms
 * @param Del_1ms: 延时次数
 */
void Delay_1ms(unsigned int Del_1ms);

/**
 * @brief 填充整个屏幕
 * @param fill_Data: 填充数据
 */
void fill_picture(unsigned char fill_Data);

/**
 * @brief 显示图片
 */
void Picture(void);

/**
 * @brief IIC开始信号
 */
void IIC_Start1(void);

/**
 * @brief IIC停止信号
 */
void IIC_Stop1(void);

/**
 * @brief 写入IIC命令
 * @param IIC_Command: 要写入的命令
 */
void Write_IIC_Command(unsigned char IIC_Command);

/**
 * @brief 写入IIC数据
 * @param IIC_Data: 要写入的数据
 */
void Write_IIC_Data(unsigned char IIC_Data);

/**
 * @brief 写入IIC字节
 * @param IIC_Byte: 要写入的字节
 */
void Write_IIC_Byte(unsigned char IIC_Byte);

/**
 * @brief IIC等待应答
 */
void IIC_Wait_Ack1(void);

/** @} */
#endif