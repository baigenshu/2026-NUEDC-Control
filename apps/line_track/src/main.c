/**
 * @file main.c
 * @brief 四路红外巡线 · 四轮独立速度 PID 闭环
 *
 * KEY_RUN PA17：巡线开/关
 * KEY_SPD PA15：模式 STOP→M1→M2→M3→STOP
 *               M1=三档持续 / M2=三档15s停 / M3=二档7s停 / STOP=不动
 * IR p1..p4 = PB19/PB17/PA16/PA14（G1–G4）
 *
 * OLED：
 *   L1  SPD:+xxxx
 *   L2  IR:xxxx E:±xx
 *   L3  A:+xxx B:+xxx
 *   L4  TRK / OFF Mx  T:xxx.xx   (RUN 键计时)
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

/* 运行模式：STOP + 三种循迹模式（KEY_SPD 循环）。timeout_ms=0 不自动停 */
typedef struct {
    int16_t  spd;         /* 基速 pulses/10ms */
    uint32_t timeout_ms;  /* 0 = 不自动停 */
} run_mode_t;

#define MODE_COUNT  (4)
#define MODE_STOP   (0)

static const run_mode_t s_modes[MODE_COUNT] = {
    { SPD_GEAR_0, 0u      },  /* M0 STOP: 不动        */
    { SPD_GEAR_3, 0u      },  /* M1: 三档, 持续        */
    { SPD_GEAR_3, 15000u  },  /* M2: 三档, 15s 自动停  */
    { SPD_GEAR_2, 7000u   },  /* M3: 二档, 7s  自动停  */
};

static uint32_t s_ms;
static uint32_t s_last_systick;
static uint32_t s_cycle_accum;

static int32_t s_ea, s_eb, s_ec, s_ed;
static int16_t s_spd;
static uint8_t s_mode_idx;
static uint8_t s_ui_dirty;

/* RUN 键计时器：按下启动计时 / 再按停止并显示 / 再启动清零重计 */
static uint8_t  s_timer_running;     /* 0=停止(显示冻结用时), 1=计时中 */
static uint32_t s_timer_start_ms;    /* 启动时刻 (ms) */
static uint32_t s_timer_elapsed_ms;  /* 停止时冻结的用时 (ms) */
static uint32_t s_timer_display_ms;  /* 当前显示用 (ms) */

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

static void start_run(uint32_t now)
{
    LineFollow_SetBaseSpd(s_spd);
    LineFollow_SetEnable(true);
    /* 启动：清零并开始计时 */
    s_timer_running    = 1u;
    s_timer_start_ms   = now;
    s_timer_elapsed_ms = 0u;
}

static void stop_run(uint32_t now)
{
    /* 停止：冻结用时并打印 */
    LineFollow_SetEnable(false);
    s_timer_running    = 0u;
    s_timer_elapsed_ms = now - s_timer_start_ms;
    UartDebug_Printf("LAP %lu.%02lu s\n",
                     (unsigned long)(s_timer_elapsed_ms / 1000u),
                     (unsigned long)((s_timer_elapsed_ms % 1000u) / 10u));
}

static void on_key_run(uint32_t now)
{
    if (!LineFollow_IsEnabled()) {
        start_run(now);
        UartDebug_Printf("KEY_RUN -> ON mode=%u spd=%d\n",
                         (unsigned)s_mode_idx, (int)s_spd);
    } else {
        stop_run(now);
        UartDebug_Printf("KEY_RUN -> OFF (manual)\n");
    }
    s_ui_dirty = 1u;
}

static void on_key_spd(void)
{
    s_mode_idx = (uint8_t)((s_mode_idx + 1u) % MODE_COUNT);
    s_spd = s_modes[s_mode_idx].spd;
    LineFollow_SetBaseSpd(s_spd);
    UartDebug_Printf("KEY_SPD -> mode=%u spd=%d timeout=%lu ms\n",
                     (unsigned)s_mode_idx, (int)s_spd,
                     (unsigned long)s_modes[s_mode_idx].timeout_ms);
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
    return (st == LF_STATE_TRACK) ? "TRK M" : "OFF M";
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
    OLED_ShowNum(4, 7, (uint32_t)s_mode_idx, 1);

    /* L4 末尾：计时 T:xxx.xx (s) */
    {
        uint32_t sec = s_timer_display_ms / 1000u;
        uint32_t cs  = (s_timer_display_ms % 1000u) / 10u;
        if (sec > 999u) sec = 999u;          /* 限幅 999.99 s */
        OLED_ShowChar(4, 9,  'T');
        OLED_ShowChar(4, 10, ':');
        OLED_ShowNum (4, 11, sec, 3);
        OLED_ShowChar(4, 14, '.');
        OLED_ShowNum (4, 15, cs, 2);
    }
}

int main(void)
{
    uint32_t t_prev;
    uint32_t t_oled;
    uint32_t t_hb;
    uint32_t now;
    uint32_t hb_cnt;

    s_mode_idx = MODE_STOP;
    s_spd = s_modes[s_mode_idx].spd;
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
            on_key_run(now);
        if (Key_PollPress(KEY_ID_SPD, now))
            on_key_spd();

        /* 模式超时自动停（M2=15s, M3=7s；M0/M1 timeout=0 不触发）*/
        if (s_timer_running &&
            s_modes[s_mode_idx].timeout_ms != 0u &&
            (now - s_timer_start_ms) >= s_modes[s_mode_idx].timeout_ms) {
            stop_run(now);
            UartDebug_Printf("AUTO-STOP mode=%u\n", (unsigned)s_mode_idx);
            s_ui_dirty = 1u;
        }

        if ((now - t_prev) >= SAMPLE_MS) {
            t_prev = now;
            LineFollow_Update();
            LineFollow_GetEncoderDeltas(&s_ea, &s_eb, &s_ec, &s_ed);
        }

        if (s_ui_dirty || (now - t_oled) >= OLED_REFRESH_MS) {
            t_oled = now;
            s_ui_dirty = 0;
            s_timer_display_ms = s_timer_running
                ? (now - s_timer_start_ms) : s_timer_elapsed_ms;
            ui_refresh();
        }

        if ((now - t_hb) >= HB_PERIOD_MS) {
            t_hb = now;
            hb_cnt++;
            UartDebug_Printf(
                "HB #%lu en=%d mode=%u spd=%d m=%02X e=%ld st=%d "
                "enc=%ld,%ld,%ld,%ld\n",
                (unsigned long)hb_cnt,
                LineFollow_IsEnabled() ? 1 : 0, (unsigned)s_mode_idx,
                (int)s_spd, (unsigned)LineFollow_GetMask(),
                (long)(LineFollow_GetError() * 10.0f),  /* e×10，单位0.1 */
                (int)LineFollow_GetState(),
                (long)s_ea, (long)s_eb, (long)s_ec, (long)s_ed);
        }
    }
}