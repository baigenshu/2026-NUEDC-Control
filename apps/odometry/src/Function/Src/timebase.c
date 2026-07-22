#include "timebase.h"
#include "bsp_systick.h"

static uint32_t s_last_val;
static uint32_t s_ms_accum;
static uint32_t s_cycle_accum;
static uint32_t s_last_ms_for_dt;

static void pump(void)
{
    uint32_t now = Systick_getTick();
    uint32_t elapsed;

    if (s_last_val >= now)
        elapsed = s_last_val - now;
    else
        elapsed = s_last_val + (SysTickMAX_COUNT + 1U) - now;

    s_last_val = now;
    s_cycle_accum += elapsed;

    while (s_cycle_accum >= (SysTickFre / 1000U)) {
        s_cycle_accum -= (SysTickFre / 1000U);
        s_ms_accum++;
    }
}

void Timebase_Init(void)
{
    s_last_val       = Systick_getTick();
    s_ms_accum       = 0;
    s_cycle_accum    = 0;
    s_last_ms_for_dt = 0;
}

uint32_t millis(void)
{
    pump();
    return s_ms_accum;
}

float Timebase_GetDeltaSec(void)
{
    uint32_t now = millis();
    uint32_t dms = now - s_last_ms_for_dt;
    s_last_ms_for_dt = now;
    return (float)dms * 0.001f;
}
