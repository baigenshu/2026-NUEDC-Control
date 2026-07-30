/**
 * @file lap_task.c
 * @brief H 题第 2 项一圈任务
 *
 * 流程：
 *   WAIT → (B21) → RUN
 *   RUN：起步低速 → 巡线 → 过最小里程后认停车线 → DONE
 *   停车线判定：≥ LAP_MARKER_MIN_ACTIVE 路同时为黑，连续确认
 */
#include "lap_task.h"
#include "chassis_cfg.h"
#include "chassis.h"
#include "line_track.h"
#include "gray.h"

static lap_state_t s_state;
static uint32_t    s_start_ms;
static uint32_t    s_elapsed_ms;
static uint8_t     s_marker_hold;
static bool        s_left_start;
static bool        s_finish_armed;
static bool        s_cruising;

static uint8_t popcount8(uint8_t v)
{
    uint8_t n = 0;
    while (v) {
        n = (uint8_t)(n + (v & 1u));
        v = (uint8_t)(v >> 1);
    }
    return n;
}

static float absf(float v)
{
    return (v < 0.f) ? -v : v;
}

static bool stop_line_seen(uint8_t mask)
{
    return popcount8(mask) >= (uint8_t)LAP_MARKER_MIN_ACTIVE;
}

static void enter_wait(void)
{
    s_state = LAP_STATE_WAIT;
    s_elapsed_ms = 0;
    s_marker_hold = 0;
    s_left_start = false;
    s_finish_armed = false;
    s_cruising = false;
    LineTrack_SetEnable(false);
}

static void finish(lap_state_t result, uint32_t now_ms)
{
    LineTrack_SetEnable(false);
    Chassis_Stop(CHASSIS_STOP_BRAKE);
    s_elapsed_ms = now_ms - s_start_ms;
    s_state = result;
    s_marker_hold = 0;
}

void LapTask_Init(void)
{
    s_start_ms = 0;
    enter_wait();
}

void LapTask_Start(uint32_t now_ms)
{
    Chassis_Stop(CHASSIS_STOP_COAST);
    Chassis_ResetOdom();

    LineTrack_SetEnable(true);
    LineTrack_SetBaseSpeed((int16_t)LAP_START_SPEED);

    s_state = LAP_STATE_RUN;
    s_start_ms = now_ms;
    s_elapsed_ms = 0;
    s_marker_hold = 0;
    s_left_start = false;
    s_finish_armed = false;
    s_cruising = false;
}

void LapTask_Abort(uint32_t now_ms)
{
    if (s_state == LAP_STATE_RUN)
        finish(LAP_STATE_ABORTED, now_ms);
}

void LapTask_Update(uint32_t now_ms)
{
    float dist;
    uint8_t mask;
    bool marker;

    if (s_state != LAP_STATE_RUN)
        return;

    s_elapsed_ms = now_ms - s_start_ms;
    LineTrack_Update();

    /* 安全兜底：超时强制停，OLED 仍显示实际耗时 */
    if (s_elapsed_ms >= (uint32_t)LAP_TIME_LIMIT_MS) {
        finish(LAP_STATE_TIMEOUT, now_ms);
        return;
    }

    dist = absf(Chassis_GetDistCm());
    mask = LineTrack_GetMask();
    marker = stop_line_seen(mask);

    /* 起步后切换巡航速度 */
    if (!s_cruising && s_elapsed_ms >= (uint32_t)LAP_START_MS) {
        s_cruising = true;
        LineTrack_SetBaseSpeed((int16_t)LAP_TRACK_SPEED);
    }

    /* 终点前降速 */
    if (s_cruising &&
        dist >= (LAP_TRACK_LENGTH_CM - LAP_FINISH_SLOWDOWN_CM))
        LineTrack_SetBaseSpeed((int16_t)LAP_FINISH_SPEED);

    /*
     * 终点判定三段锁：
     * 1) 离开起点横线
     * 2) 里程过半，避免启停线/中途误触发
     * 3) 多路同时黑 = 停车基准线
     */
    if (!s_left_start) {
        if (!marker && dist >= LAP_LEAVE_START_CM)
            s_left_start = true;
        s_marker_hold = 0;
        return;
    }

    if (!s_finish_armed) {
        if (dist >= LAP_MIN_DISTANCE_CM)
            s_finish_armed = true;
        s_marker_hold = 0;
        return;
    }

    if (marker) {
        if (s_marker_hold < 255u)
            s_marker_hold++;
        if (s_marker_hold >= (uint8_t)LAP_MARKER_CONFIRM)
            finish(LAP_STATE_DONE, now_ms);
    } else {
        s_marker_hold = 0;
    }
}

lap_state_t LapTask_GetState(void)
{
    return s_state;
}

bool LapTask_IsActive(void)
{
    return s_state == LAP_STATE_RUN;
}

uint32_t LapTask_GetElapsedMs(void)
{
    return s_elapsed_ms;
}

uint8_t LapTask_GetMask(void)
{
    if (LineTrack_IsEnabled())
        return LineTrack_GetMask();
    return Gray_ReadMask();
}

int32_t LapTask_GetError(void)
{
    if (LineTrack_IsEnabled())
        return LineTrack_GetError();
    return Gray_GetPosition();
}
