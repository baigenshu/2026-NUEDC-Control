/**
 * @file main.c
 * @brief car：四轮底盘 + 8 路灰度巡线 + OLED
 *
 * H 题第 2 项：
 *   A 点 B21 启动 → 顺时针巡线一圈 → 停回 A → OLED 显示总时间
 *
 * 节拍：SysTick 软 ms，约 5–10 ms 控制周期（禁止占用 TIMG0/PWMA）
 * OLED：约 100 ms 刷新
 */
#include "ti_msp_dl_config.h"
#include "chassis.h"
#include "lap_task.h"
#include "line_track.h"
#include "OLED.h"

#ifndef SYSTICK_HZ
#define SYSTICK_HZ      80000000u
#endif
#ifndef KEY_DEBOUNCE_MS
#define KEY_DEBOUNCE_MS 20u
#endif
#ifndef OLED_REFRESH_MS
#define OLED_REFRESH_MS 100u
#endif
#ifndef CTRL_MIN_MS
#define CTRL_MIN_MS     5u
#endif

static uint32_t s_ms;
static uint32_t s_last_systick;
static uint32_t s_cycle_accum;

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

static bool key_is_down(void)
{
    return DL_GPIO_readPins(GPIO_KEY_PORT, GPIO_KEY_B21_PIN) == 0u;
}

/** 消抖后的按下边沿（按下瞬间返回 true 一次） */
static bool key_poll_press(void)
{
    static uint8_t stable;
    static uint8_t raw_last;
    static uint32_t edge_ms;
    uint8_t raw = key_is_down() ? 1u : 0u;
    uint32_t now = millis();

    if (raw != raw_last) {
        raw_last = raw;
        edge_ms = now;
        return false;
    }
    if ((now - edge_ms) < KEY_DEBOUNCE_MS)
        return false;
    if (raw != stable) {
        stable = raw;
        if (stable)
            return true;
    }
    return false;
}

static const char *state_text(lap_state_t st)
{
    switch (st) {
    case LAP_STATE_RUN:     return "LAP:RUN ";
    case LAP_STATE_DONE:    return "LAP:DONE";
    case LAP_STATE_TIMEOUT: return "LAP:TMO ";
    case LAP_STATE_ABORTED: return "LAP:ABRT";
    default:                return "LAP:WAIT";
    }
}

static void ui_draw_static(void)
{
    OLED_Clear();
    OLED_ShowString(1, 1, "LAP:WAIT");
    OLED_ShowString(2, 1, "TIME:00.00s");
    OLED_ShowString(3, 1, "G:00000000");
    OLED_ShowString(4, 1, "D:000 E:+000");
}

static void ui_refresh(void)
{
    chassis_odom_t odom;
    lap_state_t st = LapTask_GetState();
    uint32_t elapsed = LapTask_GetElapsedMs();
    uint32_t seconds = elapsed / 1000u;
    uint32_t hundredths = (elapsed % 1000u) / 10u;
    uint8_t mask = LapTask_GetMask();
    int32_t err = LapTask_GetError() / 10;
    int32_t dist;
    char bits[9];
    uint8_t i;

    Chassis_GetOdom(&odom);
    dist = (int32_t)odom.dist_cm;
    if (dist < 0)
        dist = -dist;
    if (dist > 999)
        dist = 999;
    if (err > 999)
        err = 999;
    if (err < -999)
        err = -999;
    if (seconds > 99u)
        seconds = 99u;

    OLED_ShowString(1, 1, state_text(st));

    OLED_ShowString(2, 1, "TIME:");
    OLED_ShowNum(2, 6, seconds, 2);
    OLED_ShowChar(2, 8, '.');
    OLED_ShowNum(2, 9, hundredths, 2);
    OLED_ShowChar(2, 11, 's');

    for (i = 0; i < 8u; ++i)
        bits[i] = (mask & (1u << i)) ? '1' : '0';
    bits[8] = '\0';
    OLED_ShowString(3, 1, "G:");
    OLED_ShowString(3, 3, bits);

    OLED_ShowString(4, 1, "D:");
    OLED_ShowNum(4, 3, (uint32_t)dist, 3);
    OLED_ShowString(4, 7, " E:");
    OLED_ShowSignedNum(4, 10, err, 3);
}

int main(void)
{
    uint32_t t_prev;
    uint32_t t_oled;
    uint32_t now;
    uint32_t dt;

    SYSCFG_DL_init();
    timebase_init();

    OLED_Init();
    Chassis_Init();
    LineTrack_Init();
    LapTask_Init();
    Chassis_Enable(true);

    ui_draw_static();

    t_prev = millis();
    t_oled = t_prev;

    for (;;) {
        now = millis();
        dt = now - t_prev;
        if (dt < CTRL_MIN_MS)
            continue;
        if (dt > 50u)
            dt = 50u;
        t_prev = now;

        Chassis_Update(dt);

        if (key_poll_press()) {
            if (LapTask_IsActive())
                LapTask_Abort(now);
            else
                LapTask_Start(now);
        }

        LapTask_Update(now);

        if ((now - t_oled) >= OLED_REFRESH_MS) {
            t_oled = now;
            ui_refresh();
        }
    }
}
