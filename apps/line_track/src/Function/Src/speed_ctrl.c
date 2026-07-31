/**
 * @file speed_ctrl.c
 * @brief 四轮独立速度 PID（编码器反馈 → PWM）
 *
 * 每路独立 PID 回路，目标/反馈单位统一为 encoder pulses / 控制周期。
 * 输出直接送 Motor_Set，内部处理 POL_A/B/C/D 极性修正。
 */
#include "speed_ctrl.h"
#include "line_follow_cfg.h"
#include "encoder.h"
#include "motor.h"
#include "motor_cfg.h"

/* ---- 状态 ---- */
static bool     s_enabled;
static int16_t  s_target[SPEED_ID_COUNT];   /* pulses/sample */
static float    s_integral[SPEED_ID_COUNT];
static float    s_last_err[SPEED_ID_COUNT];
static int32_t  s_last_enc[SPEED_ID_COUNT]; /* raw accumulated encoder */
static int32_t  s_delta[SPEED_ID_COUNT];    /* last delta for telemetry */

/* ---- 每路极性（前进 = 正 duty 经过 POL 后送电机） ---- */
static const int8_t s_pol[SPEED_ID_COUNT] = {
    POL_A, POL_B, POL_C, POL_D
};

/* ================================================================
 * Helpers
 * ================================================================ */

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int16_t limit_duty(float v)
{
    if (v > SPD_OUT_MAX)  return (int16_t)SPD_OUT_MAX;
    if (v < -SPD_OUT_MAX) return (int16_t)(-SPD_OUT_MAX);
    return (int16_t)v;
}

/* ================================================================
 * Public API
 * ================================================================ */

void SpeedCtrl_Init(void)
{
    s_enabled = false;
    SpeedCtrl_Reset();
}

void SpeedCtrl_Reset(void)
{
    uint8_t i;

    for (i = 0; i < SPEED_ID_COUNT; ++i) {
        s_target[i]   = 0;
        s_integral[i] = 0.f;
        s_last_err[i] = 0.f;
        s_last_enc[i] = Encoder_Get((enc_id_t)i);
        s_delta[i]    = 0;
    }
}

void SpeedCtrl_SetEnable(bool on)
{
    s_enabled = on;
    if (!on) {
        uint8_t i;
        for (i = 0; i < SPEED_ID_COUNT; ++i) {
            s_integral[i] = 0.f;
            s_last_err[i] = 0.f;
        }
    }
}

void SpeedCtrl_SetTarget(speed_id_t id, int16_t spd)
{
    if ((unsigned)id >= SPEED_ID_COUNT) return;
    /* 允许负目标以支持内轮反转（循迹紧弯）*/
    s_target[id] = spd;
}

void SpeedCtrl_SetTargetLR(int16_t left_spd, int16_t right_spd)
{
    SpeedCtrl_SetTarget(SPEED_ID_A, left_spd);
    SpeedCtrl_SetTarget(SPEED_ID_B, left_spd);
    SpeedCtrl_SetTarget(SPEED_ID_C, right_spd);
    SpeedCtrl_SetTarget(SPEED_ID_D, right_spd);
}

void SpeedCtrl_Update(void)
{
    uint8_t i;

    if (!s_enabled) {
        Motor_StopAll(MOTOR_STOP_COAST);
        Motor_SetEnable(false);
        return;
    }

    Motor_SetEnable(true);

    for (i = 0; i < SPEED_ID_COUNT; ++i) {
        float    err, p, d, out;
        int32_t  raw_now;
        int16_t  duty_signed;

        /* 编码器增量（pulses/sample，前进为正） */
        s_delta[i] = Encoder_ReadDelta((enc_id_t)i);

        /* 速度误差 */
        err = (float)s_target[i] - (float)s_delta[i];

        /* PID */
        p  = SPD_KP * err;

        s_integral[i] += SPD_KI * err;
        s_integral[i]  = clampf(s_integral[i], -SPD_I_MAX, SPD_I_MAX);

        d  = SPD_KD * (err - s_last_err[i]);
        s_last_err[i] = err;

        out = p + s_integral[i] + d;
        duty_signed = limit_duty(out);

        /* 死区处理 */
        if (duty_signed > 0 && duty_signed < PWM_DEADZONE)
            duty_signed = (int16_t)PWM_DEADZONE;
        else if (duty_signed < 0 && duty_signed > -(int16_t)PWM_DEADZONE)
            duty_signed = (int16_t)(-(int16_t)PWM_DEADZONE);

        /* 输出：叠加极性修正 */
        Motor_Set((motor_id_t)i,
                  (int16_t)((int32_t)duty_signed * (int32_t)s_pol[i]));
    }
}

int32_t SpeedCtrl_GetEncoderDelta(speed_id_t id)
{
    if ((unsigned)id >= SPEED_ID_COUNT) return 0;
    return s_delta[id];
}
