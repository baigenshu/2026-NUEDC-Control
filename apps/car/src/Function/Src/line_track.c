/**
 * @file line_track.c
 * @brief 8 路灰度 PD 巡线
 *
 * 规则：
 * 1. 加权位置 → 低通 → 死区 → PD 转向
 * 2. |turn| ≤ 0.7 * base，慢侧不反转
 * 3. 丢线保持上次转向继续前进
 */
#include "line_track.h"
#include "chassis_cfg.h"
#include "chassis.h"
#include "gray.h"

static bool    s_enabled;
static int16_t s_base_speed;
static int32_t s_error;
static uint8_t s_mask;
static int32_t s_prev_err;
static float   s_filt_err;
static uint8_t s_lost_count;
static int16_t s_last_turn;

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static int16_t abs_i16(int16_t v)
{
    return (v < 0) ? (int16_t)(-v) : v;
}

/** 转向上限：不超过 LT_TURN_LIMIT，也不超过 0.7*base（防反转） */
static int16_t turn_limit(void)
{
    int16_t lim = (int16_t)LT_TURN_LIMIT;
    int16_t by_speed = (int16_t)((abs_i16(s_base_speed) * 7) / 10);

    if (by_speed < 4)
        by_speed = 4;
    if (lim > by_speed)
        lim = by_speed;
    return lim;
}

static int16_t clamp_turn(int16_t t)
{
    int16_t lim = turn_limit();
    return clamp_i16(t, (int16_t)(-lim), lim);
}

static int16_t slew_turn(int16_t target)
{
    int16_t d = (int16_t)(target - s_last_turn);
    int16_t step = (int16_t)LT_TURN_SLEW;

    if (step < 1)
        step = 1;
    if (d > step)
        d = step;
    else if (d < -step)
        d = (int16_t)(-step);
    return (int16_t)(s_last_turn + d);
}

void LineTrack_Init(void)
{
    Gray_Init();
    s_enabled = false;
    s_base_speed = (int16_t)LT_BASE_SPEED_DEFAULT;
    LineTrack_Reset();
}

void LineTrack_Reset(void)
{
    s_error = 0;
    s_mask = 0;
    s_prev_err = 0;
    s_filt_err = 0.f;
    s_lost_count = 0;
    s_last_turn = 0;
}

void LineTrack_SetEnable(bool on)
{
    s_enabled = on;
    LineTrack_Reset();
}

bool LineTrack_IsEnabled(void)
{
    return s_enabled;
}

void LineTrack_SetBaseSpeed(int16_t pct)
{
    if (pct > 100)
        pct = 100;
    if (pct < 0)
        pct = 0;
    s_base_speed = pct;
}

int32_t LineTrack_GetError(void)
{
    return s_error;
}

uint8_t LineTrack_GetMask(void)
{
    return s_mask;
}

void LineTrack_Update(void)
{
    float raw;
    float alpha;
    float deriv;
    int32_t err;
    int16_t turn;
    int16_t throttle;

    if (!s_enabled)
        return;

    s_mask = Gray_ReadMask();
    raw = (float)Gray_GetPosition();

    alpha = LT_ERROR_FILTER;
    if (alpha < 0.05f)
        alpha = 0.05f;
    if (alpha > 1.0f)
        alpha = 1.0f;
    s_filt_err += alpha * (raw - s_filt_err);
    err = (int32_t)(s_filt_err + (s_filt_err >= 0.f ? 0.5f : -0.5f));

    if (err > -(int32_t)LT_ERROR_DEADZONE && err < (int32_t)LT_ERROR_DEADZONE)
        err = 0;
    s_error = err;

    /* 丢线：保持上次转向继续前进 */
    if (s_mask == 0u) {
        if (s_lost_count < 255u)
            s_lost_count++;
        Chassis_Arcade(s_base_speed, s_last_turn);
        return;
    }
    s_lost_count = 0;

    deriv = (float)(s_error - s_prev_err);
    s_prev_err = s_error;

    turn = (int16_t)((float)LT_TURN_SIGN *
                     (LT_KP * (float)s_error + LT_KD * deriv));
    turn = clamp_turn(turn);
    turn = slew_turn(turn);
    s_last_turn = turn;

    /* 大偏差稍降速，仍保持前进 */
    throttle = s_base_speed;
    if (err > 1600 || err < -1600) {
        throttle = (int16_t)((s_base_speed * 8) / 10);
        if (throttle < 10)
            throttle = 10;
    }

    Chassis_Arcade(throttle, turn);
}
