/**
 * @file speed_ctrl.c
 * @brief 四轮独立增量式 PI（编码器反馈 → PWM）
 *
 * 每路独立增量式 PI 回路，目标/反馈单位统一为 encoder pulses / 控制周期。
 * 输出直接送 Motor_Set，内部处理 POL_A/B/C/D 极性修正。
 */
#include "speed_ctrl.h"
#include "line_follow_cfg.h"
#include "encoder.h"
#include "motor.h"
#include "motor_cfg.h"

/* ---- 状态 ---- */
static bool     s_enabled;
static int16_t  s_target[SPEED_ID_COUNT];     /* pulses/sample */
static float    s_out[SPEED_ID_COUNT];        /* 增量式PI累计输出（PWM duty） */
static float    s_last_bias[SPEED_ID_COUNT];  /* 上次速度偏差 */
static int32_t  s_delta[SPEED_ID_COUNT];      /* last delta for telemetry */

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
        s_target[i]    = 0;
        s_out[i]       = 0.f;
        s_last_bias[i] = 0.f;
        s_delta[i]     = 0;
    }
}

void SpeedCtrl_SetEnable(bool on)
{
    s_enabled = on;
    if (!on) {
        uint8_t i;
        for (i = 0; i < SPEED_ID_COUNT; ++i) {
            s_out[i]       = 0.f;   /* 清零累计输出，避免再启动跳变 */
            s_last_bias[i] = 0.f;
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
        float    bias;
        int16_t  duty_signed;

        /* 编码器增量（pulses/sample，前进为正） */
        s_delta[i] = Encoder_ReadDelta((enc_id_t)i);

        /* 速度偏差 */
        bias = (float)s_target[i] - (float)s_delta[i];

        /* 增量式 PI（移植自参考工程 Velocity_A/B）
         *   u(k) = u(k-1) + SPD_KP*bias + SPD_KI*(bias - last_bias)
         *   SPD_KP*bias             偏差积分项（累计成 PWM）
         *   SPD_KI*(bias-last_bias) 偏差变化项（限制加速度/抑制震荡） */
        s_out[i] += SPD_KP * bias + SPD_KI * (bias - s_last_bias[i]);
        s_last_bias[i] = bias;

        /* 限幅累计输出本身，防积分饱和（同参考工程对 ControlVelocity 限幅） */
        s_out[i] = clampf(s_out[i], -SPD_OUT_MAX, SPD_OUT_MAX);
        duty_signed = (int16_t)s_out[i];

        /* 死区处理（克服启动静摩擦） */
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