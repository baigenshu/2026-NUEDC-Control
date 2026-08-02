/**
 * @file line_follow.c
 * @brief 四路加权巡线 · 位置 PID · 编码器闭环（左 A+B · 右 C+D）
 *
 * 每 10ms 控制周期：
 *   传感器读数 → 加权线位置 → 位置 PID → 有符号差速（左右速度目标）
 *   → SpeedCtrl_SetTargetLR → SpeedCtrl_Update（四路速度 PID → PWM）
 * 算法移植自参考工程（直道+圆弧弯道；内环增量式PI；允许内轮反转）。
 */
#include "line_follow.h"
#include "line_follow_cfg.h"
#include "ir4.h"
#include "speed_ctrl.h"
#include "motor.h"
#include "motor_cfg.h"
#include "encoder.h"
#include <stdbool.h>

static bool     s_enabled;
static int16_t  s_base_spd;           /* encoder pulses / 10ms */

static uint8_t  s_sensor[IR4_CH_COUNT];
static uint8_t  s_mask;
static uint8_t  s_active;

static float    s_error;
static float    s_last_error;
static float    s_integral;

/* 编码器增量（供外部遥测） */
static int32_t  s_enc[4];

/* 传感器权重表（左负右正） */
static const float s_w[IR4_CH_COUNT] = { LF_W0, LF_W1, LF_W2, LF_W3 };

/* ================================================================
 * 内部辅助
 * ================================================================ */

static void stop_all(void)
{
    SpeedCtrl_SetEnable(false);
    Motor_StopAll(MOTOR_STOP_COAST);
    Motor_SetEnable(false);
}

/* ================================================================
 * Public API
 * ================================================================ */

void LineFollow_Init(void)
{
    Ir4_Init();
    SpeedCtrl_Init();
    s_enabled  = false;
    s_base_spd = 0;
    LineFollow_Reset();
}

void LineFollow_Reset(void)
{
    uint8_t i;

    s_error      = 0.f;
    s_last_error = 0.f;
    s_integral   = 0.f;
    s_mask       = 0;
    s_active     = 0;
    for (i = 0; i < IR4_CH_COUNT; ++i)
        s_sensor[i] = 0;
    for (i = 0; i < 4; ++i)
        s_enc[i] = 0;
    SpeedCtrl_Reset();
}

void LineFollow_SetEnable(bool on)
{
    s_enabled = on;
    LineFollow_Reset();
    if (on) {
        SpeedCtrl_SetEnable(true);   /* 启用速度 PID（原缺此调用）*/
    } else {
        stop_all();
    }
}

bool LineFollow_IsEnabled(void)
{
    return s_enabled;
}

void LineFollow_SetBaseSpd(int16_t spd)
{
    if (spd < 0)
        spd = 0;
    s_base_spd = spd;
}

int16_t LineFollow_GetBaseSpd(void)
{
    return s_base_spd;
}

void LineFollow_Update(void)
{
    uint8_t  i;
    float    sum;
    float    deriv;
    float    pid;
    float    pid_lim;
    float    maxv;
    float    left_f, right_f;

    if (!s_enabled) {
        stop_all();
        return;
    }

    /* ---- 1. 读传感器 ---- */
    Ir4_ReadRaw(s_sensor);
    s_mask   = 0;
    s_active = 0;
    for (i = 0; i < IR4_CH_COUNT; ++i) {
        if (s_sensor[i]) {
            s_mask |= (uint8_t)(1u << i);
            s_active++;
        }
    }

    /* ---- 2. 加权线位置（丢线则向最后所见方向瞬间大幅补偿，见 LF_LOST_ERR）---- */
    if (s_active == 0u) {
        if (s_last_error < 0.f) {            /* 线在左 p1-p2 间隙 → 大幅左修 */
            s_error = -LF_LOST_ERR;
        } else if (s_last_error > 0.f) {     /* 线在右 p3-p4 间隙 → 大幅右修 */
            s_error = +LF_LOST_ERR;
        } else {
            s_error = 0.f;                   /* 方向未定（启动/居中丢线）*/
        }
    } else {
        sum = 0.f;
        for (i = 0; i < IR4_CH_COUNT; ++i) {
            if (s_sensor[i])
                sum += s_w[i];
        }
        s_error = sum / (float)s_active;
    }

    /* ---- 3. 位置 PID（位置式，积分限幅）---- */
    s_integral += s_error;
    if (s_integral > LF_I_MAX)  s_integral = LF_I_MAX;
    if (s_integral < -LF_I_MAX) s_integral = -LF_I_MAX;

    deriv = s_error - s_last_error;
    pid   = LF_KP * s_error + LF_KI * s_integral + LF_KD * deriv;

    pid_lim = (float)s_base_spd * LF_PID_OUT_FRAC;
    if (pid > pid_lim)   pid = pid_lim;
    if (pid < -pid_lim)  pid = -pid_lim;

    /* ---- 4. 有符号差速（允许内轮反转）----
     * error>0 线偏右 → pid>0 → 左轮加速、右轮减速 → 右转 */
    left_f  = (float)s_base_spd + pid;
    right_f = (float)s_base_spd - pid;
    maxv    = (float)s_base_spd * LF_MAX_SPD_FRAC;
    if (left_f  >  maxv) left_f  =  maxv;
    if (left_f  < -maxv) left_f  = -maxv;
    if (right_f >  maxv) right_f =  maxv;
    if (right_f < -maxv) right_f = -maxv;

    /* ---- 5. 送内环速度 PID ---- */
    SpeedCtrl_SetTargetLR((int16_t)left_f, (int16_t)right_f);
    SpeedCtrl_Update();

    /* ---- 6. 遥测 ---- */
    for (i = 0; i < 4; ++i)
        s_enc[i] = SpeedCtrl_GetEncoderDelta((speed_id_t)i);

    s_last_error = s_error;
}

/* ---- 遥测 ---- */

float LineFollow_GetError(void)
{
    return s_error;
}

uint8_t LineFollow_GetMask(void)
{
    return s_mask;
}

lf_state_t LineFollow_GetState(void)
{
    return s_enabled ? LF_STATE_TRACK : LF_STATE_IDLE;
}

void LineFollow_GetEncoderDeltas(int32_t *a, int32_t *b,
                                  int32_t *c, int32_t *d)
{
    if (a) *a = s_enc[0];
    if (b) *b = s_enc[1];
    if (c) *c = s_enc[2];
    if (d) *d = s_enc[3];
}