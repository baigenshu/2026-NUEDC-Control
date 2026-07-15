#include "bsp_systick.h"

uint32_t Systick_getTick(void)
{
    return (SysTick->VAL);
}

void delay_ms(uint32_t ms)
{
    /* delay_us 单次上限约 209ms @80MHz，长延时分段 */
    while (ms > 200) {
        delay_us(200000);
        ms -= 200;
    }
    if (ms)
        delay_us(ms * 1000);
}

void delay_us(uint32_t us)
{
    if (us > SysTickMAX_COUNT / (SysTickFre / 1000000))
        us = SysTickMAX_COUNT / (SysTickFre / 1000000);

    us = us * (SysTickFre / 1000000);

    uint32_t runningtime = 0;
    uint32_t InserTick   = Systick_getTick();
    uint32_t tick        = 0;
    uint8_t  countflag   = 0;

    while (1) {
        tick = Systick_getTick();
        if (tick > InserTick)
            countflag = 1;

        if (countflag)
            runningtime = InserTick + SysTickMAX_COUNT - tick;
        else
            runningtime = InserTick - tick;

        if (runningtime >= us)
            break;
    }
}
