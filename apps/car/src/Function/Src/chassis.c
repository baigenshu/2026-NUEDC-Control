/**
 * @file chassis.c
 * @brief 四轮差速：开环 Arcade + 编码器里程
 */
#include "chassis.h"
#include "chassis_cfg.h"
#include "motor.h"
#include "encoder.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static bool    s_enabled;
static uint8_t s_trim_l;
static uint8_t s_trim_r;
static int16_t s_cmd_l;
static int16_t s_cmd_r;

static bool    s_odom_inited;
static int32_t s_prev_a, s_prev_b, s_prev_c, s_prev_d;
static int32_t s_acc_a, s_acc_b, s_acc_c, s_acc_d;
static int32_t s_acc_l, s_acc_r;
static float   s_dist_cm;
static float   s_heading_deg;

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static int16_t pct_to_duty(int16_t pct)
{
    if (pct > 100)
        pct = 100;
    if (pct < -100)
        pct = -100;
    return (int16_t)(((int32_t)pct * (int32_t)PWM_MAX) / 100);
}

static void apply_lr(int16_t left_pct, int16_t right_pct)
{
    int16_t lim = (int16_t)CHASSIS_SPEED_MAX;
    int16_t l = clamp_i16(left_pct, (int16_t)(-lim), lim);
    int16_t r = clamp_i16(right_pct, (int16_t)(-lim), lim);
    int16_t duty_l = pct_to_duty(l);
    int16_t duty_r = pct_to_duty(r);

    duty_l = (int16_t)(((int32_t)duty_l * (int32_t)s_trim_l) / 100);
    duty_r = (int16_t)(((int32_t)duty_r * (int32_t)s_trim_r) / 100);

    Motor_Set(MOTOR_ID_A, (int16_t)(duty_l * (int32_t)POL_A));
    Motor_Set(MOTOR_ID_B, (int16_t)(duty_l * (int32_t)POL_B));
    Motor_Set(MOTOR_ID_C, (int16_t)(duty_r * (int32_t)POL_C));
    Motor_Set(MOTOR_ID_D, (int16_t)(duty_r * (int32_t)POL_D));
}

void Chassis_Init(void)
{
    Motor_Init();
    Encoder_Init();

    s_enabled = false;
    s_trim_l = (uint8_t)LEFT_TRIM;
    s_trim_r = (uint8_t)RIGHT_TRIM;
    s_cmd_l = 0;
    s_cmd_r = 0;
    Chassis_ResetOdom();
    Motor_StopAll(MOTOR_STOP_COAST);
}

void Chassis_Enable(bool on)
{
    s_enabled = on;
    Motor_SetEnable(on);
    if (!on) {
        s_cmd_l = 0;
        s_cmd_r = 0;
        Motor_StopAll(MOTOR_STOP_COAST);
    }
}

void Chassis_Stop(chassis_stop_mode_t mode)
{
    s_cmd_l = 0;
    s_cmd_r = 0;
    Motor_StopAll(mode == CHASSIS_STOP_BRAKE ? MOTOR_STOP_BRAKE : MOTOR_STOP_COAST);
}

void Chassis_SetLR(int16_t left_pct, int16_t right_pct)
{
    s_cmd_l = left_pct;
    s_cmd_r = right_pct;
    if (s_enabled)
        apply_lr(left_pct, right_pct);
}

void Chassis_Arcade(int16_t throttle, int16_t turn)
{
    int16_t lim = (int16_t)CHASSIS_SPEED_MAX;
    int16_t l = clamp_i16((int16_t)(throttle + turn), (int16_t)(-lim), lim);
    int16_t r = clamp_i16((int16_t)(throttle - turn), (int16_t)(-lim), lim);
    Chassis_SetLR(l, r);
}

void Chassis_ResetOdom(void)
{
    Encoder_ResetAll();
    s_odom_inited = false;
    s_prev_a = s_prev_b = s_prev_c = s_prev_d = 0;
    s_acc_a = s_acc_b = s_acc_c = s_acc_d = 0;
    s_acc_l = s_acc_r = 0;
    s_dist_cm = 0.f;
    s_heading_deg = 0.f;
}

void Chassis_GetOdom(chassis_odom_t *o)
{
    if (!o)
        return;
    o->a = s_acc_a;
    o->b = s_acc_b;
    o->c = s_acc_c;
    o->d = s_acc_d;
    o->left = s_acc_l;
    o->right = s_acc_r;
    o->dist_cm = s_dist_cm;
    o->heading_deg = s_heading_deg;
}

float Chassis_GetDistCm(void)
{
    return s_dist_cm;
}

void Chassis_Update(uint32_t dt_ms)
{
    int32_t ca, cb, cc, cd;
    int32_t dA, dB, dC, dD;
    float pulse_L, pulse_R, dL_mm, dR_mm, ds_mm, dth_rad;

    (void)dt_ms;

    Encoder_GetAll(&ca, &cb, &cc, &cd);

    if (!s_odom_inited) {
        s_prev_a = ca;
        s_prev_b = cb;
        s_prev_c = cc;
        s_prev_d = cd;
        s_odom_inited = true;
    } else {
        dA = ca - s_prev_a;
        dB = cb - s_prev_b;
        dC = cc - s_prev_c;
        dD = cd - s_prev_d;
        s_prev_a = ca;
        s_prev_b = cb;
        s_prev_c = cc;
        s_prev_d = cd;

        s_acc_a += dA;
        s_acc_b += dB;
        s_acc_c += dC;
        s_acc_d += dD;

        pulse_L = 0.5f * (float)(dA + dB);
        pulse_R = 0.5f * (float)(dC + dD);
        s_acc_l += (int32_t)(pulse_L + (pulse_L >= 0.f ? 0.5f : -0.5f));
        s_acc_r += (int32_t)(pulse_R + (pulse_R >= 0.f ? 0.5f : -0.5f));

        dL_mm = pulse_L * MM_PER_PULSE;
        dR_mm = pulse_R * MM_PER_PULSE;
        ds_mm = 0.5f * (dL_mm + dR_mm);
        dth_rad = (dR_mm - dL_mm) / (float)WHEELBASE_MM;

        s_dist_cm += ds_mm / 10.f;
        s_heading_deg += dth_rad * (180.f / M_PI);
    }

    /* 持续输出：HOLD 指令每拍刷新 */
    if (s_enabled && (s_cmd_l != 0 || s_cmd_r != 0))
        apply_lr(s_cmd_l, s_cmd_r);
}
