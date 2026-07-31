/**
 * @file main.c
 * @brief 四路电机开环 + 编码器 + OLED + 双按键
 *
 * KEY_RUN PA17：电机开/关（低有效）
 * KEY_SPD PA18：档位 0→1000→2000→3000→0（低有效）
 * DEBUG UART0 PA10/PA11 115200：1 s 心跳
 *
 * OLED：
 *   L1  PWM:+xxxx
 *   L2  A:+xxx B:+xxx
 *   L3  C:+xxx D:+xxx
 *   L4  ON  G2 / OFF G0
 */
#include "ti_msp_dl_config.h"
#include "motor.h"
#include "motor_cfg.h"
#include "encoder.h"
#include "key.h"
#include "OLED.h"
#include "uart_debug.h"
#include <stdbool.h>

#ifndef SYSTICK_HZ
#define SYSTICK_HZ 80000000u
#endif

#ifndef SAMPLE_MS
#define SAMPLE_MS 20u
#endif

#ifndef OLED_REFRESH_MS
#define OLED_REFRESH_MS 100u
#endif

#ifndef HB_PERIOD_MS
#define HB_PERIOD_MS 1000u
#endif

static const int16_t s_gears[PWM_GEAR_COUNT] = {
    PWM_GEAR_0, PWM_GEAR_1, PWM_GEAR_2, PWM_GEAR_3,
};

static uint32_t s_ms;
static uint32_t s_last_systick;
static uint32_t s_cycle_accum;

static int32_t s_ea, s_eb, s_ec, s_ed;
static int16_t s_pwm;
static uint8_t s_gear_idx;
static bool s_motor_on;

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

static void apply_motor_output(void)
{
    if (s_motor_on) {
        Motor_SetEnable(true);
        Motor_SetAll((int16_t)(s_pwm * POL_A), (int16_t)(s_pwm * POL_B),
                     (int16_t)(s_pwm * POL_C), (int16_t)(s_pwm * POL_D));
    } else {
        Motor_StopAll(MOTOR_STOP_COAST);
        Motor_SetEnable(false);
    }
}

static void on_key_run(void)
{
    s_motor_on = !s_motor_on;
    apply_motor_output();
}

static void on_key_spd(void)
{
    s_gear_idx = (uint8_t)((s_gear_idx + 1u) % PWM_GEAR_COUNT);
    s_pwm = s_gears[s_gear_idx];
    if (s_motor_on)
        apply_motor_output();
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
    OLED_ShowString(1, 1, "PWM:");
    OLED_ShowString(2, 1, "A:");
    OLED_ShowString(2, 9, "B:");
    OLED_ShowString(3, 1, "C:");
    OLED_ShowString(3, 9, "D:");
}

static void ui_refresh(void)
{
    int32_t a = clamp_show(s_ea, 999);
    int32_t b = clamp_show(s_eb, 999);
    int32_t c = clamp_show(s_ec, 999);
    int32_t d = clamp_show(s_ed, 999);
    int32_t pwm = (int32_t)s_pwm;

    if (pwm > 9999)
        pwm = 9999;
    if (pwm < -9999)
        pwm = -9999;

    OLED_ShowSignedNum(1, 5, pwm, 4);

    OLED_ShowSignedNum(2, 3, a, 3);
    OLED_ShowSignedNum(2, 11, b, 3);
    OLED_ShowSignedNum(3, 3, c, 3);
    OLED_ShowSignedNum(3, 11, d, 3);

    if (s_motor_on)
        OLED_ShowString(4, 1, "ON  G");
    else
        OLED_ShowString(4, 1, "OFF G");
    OLED_ShowNum(4, 7, (uint32_t)s_gear_idx, 1);
}

int main(void)
{
    uint32_t t_prev;
    uint32_t t_oled;
    uint32_t t_hb;
    uint32_t now;
    uint32_t dt;
    uint32_t hb_cnt;

    s_gear_idx = 0;
    s_pwm = s_gears[s_gear_idx];
    s_motor_on = false;
    hb_cnt = 0;

    SYSCFG_DL_init();
    timebase_init();

    UartDebug_Init();
    OLED_Init();
    Motor_Init();
    Encoder_Init();
    Key_Init();
    apply_motor_output();

    UartDebug_Puts("line_track boot\n");
    UartDebug_Printf("UART0 %u 8N1 TX=PA10\n", (unsigned)DEBUG_UART_BAUD_RATE);

    ui_draw_static();
    ui_refresh();

    t_prev = millis();
    t_oled = t_prev;
    t_hb = t_prev;

    for (;;) {
        now = millis();
        dt = now - t_prev;
        if (dt < SAMPLE_MS)
            continue;
        t_prev = now;

        if (Key_PollPress(KEY_ID_RUN, now))
            on_key_run();
        if (Key_PollPress(KEY_ID_SPD, now))
            on_key_spd();

        if (s_motor_on)
            apply_motor_output();

        Encoder_ReadDeltaAll(&s_ea, &s_eb, &s_ec, &s_ed);

        if ((now - t_oled) >= OLED_REFRESH_MS) {
            t_oled = now;
            ui_refresh();
        }

        if ((now - t_hb) >= HB_PERIOD_MS) {
            t_hb = now;
            hb_cnt++;
            UartDebug_Printf(
                "HB #%lu t=%lums on=%d g=%u pwm=%d enc=%ld,%ld,%ld,%ld\n",
                (unsigned long)hb_cnt, (unsigned long)now,
                s_motor_on ? 1 : 0, (unsigned)s_gear_idx, (int)s_pwm,
                (long)s_ea, (long)s_eb, (long)s_ec, (long)s_ed);
        }
    }
}
