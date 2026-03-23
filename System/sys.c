#include "sys.h"

/**
 * NVIC中断优先级配置函数
 * 功能：配置NVIC中断优先级分组
 * 说明：设置为分组2，即2位抢占优先级，2位响应优先级
 * 抢占优先级：决定是否能打断其他中断
 * 响应优先级：决定同时发生时的执行顺序
 */
void NVIC_Configuration(void)
{
    //配置NVIC中断分组2:2位抢占优先级，2位响应优先级
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

}
