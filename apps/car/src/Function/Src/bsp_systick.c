#include "bsp_systick.h"

/* SysTick: 24-bit down-counter, period = LOAD+1 = 0x1000000 @ 80MHz ≈ 209.7ms */

uint32_t Systick_getTick(void)
{
    return SysTick->VAL;
}

void delay_us(uint32_t us)
{
    const uint32_t period = SysTickMAX_COUNT + 1U; /* full wrap length */
    /* keep each wait under one wrap (leave margin) */
    const uint32_t max_us = (period / (SysTickFre / 1000000U)) - 10U;

    while (us > max_us) {
        delay_us(max_us);
        us -= max_us;
    }
    if (us == 0)
        return;

    {
        uint32_t cycles = us * (SysTickFre / 1000000U);
        uint32_t start  = SysTick->VAL;
        uint32_t now, elapsed;

        while (1) {
            now = SysTick->VAL;
            if (start >= now)
                elapsed = start - now;
            else
                elapsed = start + period - now;

            if (elapsed >= cycles)
                break;
        }
    }
}

void delay_ms(uint32_t ms)
{
    while (ms--)
        delay_us(1000);
}
