#include "delay.h"

static volatile uint32_t _millis = 0;

void systick_init(void)
{
    SysTick->LOAD = (CPUCLK_FREQ / 1000) - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;
}

uint32_t millis(void)
{
    return _millis;
}

void SysTick_Handler(void)
{
    _millis++;
}

/* 与官方 15_step_motor_2 相同 */
void delay_ms(uint32_t ms)
{
    uint32_t cycles = (CPUCLK_FREQ / 1000) * ms;
    delay_cycles(cycles);
}

void delay_us(uint32_t us)
{
    uint32_t cycles = (CPUCLK_FREQ / 1000000U) * us;
    delay_cycles(cycles);
}
