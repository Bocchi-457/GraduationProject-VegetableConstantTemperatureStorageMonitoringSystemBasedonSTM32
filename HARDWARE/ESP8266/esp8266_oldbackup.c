/**
 * esp8266.c
 * ESP8266 WiFi模块驱动实现文件
 * 功能：实现ESP8266 WiFi模块与巴法云平台通信的功能
 * 版本：V1.0
 * MCU：STM32F103C8T6
 * 作者：蔬菜恒温库监控系统
 */

#include "esp8266.h"
#include "delay.h"
#include "string.h"
#include "OLED.h"

/**
 * 全局变量定义
 */
unsigned char Secret_Key[] = "1234567890";
unsigned char esp8266_buf[buf_len]; // ESP8266接收缓冲区
unsigned short esp8266_cnt = 0;      // 接收缓冲区计数
unsigned char esp8266_recive_flag = REV_WAIT; // 接收标志

/**
 * @brief 清空ESP8266接收缓冲区
 * @param 无
 * @return 无
 */
void ESP8266_Clear(void)
{
    memset(esp8266_buf, 0, sizeof(esp8266_buf));
    esp8266_cnt = 0;
    esp8266_recive_flag = REV_WAIT;
}

/**
 * @brief 显示失败界面和倒计时
 * @param message: 失败信息
 * @param time: 倒计时时间（秒）
 * @return 无
 */
void OLED_ShowFailureWithCountdown(char *message, int time)
{
    OLED_Clear(0);
    OLED_ShowCHinese(0, 0, 0); // 初
    OLED_ShowCHinese(18, 0, 1); // 始
    OLED_ShowCHinese(36, 0, 2); // 化
    OLED_ShowCHinese(54, 0, 36); // 失
    OLED_ShowCHinese(72, 0, 37); // 败
    OLED_ShowString(0, 3, message, 16);
    
    char countdown_str[10];
    for (int i = time; i > 0; i--)
    {
        sprintf(countdown_str, "%ds", i);
        OLED_ShowString(90, 6, countdown_str, 16);
        delay_ms(1000);
    }
}

/**
 * @brief 显示WiFi连接进度
 * @param progress: 进度（0-100）
 * @return 无
 */
void OLED_ShowWiFiProgress(int progress)
{
    OLED_Clear(0);
    OLED_ShowCHinese(0, 0, 0); // 初
    OLED_ShowCHinese(18, 0, 1); // 始
    OLED_ShowCHinese(36, 0, 2); // 化
    OLED_ShowCHinese(54, 0, 3); // 中
    OLED_ShowString(72, 0, "...", 16);
    OLED_ShowString(0, 3, "WiFi connecting", 16);
    
    // 显示进度百分比
    char progress_str[10];
    sprintf(progress_str, "%d%%", progress);
    OLED_ShowString(50, 6, progress_str, 16);
}

/**
 * @brief 等待ESP8266接收数据
 * @param 无
 * @return 接收完成返回1，超时返回0
 */
_Bool ESP8266_WaitRecive(void)
{
    unsigned char time_out = 0;
    while (esp8266_recive_flag == REV_WAIT)
    {
        delay_ms(10);
        time_out++;
        if (time_out > 1000) // 10000ms超时，增加到10秒以适应WiFi连接
            return 0;
    }
    esp8266_recive_flag = REV_WAIT;
    return 1;
}

/**
 * @brief 发送AT指令并等待响应
 * @param cmd: 要发送的AT指令
 * @param res: 期望的响应
 * @return 响应正确返回1，否则返回0
 */
_Bool ESP8266_SendCmd(char *cmd, char *res)
{
    ESP8266_Clear();
    Usart_SendString(Bemfa_USART, (unsigned char *)cmd, strlen(cmd));
    if (res == NULL)
        return 1;
    
    // 等待接收响应
    unsigned char time_out = 0;
    while (esp8266_recive_flag == REV_WAIT)
    {
        delay_ms(10);
        time_out++;
        if (time_out > 1000) // 10000ms超时
            return 0;
    }
    esp8266_recive_flag = REV_WAIT;
    
    // 检查响应
    char *response = (char *)esp8266_buf;
    
    // 特殊处理WiFi连接指令
    if (strstr(cmd, "AT+CWJAP") != NULL)
    {
        // WiFi连接可能返回"WIFI CONNECTED"和"OK"，需要同时检查
        return (strstr(response, "WIFI CONNECTED") != NULL) || (strstr(response, res) != NULL);
    }
    
    // 一般指令检查
    return (strstr(response, res) != NULL);
}

/**
 * @brief 串口发送字符串
 * @param USARTx: 串口编号
 * @param str: 要发送的字符串
 * @param len: 字符串长度
 * @return 无
 */
void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len)
{
    unsigned short i;
    for (i = 0; i < len; i++)
    {
        while (USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET);
        USART_SendData(USARTx, str[i]);
    }
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);
}

/**
 * @brief 发送数据到巴法云（透传模式）
 * @param data: 要发送的数据
 * @return 无
 */
void ESP8266_SendData(unsigned char *data)
{
    // 透传模式下直接发送数据
    Usart_SendString(Bemfa_USART, data, strlen((char *)data));
    // 等待响应
    ESP8266_WaitRecive();
}

/**
 * @brief ESP8266初始化
 * @param bound: 波特率
 * @return 无
 */
void ESP8266_Init(unsigned int bound)
{
    // 初始化复位引脚
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(ESP01S_RST_RCC_CLK, ENABLE);
    GPIO_InitStructure.GPIO_Pin = ESP01S_RST_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ESP01S_RST_PROT, &GPIO_InitStructure);
    
    // 复位ESP8266
    GPIO_ResetBits(ESP01S_RST_PROT, ESP01S_RST_PIN);
    delay_ms(500);
    GPIO_SetBits(ESP01S_RST_PROT, ESP01S_RST_PIN);
    delay_ms(1000);
    
    // 初始化串口
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    // 配置USART2_TX (PA.02)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 配置USART2_RX (PA.03)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 配置USART2
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);
    
    // 配置中断
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    // 启用接收中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);
    
    // 测试AT指令
    delay_ms(1000);
    int at_retry = 3;
    while (at_retry > 0)
    {
        if (ESP8266_SendCmd("AT\r\n", "OK"))
        {
            // 设置WiFi模式为STA
            int mode_retry = 3;
            while (mode_retry > 0)
            {
                if (ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK"))
                {
                    // 连接WiFi
                    int wifi_retry = 3;
                    while (wifi_retry > 0)
                    {
                        // 显示WiFi连接进度
                        for (int i = 0; i <= 100; i += 10)
                        {
                            OLED_ShowWiFiProgress(i);
                            delay_ms(300);
                        }
                        
                        // 使用修改后的ESP8266_SendCmd函数，它会正确处理WiFi连接响应
                        if (ESP8266_SendCmd(ESP8266_WIFI_INFO, "OK"))
                        {
                            // 等待WiFi连接
                            for (int i = 0; i <= 100; i += 10)
                            {
                                OLED_ShowWiFiProgress(i);
                                delay_ms(300);
                            }
                            
                            delay_ms(1000); // 额外等待
                            // 连接巴法云
                            int bemfa_retry = 3;
                            while (bemfa_retry > 0)
                            {
                                if (ESP8266_SendCmd(ESP8266_ONENET_INFO, "OK"))
                                {
                                    delay_ms(1000);
                                    // 设置单连接模式
                                    if (ESP8266_SendCmd("AT+CIPMUX=0\r\n", "OK"))
                                    {
                                        delay_ms(500);
                                        // 开启透传模式
                                        if (ESP8266_SendCmd("AT+CIPMODE=1\r\n", "OK"))
                                        {
                                            delay_ms(500);
                                            // 进入透传发送状态
                                            if (ESP8266_SendCmd("AT+CIPSEND\r\n", ">"))
                                            {
                                                delay_ms(500);
                                                // 订阅主题
                                                ESP8266_SendData((unsigned char *)ESP8266_TOPIC);
                                                delay_ms(1000);
                                                // 显示连接成功提示
                                                OLED_Clear(0);
                                                OLED_ShowCHinese(0, 0, 0); // 初
                                                OLED_ShowCHinese(18, 0, 1); // 始
                                                OLED_ShowCHinese(36, 0, 2); // 化
                                                OLED_ShowCHinese(54, 0, 38); // 成
                                                OLED_ShowCHinese(72, 0, 39); // 功
                                                OLED_ShowString(0, 3, "WiFi connected", 16);
                                                OLED_ShowString(0, 6, "Bemfa connected", 16);
                                                delay_ms(2000);
                                                return;
                                            }
                                            else
                                            {
                                                OLED_ShowFailureWithCountdown("Enter send mode failed", 5);
                                            }
                                        }
                                        else
                                        {
                                            OLED_ShowFailureWithCountdown("Set transparent mode failed", 5);
                                        }
                                    }
                                    else
                                    {
                                        OLED_ShowFailureWithCountdown("Set single connection mode failed", 5);
                                    }
                                }
                                else
                                {
                                    // 连接巴法云失败
                               OLED_ShowFailureWithCountdown((u8 *)"Bemfa connect failed", 5);
                                    bemfa_retry--;
                                }
                            }
                            if (bemfa_retry <= 0)
                            {
                                OLED_ShowFailureWithCountdown("Bemfa connect failed", 5);
                                break;
                            }
                        }
                        else
                        {
                            // 连接WiFi失败
                       OLED_ShowFailureWithCountdown((u8 *)"WiFi connect failed", 5);
                            wifi_retry--;
                        }
                    }
                    if (wifi_retry <= 0)
                    {
                        OLED_ShowFailureWithCountdown("WiFi connect failed", 5);
                        break;
                    }
                }
                else
                {
                    // 设置WiFi模式失败
               OLED_ShowFailureWithCountdown((u8 *)"WiFi mode setting failed", 5);
                    mode_retry--;
                }
            }
            if (mode_retry <= 0)
            {
                OLED_ShowFailureWithCountdown("WiFi mode setting failed", 5);
                break;
            }
        }
        else
        {
            // AT指令测试失败
       OLED_ShowFailureWithCountdown((u8 *)"AT command test failed", 5);
            at_retry--;
        }
    }
    if (at_retry <= 0)
    {
        OLED_ShowFailureWithCountdown("AT command test failed", 5);
    }
}

/**
 * @brief USART2中断处理函数
 * @param 无
 * @return 无
 */
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        unsigned char data = USART_ReceiveData(USART2);
        if (esp8266_cnt < buf_len)
        {
            esp8266_buf[esp8266_cnt++] = data;
        }
        // 只有当接收到换行符时才设置接收完成标志，确保完整接收响应
        if (data == '\n')
        {
            esp8266_recive_flag = REV_OK;
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

/**
 * @brief 模式选择函数
 * @param 无
 * @return 无
 */
void mode_choice(void)
{
    // 预留函数，可根据需要实现
}

/**
 * @brief 提取小时和分钟的函数
 * @param input: 输入字符串
 * @param hour: 提取的小时
 * @param minute: 提取的分钟
 * @return 无
 */
void extractHourAndMinute(const char *input, int *hour, int *minute)
{
    // 简单实现，假设输入格式为 "HH:MM"
    sscanf(input, "%d:%d", hour, minute);
}