/**
 * Timer.c
 * 定时器驱动实现文件
 * 功能：实现定时器初始化和中断处理函数
 * 版本：V1.0
 * 测试硬件：STM32F103RCT6
 */

#include "timer.h"     // Device header
#include "stm32f10x.h" // Device header


char TIMER_IT = 0;                      // 定时器中断标志

/**
 * 全局毫秒计数器（由TIM2中断每10ms递增）
 * @note 其他模块通过extern访问，遵循单一定义原则
 */
volatile uint32_t sys_tick_ms = 0;      // 全局毫秒计数器

/**
 * @brief 初始化定时器
 * @param 无
 * @return 无
 * @note 初始化TIM2定时器，配置为10毫秒中断一次
 *       时钟频率：72MHz / 7200 / 100 = 100Hz (10ms)
 *       PSC=7200-1, ARR=100-1 → 72MHz/(7200*100) = 100Hz
 */
void Timer_Init(void) {
  TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  // 开启TIM2时钟
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

  // 配置TIM2为内部时钟
  TIM_InternalClockConfig(TIM2);

  // 配置时基单元
  TIM_TimeBaseInitStructure.TIM_ClockDivision =
      TIM_CKD_DIV1; // 时钟分频，选择不分频
  TIM_TimeBaseInitStructure.TIM_CounterMode =
      TIM_CounterMode_Up; // 计数器模式，选择向上计数
  
  // ✅ 修改为10ms周期：72MHz / 7200 / 100 = 100Hz (10ms)
  TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;    // ARR = 100
  TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1; // PSC = 7200

  TIM_TimeBaseInitStructure.TIM_RepetitionCounter =
      0; // 重复计数器，高级定时器才会用到
  TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure); // 配置TIM2的时基单元

  // 清除定时器更新标志位
  TIM_ClearFlag(TIM2, TIM_FLAG_Update);
  // 开启TIM2的更新中断
  TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

  // 配置NVIC中断分组
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

  // 配置NVIC
  NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn; // 选择配置NVIC的TIM2线
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // 指定NVIC线路使能
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
      0; // ✅ 最高抢占优先级，确保系统时间基准不被阻塞
  NVIC_InitStructure.NVIC_IRQChannelSubPriority =
      0;                          // 指定NVIC线路的响应优先级为0
  NVIC_Init(&NVIC_InitStructure); // 配置NVIC外设

  // 使能TIM2，定时器开始运行
  TIM_Cmd(TIM2, ENABLE);
}

// TIM2 中断服务函数

void TIM2_IRQHandler(void) {
  if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET) {
    TIMER_IT = 1;       // 标志位，每10ms置1一次
    sys_tick_ms += 10;  // ✅ 每10毫秒增加10毫秒
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
  }
}

/**
 * @brief TIM2中断函数
 * @param 无
 * @return 无
 * @note 此函数为中断函数，无需调用，中断触发后自动执行
 *       函数名为预留的指定名称，可以从启动文件复制
 *       请确保函数名正确，不能有任何差异，否则中断函数将不能进入
 */
// void TIM2_IRQHandler(void)
// {
// 	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
// 	{
// 		// 在这里添加中断处理代码

// 		TIM_ClearITPendingBit(TIM2, TIM_IT_Update); // 清除中断标志位
// 	}
// }

/***************************** 结束 *****************************/
