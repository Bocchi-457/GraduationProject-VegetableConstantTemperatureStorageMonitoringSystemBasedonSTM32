#include "esp8266.h"
#include "OLED.h"
#include "delay.h"
#include "stdio.h"
#include "string.h"

// 全局变量定义
unsigned char esp8266_buf[buf_len];           // ESP8266接收缓冲区
unsigned short esp8266_cnt = 0;               // 接收缓冲区计数
unsigned char esp8266_recive_flag = REV_WAIT; // 接收标志

uint8_t ESP8266_RecvBuf[buf_len] = {0}; // 接收缓冲区
uint16_t ESP8266_RecvLen = 0;           // 接收数据长度

/**
 * 【新增】全局连接状态变量定义
 */
volatile ConnState_t g_conn_state = CONN_STATE_IDLE;  // 当前连接状态
uint8_t g_send_fail_count = 0;                        // 连续发送失败计数
uint32_t g_last_reconnect_time = 0;                   // 上次重连时间戳（用于防抖）

// 【新增】引用main.c中的系统时钟
extern volatile uint32_t sys_tick_ms;

/**
 * 【调试开关】串口日志输出控制
 * 定义 DEBUG_RECONNECT 启用重连相关日志
 * 生产环境建议注释掉以提升性能
 */
#define DEBUG_RECONNECT  1  // 1=启用日志, 0=禁用日志

#if DEBUG_RECONNECT
  #define RECONNECT_LOG(fmt, ...) Serial_Printf(fmt, ##__VA_ARGS__)
#else
  #define RECONNECT_LOG(fmt, ...)  // 空宏，不产生任何代码
#endif

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
void USART2_IRQHandler(void) {
  if (USART_GetITStatus(Bemfa_USART, USART_IT_RXNE) != RESET) // 接收中断
  {
    uint8_t data = (uint8_t)USART_ReceiveData(Bemfa_USART);

    // 应用层缓冲区（解析云平台下发的异步业务指令）
    if (esp8266_cnt < buf_len - 1) { // 保留1字节用于终止符
      esp8266_buf[esp8266_cnt++] = data;
      esp8266_buf[esp8266_cnt] = '\0';
    } else {
      // 缓冲区已满，丢弃新数据或可设置溢出标志（这里简单丢弃）
    }

    // 驱动层缓冲区（用于AT应答的同步处理）
    if (ESP8266_RecvLen < buf_len - 1) {
      ESP8266_RecvBuf[ESP8266_RecvLen++] = data;
      ESP8266_RecvBuf[ESP8266_RecvLen] = '\0';
    } else {
      // 驱动缓冲区溢出处理（简单丢弃）
    }

    // 表示接收完成（应用层可读取 local 拷贝）
    esp8266_recive_flag = REV_OK;

    // 清中断标志
    USART_ClearITPendingBit(Bemfa_USART, USART_IT_RXNE);
  }
}

/**
 * @brief  初始化ESP8266通信串口（USART2）
 * @note   配置GPIO、USART外设、中断
 * @param  bound: 波特率
 * @retval 无
 */
static void ESP8266_USART_Init(unsigned int bound) {
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
  // ✅ 提升优先级至(2,2)，确保网络数据接收实时性，与DHT22读取兼容
  NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2; // 抢占优先级2
  NVIC_InitStruct.NVIC_IRQChannelSubPriority = 2;        // 子优先级2
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
void ESP8266_Init(unsigned int bound) {
  // 显示初始化信息
  OLED_ShowCHinese(0, 3, 21);  // 正
  OLED_ShowCHinese(18, 3, 22); // 在
  OLED_ShowCHinese(36, 3, 23); // 连
  OLED_ShowCHinese(54, 3, 24); // 接
  OLED_ShowString(72, 3, "WIFI", 16);
  OLED_ShowString(108, 3, "..", 16);
  OLED_ShowCHinese(0, 6, 4);   // 进
  OLED_ShowCHinese(18, 6, 5);  // 度
  OLED_ShowCHinese(36, 6, 13); // ：
  OLED_ShowString(60, 6, "0%", 16);

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
  while (at_retry > 0) {
    if (ESP8266_SendATCmd("AT\r\n", "OK", 1000) == 0) {
      // 4. 恢复出厂设置（可选，确保初始状态）
      // ESP8266_SendATCmd("AT+RESTORE\r\n", "OK", 3000);

      // 5. 设置ESP8266为STA模式（连接路由器）
      int mode_retry = 3;
      while (mode_retry > 0) {
        if (ESP8266_SendATCmd("AT+CWMODE=1\r\n", "OK", 1000) == 0) {
          // 6. 关闭多路连接（单连接模式，简化TCP操作）
          ESP8266_SendATCmd("AT+CIPMUX=0\r\n", "OK", 1000);

          // 7. 连接WiFi
          int wifi_retry = 3;
          while (wifi_retry > 0) {
            int i = 0;
            // 显示WiFi连接进度
            for (; i <= 30; i += 1) {
              OLED_ShowWiFiProgress(i);
              delay_ms(20);
            }

            if (ESP8266_SendATCmd(ESP8266_WIFI_INFO, "WIFI CONNECTED", 10000) ==
                0) {
              // 额外检查是否获取到IP
              // ESP8266_SendATCmd("AT+CIFSR\r\n", "STAIP", 2000);

              // 等待WiFi连接
              for (; i <= 40; i += 1) {
                OLED_ShowWiFiProgress(i);
                delay_ms(30);
              }

              // 连接巴法云
              int bemfa_retry = 3;
              while (bemfa_retry > 0) {
                // 显示巴法云连接进度
                for (; i <= 60; i += 1) {
                  OLED_ShowWiFiProgress(i);
                  delay_ms(20);
                }
                if (ESP8266_SendATCmd(ESP8266_ONENET_INFO, "CONNECT", 5000) ==
                    0) {
                  // 订阅主题
                  // 显示订阅主题进度
                  for (; i <= 70; i += 1) {
                    OLED_ShowWiFiProgress(i);
                    delay_ms(20);
                  }
                  ESP8266_SendData((unsigned char *)ESP8266_TOPIC);

                  for (; i <= 100; i += 1) {
                    OLED_ShowWiFiProgress(i);
                    delay_ms(15);
                  }
                  // 显示连接成功提示
                  OLED_Clear(0);
                  OLED_ShowCHinese(0, 0, 56);  // 联
                  OLED_ShowCHinese(18, 0, 57); // 网
                  OLED_ShowCHinese(36, 0, 38); // 成
                  OLED_ShowCHinese(54, 0, 39); // 功
                  OLED_ShowString(0, 3, (u8 *)"WiFi connected", 16);
                  OLED_ShowString(0, 6, (u8 *)"Bemfa connected", 16);
                  delay_ms(1500);
                  OLED_Clear(0);
                  return;
                } else {
                  // 连接巴法云失败
                  OLED_ShowFailureWithCountdown((u8 *)"Bemfa connect failed",
                                                5);
                  bemfa_retry--;
                }
              }
              if (bemfa_retry <= 0) {
                OLED_ShowFailureWithCountdown("Bemfa connect failed", 5);
                break;
              }
            } else {
              // 连接WiFi失败
              OLED_ShowFailureWithCountdown((u8 *)"WiFi connect failed", 5);
              wifi_retry--;
            }
          }
          if (wifi_retry <= 0) {
            OLED_ShowFailureWithCountdown("WiFi connect failed", 5);
            break;
          }
        } else {
          // 设置WiFi模式失败
          OLED_ShowFailureWithCountdown((u8 *)"WiFi mode setting failed", 5);
          mode_retry--;
        }
      }
      if (mode_retry <= 0) {
        OLED_ShowFailureWithCountdown("WiFi mode setting failed", 5);
        break;
      }
    } else {
      // AT指令测试失败
      OLED_ShowFailureWithCountdown((u8 *)"AT command test failed", 5);
      at_retry--;
    }
  }
  if (at_retry <= 0) {
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
uint8_t ESP8266_SendATCmd(char *cmd, char *ack, uint32_t timeout) {
  // 只清空驱动层缓冲区，不影响应用层缓冲区（云平台指令）
  // 避免在发送AT指令时误删正在接收的云平台下行指令
  ESP8266_RecvLen = 0;
  memset(ESP8266_RecvBuf, 0, buf_len);

  // 发送AT指令（通过串口逐字节发送）
  while (*cmd) {
    USART_SendData(Bemfa_USART, *cmd++);
    while (USART_GetFlagStatus(Bemfa_USART, USART_FLAG_TXE) == RESET)
      ;
  }

  // 等待响应（超时退出）
  while (timeout--) {
    delay_ms(1);
    // 检查驱动层缓冲区是否包含期望的响应
    if (strstr((char *)ESP8266_RecvBuf, ack) != NULL) {
      return 0; // 匹配到响应，返回成功
    }
  }
  return 1; // 超时未匹配，返回失败
}

/**
 * @brief 清空ESP8266接收缓冲区
 * @note  原子性清空所有相关接收缓冲区，防止状态不同步
 * @param 无
 * @return 无
 */
void ESP8266_Clear(void) {
  // 同时清空应用层缓冲区（云平台指令）
  memset(esp8266_buf, 0, sizeof(esp8266_buf));
  esp8266_cnt = 0;

  // 同时清空驱动层缓冲区（AT响应）
  memset(ESP8266_RecvBuf, 0, buf_len);
  ESP8266_RecvLen = 0;

  // 重置接收标志
  esp8266_recive_flag = REV_WAIT;
}

/**
 * @brief 等待ESP8266接收数据
 * @param 无
 * @return 接收完成返回1，超时返回0
 */
_Bool ESP8266_WaitRecive(void) {
  unsigned char time_out = 0;
  while (esp8266_recive_flag == REV_WAIT) {
    delay_ms(10);
    time_out++;
    if (time_out > 100) // 1000ms超时
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
_Bool ESP8266_SendCmd(char *cmd, char *res) {
  return (ESP8266_SendATCmd(cmd, res, 10000) == 0);
}

/**
 * @brief 串口发送字符串
 * @param USARTx: 串口编号
 * @param str: 要发送的字符串
 * @param len: 字符串长度
 * @return 无
 */
void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str,
                      unsigned short len) {
  unsigned short i;
  for (i = 0; i < len; i++) {
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET)
      ;
    USART_SendData(USARTx, str[i]);
  }
  while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET)
    ;
}

/**
 * @brief  向巴法云TCP服务器发送数据（优化版）
 * @param  data: 要发送的字符串数据
 * @retval 0: 发送成功；1: 发送失败（需进一步诊断）
 * @note   增加重试机制和详细的状态反馈
 */
uint8_t ESP8266_SendData(unsigned char *data) {
  char at_cmd[32] = {0};
  uint16_t data_len = strlen((char *)data);
  uint8_t retry_count = 0;
  const uint8_t MAX_RETRY = 2;  // 最多重试2次
  
  while (retry_count <= MAX_RETRY) {
    // 发送数据长度指令：AT+CIPSEND=len\r\n
    sprintf(at_cmd, "AT+CIPSEND=%d\r\n", data_len);

    if (ESP8266_SendATCmd(at_cmd, ">", 1000) != 0) {
      // 未收到 ">" 提示符，TCP连接可能已断开
      RECONNECT_LOG("[SEND] Failed to get '>' prompt\r\n");
      
      if (retry_count < MAX_RETRY) {
        retry_count++;
        RECONNECT_LOG("[SEND] Retry %d/%d...\r\n", retry_count, MAX_RETRY);
        delay_ms(50);  // 短暂延时后重试
        continue;  // 重试
      }
      
      return 1; // 重试后仍失败
    }

    // 只清空驱动层缓冲区，保护应用层缓冲区的云平台指令
    ESP8266_RecvLen = 0;
    memset(ESP8266_RecvBuf, 0, buf_len);
    // 注意：不清空 esp8266_buf 和 esp8266_cnt

    // 发送实际数据（逐字节发送）
    const unsigned char *pData = data;  // 使用临时指针
    while (*pData) {
      USART_SendData(Bemfa_USART, *pData++);
      while (USART_GetFlagStatus(Bemfa_USART, USART_FLAG_TXE) == RESET);
    }

    // 等待 "SEND OK" 响应（超时1.5秒）
    if (ESP8266_SendATCmd("", "SEND OK", 1500) != 0) {
      RECONNECT_LOG("[SEND] Failed to get 'SEND OK'\r\n");
      
      if (retry_count < MAX_RETRY) {
        retry_count++;
        RECONNECT_LOG("[SEND] Retry %d/%d...\r\n", retry_count, MAX_RETRY);
        delay_ms(50);
        continue;  // 重试
      }
      
      return 1; // 发送失败
    }

    // 发送成功
    RECONNECT_LOG("[SEND] Data sent successfully\r\n");
    return 0;
  }
  
  return 1;  // 理论上不会到达这里
}

/**
 * @brief 显示失败界面和倒计时
 * @param message: 失败信息
 * @param time: 倒计时时间（秒）
 * @return 无
 */
void OLED_ShowFailureWithCountdown(u8 *message, int time) {
  OLED_Clear(0);

  OLED_ShowCHinese(0, 0, 56);  // 联
  OLED_ShowCHinese(18, 0, 57); // 网
  OLED_ShowCHinese(54, 0, 36); // 失
  OLED_ShowCHinese(72, 0, 37); // 败
  OLED_ShowString(0, 3, message, 16);

  char countdown_str[10];
  for (int i = time; i > 0; i--) {
    sprintf(countdown_str, "%ds", i);
    OLED_ShowString(90, 6, (u8 *)countdown_str, 16);
    delay_ms(1000);
  }

  OLED_Clear(0);

  // 显示初始化信息
  OLED_ShowCHinese(0, 3, 21);  // 正
  OLED_ShowCHinese(18, 3, 22); // 在
  OLED_ShowCHinese(36, 3, 23); // 连
  OLED_ShowCHinese(54, 3, 24); // 接
  OLED_ShowString(72, 3, "WIFI", 16);
  OLED_ShowString(108, 3, "..", 16);
  OLED_ShowCHinese(0, 6, 4);   // 进
  OLED_ShowCHinese(18, 6, 5);  // 度
  OLED_ShowCHinese(36, 6, 13); // ：
}

/**
 * @brief 显示WiFi连接进度
 * @param progress: 进度（0-100）
 * @return 无
 */
void OLED_ShowWiFiProgress(int progress) {
  // 显示进度百分比
  char progress_str[10];
  sprintf(progress_str, "%d%%", progress);
  OLED_ShowString(60, 6, (u8 *)progress_str, 16);
}

/**
 * @brief 【新增】获取当前连接状态
 * @param 无
 * @return 当前连接状态枚举值
 */
ConnState_t ESP8266_GetConnState(void) {
  return g_conn_state;
}

/**
 * @brief 【新增】更新连接状态
 * @param state: 新的连接状态
 * @return 无
 */
void ESP8266_UpdateConnState(ConnState_t state) {
  g_conn_state = state;
  RECONNECT_LOG("[CONN] State changed to: %d\r\n", state);
}

/**
 * @brief 【新增】诊断连接状态（非阻塞，需配合状态机使用）
 * @note 通过AT+CIPSTATUS检测TCP连接状态
 * @return 诊断结果枚举值
 */
DiagResult_t ESP8266_DiagnoseConnection(void) {
  char response_buf[64] = {0};
  
  // 步骤1：检测TCP连接状态
  ESP8266_RecvLen = 0;
  memset(ESP8266_RecvBuf, 0, buf_len);
  
  if (ESP8266_SendATCmd("AT+CIPSTATUS\r\n", "STATUS:", 1000) == 0) {
    // 解析响应：+CIPSTATUS:<link ID>,<type>,<remote IP>,<remote port>,<local port>,<tetype>
    // STATUS:2 表示已连接，STATUS:3/4 表示断开
    if (strstr((char*)ESP8266_RecvBuf, "STATUS:2") != NULL) {
      RECONNECT_LOG("[DIAG] TCP connected\r\n");
      return DIAG_OK;  // TCP连接正常，可能是网络拥塞
    } else if (strstr((char*)ESP8266_RecvBuf, "STATUS:3") != NULL ||
               strstr((char*)ESP8266_RecvBuf, "STATUS:4") != NULL) {
      RECONNECT_LOG("[DIAG] TCP disconnected\r\n");
      return DIAG_TCP_DISCONNECTED;  // TCP断开
    }
  }
  
  // 步骤2：如果CIPSTATUS超时或无响应，检测WiFi状态
  ESP8266_RecvLen = 0;
  memset(ESP8266_RecvBuf, 0, buf_len);
  
  if (ESP8266_SendATCmd("AT+CWJAP?\r\n", "+CWJAP:", 1000) == 0) {
    // WiFi仍连接
    RECONNECT_LOG("[DIAG] WiFi connected, but TCP lost\r\n");
    return DIAG_TCP_DISCONNECTED;
  } else {
    // WiFi可能断开
    RECONNECT_LOG("[DIAG] WiFi disconnected\r\n");
    return DIAG_WIFI_DISCONNECTED;
  }
}

/**
 * @brief 【新增】TCP层重连（非阻塞状态机实现）
 * @note 需要多次调用才能完成重连流程
 * @return 0=重连成功, 1=重连中, 2=重连失败
 */
uint8_t ESP8266_TCP_Reconnect(void) {
  static uint8_t step = 0;
  static uint32_t step_start_time = 0;
  uint32_t current_time = sys_tick_ms;  // 假设main.c中有sys_tick_ms
  
  switch (step) {
    case 0:  // 步骤0：检查防抖（距离上次重连至少60秒）
      if ((current_time - g_last_reconnect_time) < 60000) {
        RECONNECT_LOG("[TCP] Reconnect blocked by debounce\r\n");
        return 2;  // 被防抖阻止
      }
      
      RECONNECT_LOG("[TCP] Starting TCP reconnect...\r\n");
      ESP8266_UpdateConnState(CONN_STATE_TCP_CONNECTING);
      step = 1;
      step_start_time = current_time;
      break;
      
    case 1:  // 步骤1：关闭现有TCP连接
      ESP8266_RecvLen = 0;
      memset(ESP8266_RecvBuf, 0, buf_len);
      ESP8266_SendATCmd("AT+CIPCLOSE\r\n", "CLOSED", 1000);
      delay_ms(200);
      
      step = 2;
      step_start_time = current_time;
      RECONNECT_LOG("[TCP] Step 1: Close old connection\r\n");
      break;
      
    case 2:  // 步骤2：重新建立TCP连接
      if (ESP8266_SendATCmd(ESP8266_ONENET_INFO, "CONNECT", 5000) == 0) {
        RECONNECT_LOG("[TCP] Step 2: TCP connected\r\n");
        
        // 步骤3：重新订阅主题
        step = 3;
      } else {
        // TCP连接失败，重试
        if ((current_time - step_start_time) > 15000) {  // 15秒超时
          RECONNECT_LOG("[TCP] TCP connect failed after retry\r\n");
          step = 0;  // 重置状态
          ESP8266_UpdateConnState(CONN_STATE_TCP_LOST);
          return 2;  // 重连失败
        }
        RECONNECT_LOG("[TCP] Step 2: Retrying TCP connect...\r\n");
      }
      break;
      
    case 3:  // 步骤3：重新订阅主题
      if (ESP8266_SendData((unsigned char *)ESP8266_TOPIC) == 0) {
        RECONNECT_LOG("[TCP] Step 3: Topic subscribed\r\n");
        
        // 重连成功
        step = 0;  // 重置状态
        g_send_fail_count = 0;  // 清零失败计数
        g_last_reconnect_time = current_time;
        ESP8266_UpdateConnState(CONN_STATE_CONNECTED);
        
        RECONNECT_LOG("[TCP] Reconnect SUCCESS\r\n");
        return 0;  // 重连成功
      } else {
        RECONNECT_LOG("[TCP] Step 3: Subscribe failed, retrying...\r\n");
        if ((current_time - step_start_time) > 10000) {  // 10秒超时
          step = 0;
          ESP8266_UpdateConnState(CONN_STATE_TCP_LOST);
          return 2;  // 重连失败
        }
      }
      break;
      
    default:
      step = 0;
      break;
  }
  
  return 1;  // 重连中
}

/**
 * @brief 【新增】WiFi层重连（非阻塞状态机实现）
 * @note 需要多次调用才能完成重连流程
 * @return 0=重连成功, 1=重连中, 2=重连失败
 */
uint8_t ESP8266_WIFI_Reconnect(void) {
  static uint8_t step = 0;
  static uint32_t step_start_time = 0;
  uint32_t current_time = sys_tick_ms;
  
  switch (step) {
    case 0:  // 步骤0：检查防抖
      if ((current_time - g_last_reconnect_time) < 60000) {
        RECONNECT_LOG("[WIFI] Reconnect blocked by debounce\r\n");
        return 2;
      }
      
      RECONNECT_LOG("[WIFI] Starting WiFi reconnect...\r\n");
      ESP8266_UpdateConnState(CONN_STATE_WIFI_CONNECTING);
      step = 1;
      step_start_time = current_time;
      break;
      
    case 1:  // 步骤1：重新连接WiFi
      ESP8266_RecvLen = 0;
      memset(ESP8266_RecvBuf, 0, buf_len);
      
      if (ESP8266_SendATCmd(ESP8266_WIFI_INFO, "WIFI CONNECTED", 10000) == 0) {
        RECONNECT_LOG("[WIFI] Step 1: WiFi connected\r\n");
        delay_ms(1000);  // 等待获取IP
        
        step = 2;
      } else {
        // WiFi连接失败，重试
        if ((current_time - step_start_time) > 30000) {  // 30秒超时
          RECONNECT_LOG("[WIFI] WiFi connect failed after retry\r\n");
          step = 0;
          ESP8266_UpdateConnState(CONN_STATE_WIFI_LOST);
          return 2;  // 重连失败
        }
        RECONNECT_LOG("[WIFI] Step 1: Retrying WiFi connect...\r\n");
      }
      break;
      
    case 2:  // 步骤2：重建TCP连接（调用TCP重连）
      {
        uint8_t tcp_result = ESP8266_TCP_Reconnect();
        if (tcp_result == 0) {
          // TCP重连成功
          step = 0;
          g_last_reconnect_time = current_time;
          ESP8266_UpdateConnState(CONN_STATE_CONNECTED);
          
          RECONNECT_LOG("[WIFI] Full reconnect SUCCESS\r\n");
          return 0;  // 重连成功
        } else if (tcp_result == 2) {
          // TCP重连失败
          step = 0;
          ESP8266_UpdateConnState(CONN_STATE_TCP_LOST);
          return 2;
        }
        // tcp_result == 1 表示重连中，继续等待
      }
      break;
      
    default:
      step = 0;
      break;
  }
  
  return 1;  // 重连中
}

/**
 * @brief 【新增】模块复位重连（最激进的重连方式）
 * @note 通过硬件复位引脚重启ESP8266，然后重新初始化
 * @return 0=重连成功, 1=重连中, 2=重连失败
 */
uint8_t ESP8266_Module_Reset(void) {
  static uint8_t step = 0;
  static uint32_t step_start_time = 0;
  uint32_t current_time = sys_tick_ms;
  
  switch (step) {
    case 0:  // 步骤0：检查防抖（复位需要更长的间隔，至少120秒）
      if ((current_time - g_last_reconnect_time) < 120000) {
        RECONNECT_LOG("[RESET] Reset blocked by debounce\r\n");
        return 2;
      }
      
      RECONNECT_LOG("[RESET] Starting module reset...\r\n");
      ESP8266_UpdateConnState(CONN_STATE_MODULE_ERROR);
      step = 1;
      step_start_time = current_time;
      break;
      
    case 1:  // 步骤1：硬件复位
      GPIO_ResetBits(ESP01S_RST_PROT, ESP01S_RST_PIN);
      delay_ms(500);
      GPIO_SetBits(ESP01S_RST_PROT, ESP01S_RST_PIN);
      delay_ms(2000);  // 等待模块启动
      
      RECONNECT_LOG("[RESET] Step 1: Hardware reset done\r\n");
      step = 2;
      break;
      
    case 2:  // 步骤2：重新初始化（调用ESP8266_Init的部分逻辑）
      // 注意：这里不调用完整的ESP8266_Init，避免OLED显示干扰
      // 只执行必要的AT指令序列
      
      // 2.1 测试AT
      if (ESP8266_SendATCmd("AT\r\n", "OK", 1000) != 0) {
        if ((current_time - step_start_time) > 10000) {
          RECONNECT_LOG("[RESET] AT test failed\r\n");
          step = 0;
          return 2;
        }
        RECONNECT_LOG("[RESET] Step 2: Retrying AT test...\r\n");
        break;
      }
      
      // 2.2 设置STA模式
      if (ESP8266_SendATCmd("AT+CWMODE=1\r\n", "OK", 1000) != 0) {
        RECONNECT_LOG("[RESET] Set STA mode failed\r\n");
        step = 0;
        return 2;
      }
      
      // 2.3 关闭多路连接
      ESP8266_SendATCmd("AT+CIPMUX=0\r\n", "OK", 1000);
      
      RECONNECT_LOG("[RESET] Step 2: Module initialized\r\n");
      step = 3;
      break;
      
    case 3:  // 步骤3：重新连接WiFi
      {
        uint8_t wifi_result = ESP8266_WIFI_Reconnect();
        if (wifi_result == 0) {
          // WiFi重连成功
          step = 0;
          g_last_reconnect_time = current_time;
          ESP8266_UpdateConnState(CONN_STATE_CONNECTED);
          
          RECONNECT_LOG("[RESET] Full module reset SUCCESS\r\n");
          return 0;  // 重连成功
        } else if (wifi_result == 2) {
          // WiFi重连失败
          step = 0;
          return 2;
        }
        // wifi_result == 1 表示重连中
      }
      break;
      
    default:
      step = 0;
      break;
  }
  
  return 1;  // 重连中
}
