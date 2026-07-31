/**
 * @file main.c
 * @brief 四路红外巡线 · 四轮独立速度 PID 闭环
 *
 * KEY_RUN PA17：巡线开/关
 * KEY_SPD PA15：base speed 0→SPD1→SPD2→SPD3→0
 * IR p1..p4 = PB19/PB17/PA16/PA14（G1–G4）
 *
 * OLED：
 *   L1  SPD:+xxxx
 *   L2  IR:xxxx E:±xx
 *   L3  A:+xxx B:+xxx
 *   L4  TRK / OFF Gx
 */
#include "ti_msp_dl_config.h"
#include "motor.h"
#include "motor_cfg.h"
#include "encoder.h"
#include "key.h"
#include "ir4.h"
#include "line_follow.h"
#include "line_follow_cfg.h"
#include "OLED.h"
#include "uart_debug.h"
#include <stdbool.h>

#ifndef SYSTICK_HZ
#define SYSTICK_HZ 80000000u
#endif

#ifndef SAMPLE_MS
#define SAMPLE_MS 10u
#endif

#ifndef OLED_REFRESH_MS
#define OLED_REFRESH_MS 100u
#endif

#ifndef HB_PERIOD_MS
#define HB_PERIOD_MS 1000u
#endif

static const int16_t s_gears[SPD_GEAR_COUNT] = {
    SPD_GEAR_0, SPD_GEAR_1, SPD_GEAR_2, SPD_GEAR_3,
};

static uint32_t s_ms;
static uint32_t s_last_systick;
static uint32_t s_cycle_accum;

static int32_t s_ea, s_eb, s_ec, s_ed;
static int16_t s_spd;
static uint8_t s_gear_idx;
static uint8_t s_ui_dirty;

static void timebase_init(void)
{
    s_ms = 0;
    s_last_systick = SysTick->VAL;
    s_cycle_accum = 0;
}

static uint32_t millis(void)
{
    uint32_t now = SysTick->VAL;
    uint32_t elapsed;

    if (s_last_systick >= now)
        elapsed = s_last_systick - now;
    else
        elapsed = s_last_systick + (SysTick->LOAD + 1u) - now;

    s_last_systick = now;
    s_cycle_accum += elapsed;
    while (s_cycle_accum >= (SYSTICK_HZ / 1000u)) {
        s_cycle_accum -= (SYSTICK_HZ / 1000u);
        s_ms++;
    }
    return s_ms;
}

static void on_key_run(void)
{
    bool on = !LineFollow_IsEnabled();
    LineFollow_SetBaseSpd(s_spd);
    LineFollow_SetEnable(on);
    UartDebug_Printf("KEY_RUN -> %s spd=%d\n", on ? "ON" : "OFF", (int)s_spd);
    s_ui_dirty = 1u;
}

static void on_key_spd(void)
{
    s_gear_idx = (uint8_t)((s_gear_idx + 1u) % SPD_GEAR_COUNT);
    s_spd = s_gears[s_gear_idx];
    LineFollow_SetBaseSpd(s_spd);
    UartDebug_Printf("KEY_SPD -> g=%u spd=%d\n", (unsigned)s_gear_idx, (int)s_spd);
    s_ui_dirty = 1u;
}

static int32_t clamp_show(int32_t v, int32_t lim)
{
    if (v > lim)
        return lim;
    if (v < -lim)
        return -lim;
    return v;
}

static void ui_draw_static(void)
{
    OLED_Clear();
    OLED_ShowString(1, 1, "SPD:");
    OLED_ShowString(2, 1, "IR:");
    OLED_ShowString(2, 9, "E:");
    OLED_ShowString(3, 1, "A:");
    OLED_ShowString(3, 9, "B:");
}

static const char *state_text(lf_state_t st)
{
    return (st == LF_STATE_TRACK) ? "TRK G" : "OFF G";
}

static void ui_refresh(void)
{
    uint8_t  mask = LineFollow_GetMask();
    float    err_f = LineFollow_GetError();
    int32_t  err;
    int32_t  a = clamp_show(s_ea, 999);
    int32_t  b = clamp_show(s_eb, 999);
    int32_t  c = clamp_show(s_ec, 999);
    int32_t  d = clamp_show(s_ed, 999);
    int32_t  spd = (int32_t)s_spd;
    char     bits[5];
    uint8_t  i;

    if (spd > 9999)  spd = 9999;
    if (spd < -9999) spd = -9999;

    if (err_f <= -90.f || err_f >= 90.f)
        err = (int32_t)err_f;
    else
        err = (int32_t)(err_f * 10.f);
    err = clamp_show(err, 99);

    OLED_ShowSignedNum(1, 5, spd, 4);

    OLED_ShowSignedNum(2, 3, a, 3);
    OLED_ShowSignedNum(2, 11, b, 3);
    OLED_ShowSignedNum(3, 3, c, 3);
    OLED_ShowSignedNum(3, 11, d, 3);

    for (i = 0; i < 4u; ++i)
        bits[i] = (mask & (1u << i)) ? '1' : '0';
    bits[4] = '\0';
    OLED_ShowString(2, 4, bits);
    OLED_ShowSignedNum(2, 11, err, 2);

    OLED_ShowString(4, 1, state_text(LineFollow_GetState()));
    OLED_ShowNum(4, 7, (uint32_t)s_gear_idx, 1);
}

int main(void)
{
    uint32_t t_prev;
    uint32_t t_oled;
    uint32_t t_hb;
    uint32_t now;
    uint32_t hb_cnt;

    s_gear_idx = 1;
    s_spd = s_gears[s_gear_idx];
    s_ui_dirty = 0;
    hb_cnt = 0;

    SYSCFG_DL_init();
    timebase_init();

    UartDebug_Init();
    OLED_Init();
    Motor_Init();
    Encoder_Init();
    Key_Init();
    LineFollow_Init();
    LineFollow_SetBaseSpd(s_spd);

    UartDebug_Puts("line_track SPD-CTRL boot\n");
    UartDebug_Printf("RUN=PA17 SPD=PA15 init_spd=%d\n", (int)s_spd);

    ui_draw_static();
    ui_refresh();

    t_prev = millis();
    t_oled = t_prev;
    t_hb   = t_prev;

    for (;;) {
        now = millis();

        if (Key_PollPress(KEY_ID_RUN, now))
            on_key_run();
        if (Key_PollPress(KEY_ID_SPD, now))
            on_key_spd();

        if ((now - t_prev) >= SAMPLE_MS) {
            t_prev = now;
            LineFollow_Update();
            LineFollow_GetEncoderDeltas(&s_ea, &s_eb, &s_ec, &s_ed);
        }

        if (s_ui_dirty || (now - t_oled) >= OLED_REFRESH_MS) {
            t_oled = now;
            s_ui_dirty = 0;
            ui_refresh();
        }

        if ((now - t_hb) >= HB_PERIOD_MS) {
            t_hb = now;
            hb_cnt++;
            UartDebug_Printf(
                "HB #%lu en=%d g=%u spd=%d m=%02X e=%.1f st=%d "
                "enc=%ld,%ld,%ld,%ld\n",
                (unsigned long)hb_cnt,
                LineFollow_IsEnabled() ? 1 : 0, (unsigned)s_gear_idx,
                (int)s_spd, (unsigned)LineFollow_GetMask(),
                (double)LineFollow_GetError(),
                (int)LineFollow_GetState(),
                (long)s_ea, (long)s_eb, (long)s_ec, (long)s_ed);
        }
    }
}