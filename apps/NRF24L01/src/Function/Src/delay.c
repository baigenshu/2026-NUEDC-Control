#include "delay.h"

void delay_ms(uint32_t ms)
{
    uint32_t cycles = (CPUCLK_FREQ / 1000U) * ms;
    delay_cycles(cycles);
}

void delay_us(uint32_t us)
{
    uint32_t cycles = (CPUCLK_FREQ / 1000000U) * us;
    delay_cycles(cycles);
}
