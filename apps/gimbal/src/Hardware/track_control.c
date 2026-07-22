#include "track_control.h"
#include "track_proto.h"
#include "step_motor.h"
#include "delay.h"

/*
 * 跟踪控制 — P 控制 + 死区 + 超时停机
 *
 * 控制律：
 *   e_yaw   = err_x          (像素)
 *   e_pitch = err_y
 *
 *   if |e| < deadzone: v = 0
 *   else:              v = Kp * e
 *
 *   v = clamp(v, -Vmax, +Vmax)
 */

static float clamp_f(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float p_control(int16_t err_px, float kp, float vmax)
{
    float v;

    if (err_px > -TRACK_DEADZONE_PX && err_px < TRACK_DEADZONE_PX)
        return 0.0f;

    v = kp * (float)err_px;
    return clamp_f(v, -vmax, vmax);
}

void track_control_init(void)
{
    /* nothing special — motor & proto already inited */
}

void track_control_update(void)
{
    track_cmd_t m;
    uint32_t    now;
    float       vy, vp;

    now = millis();

    if (track_proto_take(&m)) {
        /* 有新帧 */
    } else {
        /* 用 g_track_cmd 查超时 */
        now = millis();  /* re-read in case of long gap */
        if (now - g_track_cmd.last_ms > TRACK_TIMEOUT_MS) {
            step_set_velocity_f(0.0f, 1);
            step_set_velocity_f(0.0f, 2);
            return;
        }
        /* 未超时：保持上次速度，直接返回 */
        return;
    }

    if (!m.found) {
        /* 丢目标 → 立即停机 */
        step_set_velocity_f(0.0f, 1);
        step_set_velocity_f(0.0f, 2);
        return;
    }

    /* P 控制：err_x → Yaw；err_y 下正，俯仰用 -err_y 做负反馈 */
    vy = p_control(m.err_x, TRACK_KP_X, TRACK_VMAX_YAW) * (float)YAW_DIR_SIGN;
    vp = p_control(-m.err_y, TRACK_KP_Y, TRACK_VMAX_PITCH) * (float)PITCH_DIR_SIGN;

    /* 软限位（Phase 5 可选） */
#if PITCH_LIMIT_DEG_X10 > 0
    if (m.pitch > (PITCH_LIMIT_DEG_X10 / 10.0f) && vp > 0.0f) vp = 0.0f;
    if (m.pitch < -(PITCH_LIMIT_DEG_X10 / 10.0f) && vp < 0.0f) vp = 0.0f;
#endif

    step_set_velocity_f(vy, 1);
    step_set_velocity_f(vp, 2);
}
