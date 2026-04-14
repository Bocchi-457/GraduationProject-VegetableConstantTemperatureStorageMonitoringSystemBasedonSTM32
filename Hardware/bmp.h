/**
 * bmp.h
 * OLED显示图片数据头文件
 * 功能：存储OLED显示所需的图片数据
 * 版本：V1.0
 * 测试硬件：单片机STM32F103RCT6,晶振72M  单片机工作电压3.3V或5V
 * 作者：xiao冯@ShenZhen QDtech co.,LTD
 * 公司网站:www.qdtech.net
 * 淘宝网站：http://qdtech.taobao.com
 * 邮箱:QDtech2008@gmail.com
 * Skype:QDtech2008
 * 技术交流QQ群:324828016
 * 创建日期:2018/6/6
 * 项目：蔬菜恒温库监控系统
 *
 * 版权所有，盗版必究。
 * Copyright(C) 深圳市全动电子技术有限公司 2009-2019
 * All rights reserved
 */

/****************************************************************************************************
 * 电源接线：
 *      5V  接DC 5V电源
 *     GND  接地
 *
 * OLED屏数据线接线：
 * 本模块数据总线类型为IIC
 *     SCL  接PB8    // IIC时钟信号
 *     SDA  接PB9    // IIC数据信号
 *
 * OLED屏控制线接线：
 * 本模块数据总线类型为IIC，不需要接控制信号线
 *
 * 触摸屏接线：
 * 本模块本身不带触摸，不需要接触摸屏线
 **************************************************************************************************/

/****************************************************************************************************
 * @attention
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, QD electronic SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 **************************************************************************************************/

/**
 * 存储图片数据，图片大小为64*32像素
 * 数据格式：每8个像素点占用一个字节，从左到右，从上到下
 */

#ifndef __BMP_H
#define __BMP_H

/**
 * WiFi状态图标 - 16x16像素
 * 用于在OLED右上角显示WiFi状态
 * 位置：x=112~127, y=0~15（右上角）
 *
 * 数据存储格式：按页存储，页内按列
 * - 第0页（像素y=0~7）：16字节，每字节代表一列8个垂直像素（bit0=y0顶部,
 * bit7=y7底部）
 * - 第1页（像素y=8~15）：16字节，每字节代表一列8个垂直像素（bit0=y8顶部,
 * bit7=y15底部）
 *
 * 标准WiFi图标样式（细圆弧，1像素宽）：
 * - 底部圆点（列7~8, 像素y=14）
 * - 圆弧1（最小，列7~8, 像素y=11）
 * - 圆弧2（中等，列6~10, 跨越页0和页1，正U形）
 * - 圆弧3（最大，列4~11, 像素y=1~5，正U形）
 *
 * 正U形圆弧：中间y小（靠近顶部），两边y大（靠近底部）
 */

/* WiFi已连接图标 - 底部点 + 3条圆弧*/
unsigned char WIFI_CONNECTED[] = {
    0x60, 0x70, 0x38, 0x18, 0x8c, 0xcc, 0xee, 0x66, 0x66, 0xee, 0xcc,
    0x8c, 0x18, 0x38, 0x70, 0x60, 0x00, 0x00, 0x00, 0x03, 0x03, 0x09,
    0x0c, 0x66, 0x66, 0x0c, 0x09, 0x03, 0x03, 0x00, 0x00, 0x00};

/* WiFi断开图标 - WiFi图标 + 斜杠（左上到右下）*/
unsigned char WIFI_DISCONNECTED[] = {
    0x66, 0x7e, 0x3c, 0x38, 0x7c, 0xec, 0xce, 0xe6, 0x66, 0xce, 0xcc,
    0x8c, 0x18, 0x38, 0x70, 0x60, 0x00, 0x00, 0x00, 0x03, 0x03, 0x09,
    0x0d, 0x67, 0x67, 0x0e, 0x1d, 0x3b, 0x73, 0x60, 0x00, 0x00};

/* WiFi重连动画帧1 - 只有底部圆点 */
unsigned char WIFI_ANIM_FRAME1[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* WiFi重连动画帧2 - 底部点 + 小圆弧（圆弧1）*/
unsigned char WIFI_ANIM_FRAME2[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
    0x0c, 0x66, 0x66, 0x0c, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00};

/* WiFi重连动画帧3 - 底部点 + 小圆弧 + 中圆弧（圆弧1+2）*/
unsigned char WIFI_ANIM_FRAME3[] = {
    0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0x60, 0x60, 0xe0, 0xc0,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x09,
    0x0c, 0x66, 0x66, 0x0c, 0x09, 0x03, 0x03, 0x00, 0x00, 0x00};

#endif
