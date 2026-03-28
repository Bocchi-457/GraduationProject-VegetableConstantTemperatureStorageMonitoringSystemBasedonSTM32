/**
 * main.c
 * 蔬菜恒温库监控系统主文件
 * 功能：实现温湿度采集、显示、控制以及WiFi远程监控
 */

#include "delay.h"
#include "sys.h"
#include "OLED.h"
#include "dht22.h"
#include "esp8266.h"
#include "Timer.h"
#include "Key.h"
// #include "stmflash.h"
#include "Usart.h" 
#include "DS1302.h"
#include "control.h"

//C标准库
#include <string.h>
#include "stdio.h"

/**
 * 函数声明
 */
u8 WeekYearday(int years, int months, int days); //计算星期几的函数
extern unsigned char esp8266_buf[buf_len]; //ESP8266接收缓冲区

/**
 * 自定义函数声明
 */
void show_wendu(void); //显示温度湿度函数
void show_mode(void);  //显示模式函数
void show_on_off(void); //显示开关状态函数

/**
 * 全局变量定义
 */
u8 mode = 1;          //模式选择：1=自动模式，2=手动模式
int page = 1;         //页面选择：1=主页面，2=控制页面，3=设置页面
int page_clear = 0;   //页面清屏标志
int page2_index = 1;  //页面2的索引，用于选择不同的控制项
int page3_index = 1;  //页面3的索引，用于选择不同的设置项
int set_wendu_high = 25; //温度上限设置
int set_wendu_lou = 20;  //温度下限设置
int set_shidu_high = 65; //湿度上限设置
int set_shidu_low = 50;  //湿度下限设置

/**
 * DHT22相关变量
 */
DHT22_Data_TypeDef DHT22_Data;       // 当前温湿度数据
u8 data_valid = 0;                     // 温湿度数据有效标志
// volatile uint32_t sys_tick_ms = 0;    // 全局毫秒计数器
uint32_t last_dht22_read_time = 0;

/**
 * 按键相关变量
 */
u8 key_num = 0;      //按键返回值

/**
 * WiFi相关变量
 */
// char TIMER_IT = 0;     //定时器中断标志
char WiFiInit_time;    //WiFi初始化时间
char Flagout = 0;      //输出标志（0：禁止，1：允许）
uint32_t heart_beat_count = 0; //心跳包计数器
uint32_t reconnect_count = 0;   //重连计数器

/**
 * OLED显示相关变量
 */
u8 oled_Fill_flag = 1; //OLED填充标志
char oled_str[100];     //OLED显示字符串
char data[200];         //ESP8266发送缓冲区
u8 buff[30];            //缓冲区，用于显示数据
u8 wendu_display_force_update = 1; // 强制更新温湿度显示标志

/**
 * 主函数
 * 功能：系统初始化、数据采集、显示控制、WiFi通信
 */
int main(void)
{
    //设置中断优先级分组
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
                    
    //初始化各模块
    Key_Init();        //按键初始化
    OLED_Init();       //OLED显示初始化
    OLED_Clear(0);     //清屏
    Serial_Iint(115200);     //串口初始化，用于调试
    Serial_Printf("系统初始化中...\n\r");
    Ds1302_Init();     //DS1302实时时钟初始化
    beep_init();       //蜂鸣器初始化
    jiare_init();       //加热模块初始化
    zhileng_init();    //制冷模块初始化
    chushi_init();     //除湿模块初始化
    jiashi_init(); //加湿器模块初始化
    Timer_Init();          //初始化定时器

    // 全局上电延时，确保DHT22 2s稳定期
    OLED_ShowCHinese(0, 3, 19); // 系
    OLED_ShowCHinese(18, 3, 20); // 统
    OLED_ShowCHinese(36, 3, 21); // 正
    OLED_ShowCHinese(54, 3, 22); // 在
    OLED_ShowCHinese(72, 3, 0); // 初
    OLED_ShowCHinese(90, 3, 1); // 始
    OLED_ShowCHinese(108, 3, 2); // 化 
    delay_ms(2000); // 等待DHT22稳定
	
    // 初始化DHT22
    Serial_Printf("初始化DHT22...\n\r");
    if (DHT22_Init() == 0)
    {
        Serial_Printf("DHT22初始化成功\n\r");
        Serial_Printf("系统初始化完成\n\r");
                
        // 第一次读取的是上一次数据，连续读两次获取实时值
        Read_DHT22(&DHT22_Data); // 丢弃第一次
        delay_ms(2000);
        if (Read_DHT22(&DHT22_Data) == SUCCESS)
        {
            data_valid = 1;
        }
    }
    else
    {
        Serial_Printf("DHT22初始化失败，请检查接线\n\r");
    }
    OLED_Clear(0);
    // 系统初始化完成，蜂鸣器鸣响一下（低电平触发）
    beep = 0;
    delay_ms(100);
    beep = 1;

    //开机界面
    do
    {
        //显示欢迎信息
        OLED_ShowCHinese(0, 0, 52); //是
        OLED_ShowCHinese(18, 0, 53); //否
        OLED_ShowCHinese(36, 0, 54); //需
        OLED_ShowCHinese(52, 0, 55); //要
        OLED_ShowCHinese(68, 0, 56); //联
        OLED_ShowCHinese(84, 0, 57); //网
        OLED_ShowCHinese(100, 0, 58); //？ 
        
        //显示菜单选项
        OLED_ShowString(8, 4, "1.", 16);  
        OLED_ShowCHinese(24, 4, 52); //是
        OLED_ShowString(62, 4, "2.", 16);  
        OLED_ShowCHinese(78, 4, 53); //否
        
        key_num = KEY_Scan(0); //按键扫描
        
        if(key_num == 1) //选择菜单
        {
            OLED_Clear(0);
            
            Serial_Printf("初始化ESP8266模块...\n\r");
            ESP8266_Init(115200); //初始化ESP8266模块，波特率115200
            Flagout = 1;           //允许数据输出
            break;
        }
    } while(key_num != 2);  
	
    OLED_Clear(0); //清屏

    //主循环
	while(1)
	{
         /*****************************************数据采集与上传区*****************************************/ 
        // 捕捉 2 秒一次的节拍 
        if(TIMER_IT == 1)
        {
            TIMER_IT = 0; // 及时清零标志位
        
            /***************************** 第一步：读取温湿度 *****************************/
            if (Read_DHT22(&DHT22_Data) == SUCCESS)
            {
                wendu_display_force_update = 1; // 刷新屏幕标志
            }
            else
            {
                Serial_Printf("DHT22 Read Error\r\n");
                // 读取失败会保持上一次读取的数据
            }

            /***************************** 第二步：上传云平台 *****************************/
            if(Flagout == 1)
            {
                int abs_temp = (DHT22_Data.temperature < 0) ? -DHT22_Data.temperature : DHT22_Data.temperature;
                char sign_str[2] = "";
                if (DHT22_Data.temperature < 0) strcpy(sign_str, "-"); // 处理上传字符串的负号

                // 构建数据上传格式 
                sprintf(data, "cmd=2&uid=%s&topic=data&msg=Mode:%d temp:%s%d.%d humi:%d.%d\r\n", 
                BEMFA_ID, 
                mode,     
                sign_str, abs_temp / 10, abs_temp % 10, 
                DHT22_Data.humidity / 10, DHT22_Data.humidity % 10);
            


                // 发送数据
                if(ESP8266_SendData((unsigned char *)data) == 0)
                {
                    heart_beat_count = 0; // 重置心跳计数器
                    reconnect_count = 0;   // 重置重连计数器
                }
                else
                {
                    
                    reconnect_count++;
                    // 连续5次发送失败，尝试重连
                    if(reconnect_count >= 5)
                    {
                        // 清屏显示重连状态
                        OLED_Clear(0);
                        // 重连ESP8266
                        ESP8266_Init(115200);
                        reconnect_count = 0;
                    }
                    // // 显示上传失败状态
                    // OLED_Clear(0);
                    // sprintf(oled_str, "Upload: FAIL");
                    // OLED_ShowString(0, 4, oled_str, 12);
                    // delay_ms(1000); // 显示失败状态1秒
                    // OLED_Clear(0); // 显示失败状态后清屏
                }

                // 定期发送心跳包
                heart_beat_count++;
                if(heart_beat_count >= 30) // 每30个周期（60秒）发送一次心跳包
                {
                    sprintf(data, "cmd=2&uid=%s&topic=online&msg=Keep online\r\n", BEMFA_ID );
                    ESP8266_SendData((unsigned char *)data);
                    heart_beat_count = 0;
                }

                // 清空接收缓冲区
                ESP8266_Clear();
            }  
        }
        
        /*********************************温湿度超范围报警************************************/
        // 温湿度超范围报警
        static uint8_t beep_count = 0;
        static uint8_t beep_state = 0;
        
        // 检查温度是否超过范围2度
        float current_temp = DHT22_Data.temperature / 10.0f;
        float current_humi = DHT22_Data.humidity / 10.0f;
        
        float temp_diff = 0;
        if(current_temp > set_wendu_high)
            temp_diff = current_temp - set_wendu_high;
        else if(current_temp < set_wendu_lou)
            temp_diff = set_wendu_lou - current_temp;
        
        // 检查湿度是否超过范围5%
        float humi_diff = 0;
        if(current_humi > set_shidu_high)
            humi_diff = current_humi - set_shidu_high;
        else if(current_humi < set_shidu_low)
            humi_diff = set_shidu_low - current_humi;
        
        // 当温度超过范围2度或湿度超过范围5%时报警
        if(temp_diff >= 2.0f || humi_diff >= 5.0f)
        {
            // 使用简单的计数方式控制蜂鸣器频率
            beep_count++;
            if(beep_count >= 5) // 大约5个主循环周期（根据实际主循环执行时间调整）触发一次蜂鸣状态切换
            {
                beep_count = 0;
                beep_state = !beep_state;
                beep = beep_state ? 0 : 1; // 低电平触发
            }
        }
        else
        {
            // 正常状态，关闭蜂鸣器（高电平）
            beep = 1;
            beep_state = 0;
            beep_count = 0;
        }

        /*********************************面板控制区**************************************************/      
        key_num = KEY_Scan(0); //按键扫描
        
        if(key_num == 1) //切换页面
        {
            page += 1;
            OLED_Clear(0);
            if(page == 4)  //页面循环
                page = 1;
        }

        //页面1：主页面
        if(page == 1)
        {
            if(page_clear == 0)
            {
                page_clear = 1;
                OLED_Clear(0);
                wendu_display_force_update = 1; // 页面切换，强制更新温湿度显示
            }
            show_mode();    //显示当前模式
            TIME();         //显示时间
            show_wendu();   //显示温湿度
            
            if(key_num == 4) //切换到手动模式
            {
                mode = 2;
                jiare = 0;    //关闭加热
                zhileng = 0;  //关闭制冷
                chushi = 0;   //关闭除湿
                // 模式变化，重新显示
                show_mode();
            }
            if(key_num == 3) //切换到自动模式
            {
                mode = 1;
                jiare = 0;    //关闭加热
                zhileng = 0;  //关闭制冷
                chushi = 0;   //关闭除湿
                // 模式变化，重新显示
                show_mode();
            }
        }

        //页面2：控制页面
        if(page == 2)
        {
            if(page_clear == 1)
            {
                page_clear = 2;
                OLED_Clear(0);
            }
            
            if(key_num == 2) //切换控制项
        {
            page2_index++;
            if(page2_index == 5)
                page2_index = 1;
        }
            
            //显示加热控制
            OLED_ShowCHinese(0, 0, 116); //加
            OLED_ShowCHinese(16, 0, 117); //热
            OLED_ShowString(32, 0, (u8 *)":", 16);
            
            //显示制冷控制
            OLED_ShowCHinese(0, 2, 118); //制
            OLED_ShowCHinese(16, 2, 119); //冷
            OLED_ShowString(32, 2, (u8 *)":", 16);
            
            //显示除湿控制
            OLED_ShowCHinese(0, 4, 120); //除
            OLED_ShowCHinese(16, 4, 121); //湿
            OLED_ShowString(32, 4, (u8 *)":", 16);
            
            //显示加湿控制
            OLED_ShowCHinese(0, 6, 116); //加
            OLED_ShowCHinese(16, 6, 121); //湿
            OLED_ShowString(32, 6, (u8 *)":", 16);
            
            //加热控制
            if(page2_index == 1)
            {
                OLED_ShowString(60, 0, (u8 *)"<", 16);
                OLED_ShowString(60, 2, (u8 *)" ", 16);   
                OLED_ShowString(60, 4, (u8 *)" ", 16); 
                OLED_ShowString(60, 6, (u8 *)" ", 16); 
                
                if(mode == 2) //手动模式下才能控制
                {
                    if(key_num == 3) //开启加热
                    {
                        jiare = 1;
                        zhileng = 0; // 开启加热时关闭制冷
                    }
                    else if(key_num == 4) //关闭加热
                    {
                        jiare = 0;
                    }
                }
            }
            
            //制冷控制
            if(page2_index == 2)
            {
                OLED_ShowString(60, 0, (u8 *)" ", 16);
                OLED_ShowString(60, 2, (u8 *)"<", 16);   
                OLED_ShowString(60, 4, (u8 *)" ", 16); 
                OLED_ShowString(60, 6, (u8 *)" ", 16); 
                
                if(mode == 2) //手动模式下才能控制
                {
                    if(key_num == 3) //开启制冷
                    {
                        zhileng = 1;
                        jiare = 0; // 开启制冷时关闭加热
                    }
                    else if(key_num == 4) //关闭制冷
                    {
                        zhileng = 0;
                    }
                }
            }
            
            //除湿控制
            if(page2_index == 3)
            {
                OLED_ShowString(60, 0, (u8 *)" ", 16);
                OLED_ShowString(60, 2, (u8 *)" ", 16);   
                OLED_ShowString(60, 4, (u8 *)"<", 16);
                OLED_ShowString(60, 6, (u8 *)" ", 16); 
                
                if(mode == 2) //手动模式下才能控制
                {
                    if(key_num == 3) //开启除湿
                    {
                        chushi = 1;
                        jiashi = 0; // 开启除湿时关闭加湿
                    }
                    else if(key_num == 4) //关闭除湿
                    {
                        chushi = 0;
                    }
                }
            }
            
            //加湿控制
            if(page2_index == 4)
            {
                OLED_ShowString(60, 0, (u8 *)" ", 16);
                OLED_ShowString(60, 2, (u8 *)" ", 16);   
                OLED_ShowString(60, 4, (u8 *)" ", 16);
                OLED_ShowString(60, 6, (u8 *)"<", 16); 
                
                if(mode == 2) //手动模式下才能控制
                {
                    if(key_num == 3) //开启加湿
                    {
                        jiashi = 1;
                        chushi = 0; // 开启加湿时关闭除湿
                    }
                    else if(key_num == 4) //关闭加湿
                    {
                        jiashi = 0;
                    }
                }
            }
            
            //显示加热状态
            if(jiare == 1)
            {
                OLED_ShowCHinese(40, 0, 122); //开
            }
            else
            {
                OLED_ShowCHinese(40, 0, 123); //关
            }
            
            //显示制冷状态
            if(zhileng == 1)
            {
                OLED_ShowCHinese(40, 2, 122); //开
            }
            else
            {
                OLED_ShowCHinese(40, 2, 123); //关
            }
            
            //显示除湿状态
            if(chushi == 1)
            {
                OLED_ShowCHinese(40, 4, 122); //开
            }
            else
            {
                OLED_ShowCHinese(40, 4, 123); //关
            }
            
            //显示加湿状态
            if(jiashi == 1)
            {
                OLED_ShowCHinese(40, 6, 122); //开
            }
            else
            {
                OLED_ShowCHinese(40, 6, 123); //关
            }
        }

        //页面3：设置页面
        if(page == 3)
        {
            if(page_clear == 2)
            {
                page_clear = 1;
                OLED_Clear(0);
            }
            
            if(key_num == 2) //切换设置项
            {
                page3_index++;
                if(page3_index == 5)
                    page3_index = 1;
            }
            
            // //显示设置标题
            // OLED_ShowCHinese(16+15, 0, 26);
            // OLED_ShowCHinese(16+15+16, 0, 27);
            // OLED_ShowCHinese(16+15+32, 0, 28);
            // OLED_ShowCHinese(16+15+48, 0, 29);
            
            //显示温度上限
            OLED_ShowCHinese(0, 0, 10); //温
            OLED_ShowCHinese(16, 0, 12); //度
            OLED_ShowCHinese(32, 0, 124); //上
            OLED_ShowCHinese(48, 0, 125); //限
            OLED_ShowChar(64, 0, ':', 16);
            OLED_ShowNum(72, 0, set_wendu_high, 2, 16);
            
            //显示温度下限
            OLED_ShowCHinese(0, 2, 10); //温
            OLED_ShowCHinese(16, 2, 12); //度
            OLED_ShowCHinese(32, 2, 126); //下
            OLED_ShowCHinese(48, 2, 127); //限
            OLED_ShowChar(64, 2, ':', 16);
            OLED_ShowNum(72, 2, set_wendu_lou, 2, 16);
            
            //显示湿度上限
            OLED_ShowCHinese(0, 4, 11); //湿
            OLED_ShowCHinese(16, 4, 12); //度
            OLED_ShowCHinese(32, 4, 124); //上
            OLED_ShowCHinese(48, 4, 125); //限
            OLED_ShowChar(64, 4, ':', 16);
            OLED_ShowNum(72, 4, set_shidu_high, 2, 16);
            
            //显示湿度下限
            OLED_ShowCHinese(0, 6, 11); //湿
            OLED_ShowCHinese(16, 6, 12); //度
            OLED_ShowCHinese(32, 6, 126); //下
            OLED_ShowCHinese(48, 6, 127); //限
            OLED_ShowChar(64, 6, ':', 16);
            OLED_ShowNum(72, 6, set_shidu_low, 2, 16);
            
            //设置温度上限
            if(page3_index == 1)
            {
                OLED_ShowString(100, 0, (u8 *)"<", 16);
                OLED_ShowString(100, 2, (u8 *)" ", 16);
                OLED_ShowString(100, 4, (u8 *)" ", 16);   
                OLED_ShowString(100, 6, (u8 *)" ", 16); 
                
                if(key_num == 3) //增加温度上限
                {
                    set_wendu_high++;
                    // 确保上限大于下限
                    if(set_wendu_high <= set_wendu_lou)
                    {
                        set_wendu_lou = set_wendu_high - 1;
                    }
                }
                if(key_num == 4) //减少温度上限
                {
                    set_wendu_high--;
                    // 确保上限大于下限
                    if(set_wendu_high <= set_wendu_lou)
                    {
                        set_wendu_high = set_wendu_lou + 1;
                    }
                }
            }
            
            //设置温度下限
            if(page3_index == 2)
            {
                OLED_ShowString(100, 0, (u8 *)" ", 16);
                OLED_ShowString(100, 2, (u8 *)"<", 16);
                OLED_ShowString(100, 4, (u8 *)" ", 16);   
                OLED_ShowString(100, 6, (u8 *)" ", 16); 
                
                if(key_num == 3) //增加温度下限
                {
                    set_wendu_lou++;
                    // 确保下限小于上限
                    if(set_wendu_lou >= set_wendu_high)
                    {
                        set_wendu_high = set_wendu_lou + 1;
                    }
                }
                if(key_num == 4) //减少温度下限
                {
                    set_wendu_lou--;
                }
            }
            
            //设置湿度上限
            if(page3_index == 3)
            {
                OLED_ShowString(100, 0, (u8 *)" ", 16);
                OLED_ShowString(100, 2, (u8 *)" ", 16);   
                OLED_ShowString(100, 4, (u8 *)"<", 16);
                OLED_ShowString(100, 6, (u8 *)" ", 16); 
                
                if(key_num == 3) //增加湿度上限
                {
                    set_shidu_high++;
                    // 确保上限大于下限
                    if(set_shidu_high <= set_shidu_low)
                    {
                        set_shidu_low = set_shidu_high - 1;
                    }
                }
                if(key_num == 4) //减少湿度上限
                {
                    set_shidu_high--;
                    // 确保上限大于下限
                    if(set_shidu_high <= set_shidu_low)
                    {
                        set_shidu_high = set_shidu_low + 1;
                    }
                }
            }
            
            //设置湿度下限
            if(page3_index == 4)
            {
                OLED_ShowString(100, 0, (u8 *)" ", 16);
                OLED_ShowString(100, 2, (u8 *)" ", 16);   
                OLED_ShowString(100, 4, (u8 *)" ", 16);
                OLED_ShowString(100, 6, (u8 *)"<", 16); 
                
                if(key_num == 3) //增加湿度下限
                {
                    set_shidu_low++;
                    // 确保下限小于上限
                    if(set_shidu_low >= set_shidu_high)
                    {
                        set_shidu_high = set_shidu_low + 1;
                    }
                }
                if(key_num == 4) //减少湿度下限
                {
                    set_shidu_low--;
                }
            }
        }
        
        // // 从ESP8266接收的数据中解析设置值
        // if (sscanf((strstr((char *)esp8266_buf, "wendu_high") + 10), "=%d", &set_wendu_high)) 
        // {
        //     ESP8266_Clear(); //清除缓冲区
        // }
        // if (sscanf((strstr((char *)esp8266_buf, "wendu_lou") + 9), "=%d", &set_wendu_lou)) 
        // {
        //     ESP8266_Clear(); //清除缓冲区
        // }
        // if (sscanf((strstr((char *)esp8266_buf, "shidu_high") + 10), "=%d", &set_shidu_high)) 
        // {
        //     ESP8266_Clear(); //清除缓冲区
        // }
        // if (sscanf((strstr((char *)esp8266_buf, "shidu_low") + 9), "=%d", &set_shidu_low)) 
        // {
        //     ESP8266_Clear(); //清除缓冲区
        // }
        
        // 从ESP8266接收的数据中解析设置值
        int parse_count = 0;
        if (strstr((char *)esp8266_buf, "wendu_high")) {
            sscanf((strstr((char *)esp8266_buf, "wendu_high") + 10), "=%d", &set_wendu_high);
            parse_count++;
        }
        if (strstr((char *)esp8266_buf, "wendu_lou")) {
            sscanf((strstr((char *)esp8266_buf, "wendu_lou") + 9), "=%d", &set_wendu_lou);
            parse_count++;
        }
        if (strstr((char *)esp8266_buf, "shidu_high")) {
            sscanf((strstr((char *)esp8266_buf, "shidu_high") + 10), "=%d", &set_shidu_high);
            parse_count++;
        }
        if (strstr((char *)esp8266_buf, "shidu_low")) {
            sscanf((strstr((char *)esp8266_buf, "shidu_low") + 9), "=%d", &set_shidu_low);
            parse_count++;
        }
        // 验证并调整上下限关系
        if (parse_count > 0) {
            // 温度上下限验证
            if (set_wendu_lou >= set_wendu_high) {
                set_wendu_high = set_wendu_lou + 1;
            }
            // 湿度上下限验证
            if (set_shidu_low >= set_shidu_high) {
                set_shidu_high = set_shidu_low + 1;
            }
            ESP8266_Clear(); // 所有解析完成后再清除缓冲区
        }

        /*********************************模式控制区**************************************************/
        //APP模式切换
        if(strstr((const char *)esp8266_buf, "ZD") != 0) //自动模式
        {
            mode = 1;
            jiare = 0;    //关闭加热
            zhileng = 0;  //关闭制冷
            chushi = 0;   //关闭除湿
            jiashi = 0; //关闭加湿
        }
        else if(strstr((const char *)esp8266_buf, "SD") != 0) //手动模式
        {
            mode = 2;
            jiare = 0;    //关闭加热
            zhileng = 0;  //关闭制冷
            chushi = 0;   //关闭除湿
            jiashi = 0; //关闭加湿
        } 

        //自动模式逻辑
        if(mode == 1)
        {
            // float current_temp = DHT22_Data.temperature / 10.0f;
            // float current_humi = DHT22_Data.humidity / 10.0f;
            
            // 温度控制逻辑
            if(current_temp > set_wendu_high)
            {
                zhileng = 1;
                jiare = 0; // 制冷时关闭加热
            }
            else if(current_temp < set_wendu_lou)
            {
                jiare = 1;
                zhileng = 0; // 加热时关闭制冷
            }
            else
            {
                jiare = 0;
                zhileng = 0;
            }
            
            // 湿度控制逻辑
            if(current_humi > set_shidu_high)
            {
                chushi = 1;
                jiashi = 0; // 除湿时关闭加湿
            }
            else if(current_humi < set_shidu_low)
            {
                jiashi = 1;
                chushi = 0; // 加湿时关闭除湿
            }
            else
            {
                chushi = 0;
                jiashi = 0;
            }
        }
        
        //手动模式逻辑
        if(mode == 2)
        {
            if(Flagout == 1)
            {
                //远程控制加热
                if(strstr((const char *)esp8266_buf, "KJR") != 0) //开启加热
                {
                    jiare = 1;
                    zhileng = 0; // 开启加热时关闭制冷
                }
                if(strstr((const char *)esp8266_buf, "GJR") != 0) //关闭加热
                {
                    jiare = 0;
                }
                
                //远程控制制冷
                if(strstr((const char *)esp8266_buf, "KZL") != 0) //开启制冷
                {
                    zhileng = 1;
                    jiare = 0; // 开启制冷时关闭加热
                }
                if(strstr((const char *)esp8266_buf, "GZL") != 0) //关闭制冷
                {
                    zhileng = 0;
                }
                
                //远程控制除湿
                if(strstr((const char *)esp8266_buf, "KCS") != 0) //开启除湿
                {
                    chushi = 1;
                    jiashi = 0; // 开启除湿时关闭加湿
                }
                if(strstr((const char *)esp8266_buf, "GCS") != 0) //关闭除湿
                {
                    chushi = 0;
                }
                
                //远程控制加湿
                if(strstr((const char *)esp8266_buf, "KJS") != 0) //开启加湿
                {
                    jiashi = 1;
                    chushi = 0; // 开启加湿时关闭除湿
                }
                if(strstr((const char *)esp8266_buf, "GJS") != 0) //关闭加湿
                {
                    jiashi = 0;
                }
            }
        }
	}
}

/**
 * 计算星期几的函数
 * @param years 年份
 * @param months 月份
 * @param days 日期
 * @return 星期几（1-7，其中1表示星期一，7表示星期日）
 */
u8 WeekYearday(int years, int months, int days) 
{
    int WeekDay = -1; 
    
    //处理1月和2月的特殊情况
    if(1 == months || 2 == months) 
    {
        months += 12; 
        years--;
    }
    
    //使用基姆拉尔森计算公式计算星期几
    WeekDay = (days + 1 + 2 * months + 3 * (months + 1) / 5 + years + years / 4 - years / 100 + years / 400) % 7;
    
    //转换结果，使1表示星期一，7表示星期日
    switch(WeekDay)
    {
        case 0 : 
            return 7; //星期日
        case 1 : 
            return 1; //星期一
        case 2 : 
            return 2; //星期二
        case 3 : 
            return 3; //星期三
        case 4 : 
            return 4; //星期四
        case 5 : 
            return 5; //星期五                                                           
        case 6 : 
            return 6; //星期六
        default : 
            return NULL; //错误情况
    }
    return NULL;
}


/**
 * 显示温湿度函数
 * 功能：在OLED上显示当前温度和湿度
 */
void show_wendu(void)
{
    // 提取绝对值用于求模计算
    int abs_temp = (DHT22_Data.temperature < 0) ? -DHT22_Data.temperature : DHT22_Data.temperature;
    char temp_sign = (DHT22_Data.temperature < 0) ? '-' : ' '; // 负数显示减号，正数补空格对齐

    sprintf(oled_str, "T:%c%d.%dC H:%d.%d%%", 
        temp_sign,
        abs_temp / 10, abs_temp % 10,
        DHT22_Data.humidity / 10, DHT22_Data.humidity % 10);
        
    OLED_ShowString(0, 6, (u8 *)oled_str, 16);  
}

/**
 * 显示模式函数
 * 功能：在OLED上显示当前运行模式
 */
void show_mode(void)
{
    if(mode == 1) //自动模式
    {
        OLED_ShowCHinese(0, 0, 31); //自
        OLED_ShowCHinese(16, 0, 33); //动
        OLED_ShowCHinese(32, 0, 108); //模
        OLED_ShowCHinese(48, 0, 109); //式
    }
    if(mode == 2) //手动模式
    {
        OLED_ShowCHinese(0, 0, 32); //手
        OLED_ShowCHinese(16, 0, 33); //动
        OLED_ShowCHinese(32, 0, 108); //模
        OLED_ShowCHinese(48, 0, 109); //式
    }
}

/**
 * 显示开关状态函数
 * 功能：预留函数，用于显示设备开关状态
 */
void show_on_off(void)
{
    //预留函数，可根据需要实现
}