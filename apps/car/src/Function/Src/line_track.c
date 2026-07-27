/**
 * @file line_track.c
 * @brief 灰度巡线 PID → Chassis Arcade（owner=LINE）
 */
#include "line_track.h"
#include "chassis_cfg.h"
#include "chassis.h"
#include "gray.h"

/* chassis.c 内部导出：带 owner 的 Arcade */
void Chassis_ArcadeFromLine(int16_t throttle, int16_t turn);

static bool    s_enabled;
static int16_t s_base_speed;
static int32_t s_error;
static uint8_t s_mask;
static int32_t s_prev_err;
static float   s_integral;
static uint8_t s_lost_count;
static uint8_t s_searching;
static uint32_t s_search_ms;
static int16_t s_last_turn;

void LineTrack_Init(void)
{
    Gray_Init();
    s_enabled    = false;
    s_base_speed = (int16_t)LT_BASE_SPEED_DEFAULT;
    s_error      = 0;
    s_mask       = 0;
    s_prev_err   = 0;
    s_integral   = 0.f;
    s_lost_count = 0;
    s_searching  = 0;
    s_search_ms  = 0;
    s_last_turn  = 0;
}

void LineTrack_SetEnable(bool on)
{
    s_enabled = on;
    if (!on) {
        s_lost_count = 0;
        s_searching  = 0;
        s_integral   = 0.f;
    }
}

bool LineTrack_IsEnabled(void)
{
    return s_enabled;
}

void LineTrack_SetBaseSpeed(int16_t pct)
{
    if (pct > 100)  pct = 100;
    if (pct < -100) pct = -100;
    s_base_speed = pct;
}

int32_t LineTrack_GetError(void) { return s_error; }
uint8_t LineTrack_GetMask(void)  { return s_mask; }

static int16_t clamp_turn(int16_t t)
{
    if (t >  (int16_t)LT_TURN_LIMIT) return (int16_t)LT_TURN_LIMIT;
    if (t < -(int16_t)LT_TURN_LIMIT) return (int16_t)(-LT_TURN_LIMIT);
    return t;
}

void LineTrack_Update(void)
{
    int16_t turn;
    float deriv;

    if (!s_enabled)
        return;
    /* 与 MOTION 互斥：Busy 时 main 不应调用；双保险 */
    if (Chassis_Busy())
        return;

    s_mask  = Gray_ReadMask();
    s_error = Gray_GetPosition();

    if (s_mask == 0u) {
        s_lost_count++;
        if (s_lost_count < (uint8_t)LT_LOST_DEBOUNCE) {
            /* 去抖期内保持上次转向 */
            Chassis_ArcadeFromLine(s_base_speed, s_last_turn);
            return;
        }

        /* 触发丢线策略 */
        switch (LT_LOST_LINE_POLICY) {
        case 1: /* HOLD last turn */
            Chassis_ArcadeFromLine(s_base_speed, s_last_turn);
            break;
        case 2: /* SEARCH */
            if (!s_searching) {
                s_searching = 1;
                s_search_ms = 0;
            }
            s_search_ms += 10u; /* 假定 10ms 节拍；更精确可由外部传 dt */
            if (s_search_ms >= (uint32_t)LT_SEARCH_TIMEOUT_MS) {
                Chassis_Stop(CHASSIS_STOP_DEFAULT);
                s_searching = 0;
                s_enabled   = false;
            } else {
                turn = (s_last_turn >= 0) ? (int16_t)LT_SEARCH_TURN
                                          : (int16_t)(-LT_SEARCH_TURN);
                Chassis_ArcadeFromLine(0, turn);
            }
            break;
        default: /* 0 = STOP */
            Chassis_Stop(CHASSIS_STOP_DEFAULT);
            s_enabled = false;
            break;
        }
        return;
    }

    /* 有线：退出搜索 / 清丢线计数 */
    s_lost_count = 0;
    s_searching  = 0;
    s_search_ms  = 0;

    /* PID */
    s_integral += (float)s_error * LT_KI;
    deriv = (float)(s_error - s_prev_err);
    s_prev_err = s_error;

    turn = (int16_t)(LT_KP * (float)s_error + s_integral + LT_KD * deriv);
    turn = clamp_turn(turn);
    s_last_turn = turn;

    Chassis_ArcadeFromLine(s_base_speed, turn);
}
