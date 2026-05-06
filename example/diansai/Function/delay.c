#include "ti_msp_dl_config.h"
#include <stdint.h>

/* 毫秒计数器，供 MPU6050 计算 dt 使用 */
volatile uint32_t g_sysTick = 0;

/**
  * @brief  初始化 SysTick 为 1ms 周期性中断模式
  *         产生 g_sysTick 毫秒计数器
  */
void SysTick_Init(void)
{
    /* SysTick 每毫秒中断一次：SystemCoreClock / 1000 */
    SysTick->LOAD = (CPUCLK_FREQ / 1000) - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 0x00000007;  // 使能，使用处理器时钟，使能中断
}

/**
  * @brief  SysTick 中断处理函数（名称来自 startup 文件）
  *         每 1ms 进入一次，递增毫秒计数器
  */
void SysTick_Handler(void)
{
    g_sysTick++;
}

/**
  * @brief  微秒级延时（使用 SysTick 轮询模式）
  * @param  xus 延时时长，范围：0 ~ (2^24-1) / (SystemCoreClock/1000000)
  *        例如 SystemCoreClock = 32MHz 时，最大约 524287 微秒
  * @retval 无
  * @note   延时期间不关闭全局中断，但 SysTick 中断被禁止。
  */
void Delay_us(uint32_t xus)
{
    uint32_t ticks;

    // 计算需要的 SysTick 重载值：每微秒的时钟周期数 × 微秒数
    // 每微秒周期数 = SystemCoreClock / 1000000
    ticks = (CPUCLK_FREQ / 1000000) * xus;

    // 检查是否超过 24 位计数器的最大值（0xFFFFFF）
    if (ticks > 0xFFFFFF)
    {
        ticks = 0xFFFFFF;
    }

    SysTick->LOAD = ticks;          // 设置重载值
    SysTick->VAL  = 0x00;           // 清空当前计数值
    SysTick->CTRL = 0x00000005;     // 使能定时器，时钟源=处理器时钟，不产生中断

    while (!(SysTick->CTRL & 0x00010000)); // 等待 COUNTFLAG 标志位置位

    SysTick->CTRL = 0x00000004;     // 关闭定时器（ENABLE=0）
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t ms)
{
    while (ms--)
    {
        Delay_us(1000);
    }
}

/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t s)
{
    while (s--)
    {
        Delay_ms(1000);
    }
}