#include "esp8266.h"
#include "delay.h"
#include "string.h"
#include "OLED.h"
#include "stdio.h"

// 全局变量定义
unsigned char Secret_Key[] = "1234567890";
unsigned char esp8266_buf[buf_len]; // ESP8266接收缓冲区
unsigned short esp8266_cnt = 0;      // 接收缓冲区计数
unsigned char esp8266_recive_flag = REV_WAIT; // 接收标志

// 新代码使用的全局变量
uint8_t ESP8266_RecvBuf[buf_len] = {0}; // 接收缓冲区
uint16_t ESP8266_RecvLen = 0;                        // 接收数据长度

// 函数声明
uint8_t ESP8266_SendATCmd(char *cmd, char *ack, uint32_t timeout);
void OLED_ShowFailureWithCountdown(u8 *message, int time);
void OLED_ShowWiFiProgress(int progress);

/**
 * @brief  串口中断服务函数（ESP8266数据接收）
 * @note   USART2中断，用于接收ESP8266返回的AT指令响应/数据
 * @param  无
 * @retval 无
 */
void USART2_IRQHandler(void)
{
    if(USART_GetITStatus(Bemfa_USART, USART_IT_RXNE) != RESET) // 接收中断
    {
        unsigned char data = USART_ReceiveData(Bemfa_USART);
        if(esp8266_cnt < buf_len) // 防止缓冲区溢出
        {
            esp8266_buf[esp8266_cnt++] = data;
        }
        if(ESP8266_RecvLen < buf_len) // 防止缓冲区溢出
        {
            ESP8266_RecvBuf[ESP8266_RecvLen++] = data;
        }
        // 只有当接收到换行符时才设置接收完成标志，确保完整接收响应
        if (data == '\n')
        {
            esp8266_recive_flag = REV_OK;
        }
        USART_ClearITPendingBit(Bemfa_USART, USART_IT_RXNE); // 清除中断标志
    }
}

/**
 * @brief  初始化ESP8266通信串口（USART2）
 * @note   配置GPIO、USART外设、中断
 * @param  bound: 波特率
 * @retval 无
 */
static void ESP8266_USART_Init(unsigned int bound)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    // 1. 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    // 2. 配置TX引脚（推挽复用输出）
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 3. 配置RX引脚（浮空输入）
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 4. 配置USART参数
    USART_InitStruct.USART_BaudRate = bound;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(Bemfa_USART, &USART_InitStruct);

    // 5. 配置中断（接收中断）
    NVIC_InitStruct.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 3; // 抢占优先级3
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 3;        // 子优先级3
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    // 6. 使能接收中断和USART外设
    USART_ITConfig(Bemfa_USART, USART_IT_RXNE, ENABLE);
    USART_Cmd(Bemfa_USART, ENABLE);
}

/**
 * @brief  初始化ESP8266（串口+基础AT指令配置）
 * @note   包括串口初始化、恢复出厂设置、设置STA模式、关闭多路连接
 * @param  bound: 波特率
 * @retval 无
 */
void ESP8266_Init(unsigned int bound)
{
    //显示初始化信息
    OLED_ShowCHinese(0, 3, 21); //正
    OLED_ShowCHinese(18, 3, 22); //在
    OLED_ShowCHinese(36, 3, 23); //连
    OLED_ShowCHinese(54, 3, 24); //接
    OLED_ShowString(72, 3, "WIFI", 16);
    OLED_ShowString(108, 3, "..", 16);
    OLED_ShowCHinese(0, 6, 4); //进
    OLED_ShowCHinese(18, 6, 5); //度
    OLED_ShowCHinese(36, 6, 13); //：
    
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

    // 1. 初始化通信串口
    ESP8266_USART_Init(bound);

    // 2. 清空接收缓冲区
    memset(ESP8266_RecvBuf, 0, buf_len);
    ESP8266_RecvLen = 0;
    ESP8266_Clear();
            

    // 3. 发送AT指令检测ESP8266是否在线
    int at_retry = 3;
    while (at_retry > 0)
    {
        if(ESP8266_SendATCmd("AT\r\n", "OK", 1000) == 0)
        {
            // 4. 恢复出厂设置（可选，确保初始状态）
            ESP8266_SendATCmd("AT+RESTORE\r\n", "OK", 3000);

            // 5. 设置ESP8266为STA模式（连接路由器）
            int mode_retry = 3;
            while (mode_retry > 0)
            {
                if(ESP8266_SendATCmd("AT+CWMODE=1\r\n", "OK", 1000) == 0)
                {
                    // 6. 关闭多路连接（单连接模式，简化TCP操作）
                    ESP8266_SendATCmd("AT+CIPMUX=0\r\n", "OK", 1000);
                    
                    // 7. 连接WiFi
                    int wifi_retry = 3;
                    while (wifi_retry > 0)
                    {
                        int i = 0;
                        // 显示WiFi连接进度
                        for (; i <= 30; i += 1)
                        {
                            OLED_ShowWiFiProgress(i);
                            delay_ms(20);
                        }
                        
                        if(ESP8266_SendATCmd(ESP8266_WIFI_INFO, "WIFI CONNECTED", 10000) == 0)
                        {
                            // 额外检查是否获取到IP
                            ESP8266_SendATCmd("AT+CIFSR\r\n", "STAIP", 2000);
                            
                            // 等待WiFi连接
                            for (; i <= 40; i += 1)
                            {
                                OLED_ShowWiFiProgress(i);
                                delay_ms(30);
                            }
                            
                            // delay_ms(500); // 额外等待
                            // 连接巴法云
                            int bemfa_retry = 3;
                            while (bemfa_retry > 0)
                            {
                                // 显示巴法云连接进度
                                for (; i <= 60; i += 1)
                                {
                                    OLED_ShowWiFiProgress(i);
                                    delay_ms(20);
                                }
                                if(ESP8266_SendATCmd(ESP8266_ONENET_INFO, "CONNECT", 5000) == 0)
                                {
                                    // delay_ms(500);
                                    // 订阅主题
                                    // 显示订阅主题进度
                                    for (; i <= 70; i += 1)
                                    {
                                        OLED_ShowWiFiProgress(i);
                                        delay_ms(20);
                                    }
                                    ESP8266_SendData((unsigned char *)ESP8266_TOPIC);
                                    
                                    for (; i <= 100; i += 1)
                                    {
                                        OLED_ShowWiFiProgress(i);
                                        delay_ms(15);
                                    }
                                    // delay_ms(500);
                                    // 显示连接成功提示
                                    OLED_Clear(0);
                                    OLED_ShowCHinese(0, 0, 0); // 初
                                    OLED_ShowCHinese(18, 0, 1); // 始
                                    OLED_ShowCHinese(36, 0, 2); // 化
                                    OLED_ShowCHinese(54, 0, 38); // 成
                                    OLED_ShowCHinese(72, 0, 39); // 功
                                    OLED_ShowString(0, 3, (u8 *)"WiFi connected", 16);
                                    OLED_ShowString(0, 6, (u8 *)"Bemfa connected", 16);
                                    delay_ms(1500);
                                    return;
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
 * @brief  发送AT指令并等待指定响应
 * @note   核心底层函数，所有AT指令操作基于此
 * @param  cmd: 要发送的AT指令（需带\r\n换行）
 * @param  ack: 期望的响应字符串（如"OK"、"CONNECT"）
 * @param  timeout: 超时时间（ms）
 * @retval 0: 指令执行成功（收到期望响应）；1: 失败（超时/无匹配响应）
 */
uint8_t ESP8266_SendATCmd(char *cmd, char *ack, uint32_t timeout)
{
    ESP8266_RecvLen = 0;          // 清空接收长度
    memset(ESP8266_RecvBuf, 0, buf_len); // 清空接收缓冲区
    ESP8266_Clear();

    // 发送AT指令（通过串口逐字节发送）
    while(*cmd)
    {
        USART_SendData(Bemfa_USART, *cmd++);
        while(USART_GetFlagStatus(Bemfa_USART, USART_FLAG_TXE) == RESET);
    }

    // 等待响应（超时退出）
    while(timeout--)
    {
        delay_ms(1); // 需确保delay_ms函数已实现（STM32毫秒延时）
        // 检查接收缓冲区是否包含期望的响应
        if(strstr((char*)ESP8266_RecvBuf, ack) != NULL)
        {
            return 0; // 匹配到响应，返回成功
        }
    }
    return 1; // 超时未匹配，返回失败
}

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
        if (time_out > 1000) // 10000ms超时
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
    return (ESP8266_SendATCmd(cmd, res, 10000) == 0);
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
 * @brief  连接WiFi网络
 * @note   需确保ESP8266已配置为STA模式
 * @param  ssid: WiFi名称（字符串）
 * @param  pwd: WiFi密码（字符串）
 * @retval 0: 连接成功；1: 连接失败
 */
uint8_t ESP8266_ConnectWiFi(char *ssid, char *pwd)
{
    char at_cmd[64] = {0};

    // 拼接连接WiFi的AT指令：AT+CWJAP="ssid","pwd"\r\n
    sprintf(at_cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);

    // 发送指令，等待"WIFI CONNECTED"响应（超时10秒）
    if(ESP8266_SendATCmd(at_cmd, "WIFI CONNECTED", 10000) == 0)
    {
        // 额外检查是否获取到IP（可选）
        if(ESP8266_SendATCmd("AT+CIFSR\r\n", "STAIP", 2000) == 0)
        {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief  连接巴法云TCP服务器
 * @param  server: 服务器域名/IP
 * @param  port: 服务器端口（如80）
 * @retval 0: 连接成功；1: 连接失败
 */
uint8_t ESP8266_ConnectBafaCloud(char *server, uint16_t port)
{
    char at_cmd[64] = {0};

    // 拼接TCP连接指令：AT+CIPSTART="TCP","server",port\r\n
    sprintf(at_cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", server, port);

    // 发送指令，等待"CONNECT"响应（超时5秒）
    if(ESP8266_SendATCmd(at_cmd, "CONNECT", 5000) == 0)
    {
        return 0;
    }
    return 1;
}

/**
 * @brief  向巴法云TCP服务器发送数据
 * @note   发送流程：AT+CIPSEND=数据长度 -> 等待> -> 发送数据
 * @param  data: 要发送的字符串数据
 * @retval 无
 */
uint8_t ESP8266_SendData(unsigned char *data)
{
    char at_cmd[32] = {0};
    uint16_t data_len = strlen((char *)data);

    // 1. 检查连接状态
    if(ESP8266_SendATCmd("AT\r\n", "OK", 1000) != 0)
    {
        return 1; // ESP8266未响应
    }

    // 2. 发送数据长度指令：AT+CIPSEND=len\r\n
    sprintf(at_cmd, "AT+CIPSEND=%d\r\n", data_len);
    if(ESP8266_SendATCmd(at_cmd, ">", 2000) != 0)
    {
        // 尝试重新建立TCP连接
        ESP8266_SendATCmd(ESP8266_ONENET_INFO, "CONNECT", 5000);
        delay_ms(500);
        // 再次尝试发送数据长度指令
        if(ESP8266_SendATCmd(at_cmd, ">", 2000) != 0)
        {
            return 1; // 等待">"提示符失败
        }
    }

    // 3. 清空接收缓冲区，准备发送数据
    ESP8266_RecvLen = 0;
    memset(ESP8266_RecvBuf, 0, buf_len);

    // 4. 发送实际数据
    while(*data)
    {
        USART_SendData(Bemfa_USART, *data++);
        while(USART_GetFlagStatus(Bemfa_USART, USART_FLAG_TXE) == RESET);
    }

    // 5. 等待"SEND OK"响应（确认数据发送成功）
    if(ESP8266_SendATCmd("", "SEND OK", 3000) != 0)
    {
        return 1; // 发送失败
    }
    
    return 0; // 发送成功
}

/**
 * @brief  读取从巴法云接收的数据
 * @note   将接收缓冲区的数据拷贝到用户缓冲区
 * @param  buf: 用户接收缓冲区
 * @param  len: 要读取的最大长度
 * @retval 0: 读取成功；1: 无数据/长度超限
 */
uint8_t ESP8266_RecvData(char *buf, uint16_t len)
{
    if(ESP8266_RecvLen == 0 || len < ESP8266_RecvLen)
    {
        return 1; // 无数据或用户缓冲区长度不足
    }

    // 拷贝数据到用户缓冲区
    memcpy(buf, ESP8266_RecvBuf, ESP8266_RecvLen);

    // 清空接收缓冲区，准备下一次接收
    ESP8266_RecvLen = 0;
    memset(ESP8266_RecvBuf, 0, buf_len);

    return 0;
}

/**
 * @brief  关闭TCP连接
 * @note   发送AT+CIPCLOSE指令关闭当前TCP连接
 * @param  无
 * @retval 无
 */
void ESP8266_CloseTCP(void)
{
    ESP8266_SendATCmd("AT+CIPCLOSE\r\n", "CLOSED", 2000);
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

/**
 * @brief 显示失败界面和倒计时
 * @param message: 失败信息
 * @param time: 倒计时时间（秒）
 * @return 无
 */
void OLED_ShowFailureWithCountdown(u8 *message, int time)
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
        OLED_ShowString(90, 6, (u8 *)countdown_str, 16);
        delay_ms(1000);
    }

    OLED_Clear(0);

    //显示初始化信息
    OLED_ShowCHinese(0, 3, 21); //正
    OLED_ShowCHinese(18, 3, 22); //在
    OLED_ShowCHinese(36, 3, 23); //连
    OLED_ShowCHinese(54, 3, 24); //接
    OLED_ShowString(72, 3, "WIFI", 16);
    OLED_ShowString(108, 3, "..", 16);
    OLED_ShowCHinese(0, 6, 4); //进
    OLED_ShowCHinese(18, 6, 5); //度
    OLED_ShowCHinese(36, 6, 13); //：
            
}

/**
 * @brief 显示WiFi连接进度
 * @param progress: 进度（0-100）
 * @return 无
 */
void OLED_ShowWiFiProgress(int progress)
{
    // OLED_Clear(0);
    // OLED_ShowCHinese(0, 0, 0); // 初
    // OLED_ShowCHinese(18, 0, 1); // 始
    // OLED_ShowCHinese(36, 0, 2); // 化
    // OLED_ShowCHinese(54, 0, 3); // 中
    // OLED_ShowString(72, 0, (u8 *)"...", 16);
    // OLED_ShowString(0, 3, (u8 *)"WiFi connecting", 16);
    
    // 显示进度百分比
    char progress_str[10];
    sprintf(progress_str, "%d%%", progress);
    OLED_ShowString(60, 6, (u8 *)progress_str, 16);
}