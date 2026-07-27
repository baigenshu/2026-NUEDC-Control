#include "go_straight.h"
#include "chassis.h"
#include "pid.h"

/*
 * 编码器差速闭环走直线（支持正负基准速度）
 * 保证两侧都有足够最小 PWM，避免一侧被压到几乎不转
 */
#define GS_KP       0.12f
#define GS_KI       0.000f
#define GS_KD       0.04f
#define GS_OUT_MAX  5.0f

/* 1：左超前 → 减速左；若越纠越偏改为 -1 */
#define ENC_ERR_SIGN  (1)

/* 轻微开环偏置，正值=略加强右指令侧 */
#define SIDE_BIAS   2

#define PWM_MIN    (-100)
#define PWM_MAX    (100)

static PID_t s_pid;
static GoStraight_Status_t s_st;
static int32_t s_base_left;
static int32_t s_base_right;
static int32_t s_prev_left;
static int32_t s_prev_right;

static int16_t clamp_pwm(int16_t v)
{
    if (v > PWM_MAX) {
        return PWM_MAX;
    }
    if (v < PWM_MIN) {
        return PWM_MIN;
    }
    return v;
}

static int16_t clamp_base(int16_t base_speed)
{
    if (base_speed > 100) {
        return 100;
    }
    if (base_speed < -100) {
        return -100;
    }
    return base_speed;
}

void GoStraight_Init(void)
{
    PID_Init(&s_pid, GS_KP, GS_KI, GS_KD, GS_OUT_MAX, 0.0f);
    s_st.base = 0;
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
    s_st.left_cnt = 0;
    s_st.right_cnt = 0;
    s_st.err = 0;
    s_st.running = 0;
    s_base_left = 0;
    s_base_right = 0;
    s_prev_left = 0;
    s_prev_right = 0;
}

void GoStraight_Start(int16_t base_speed)
{
    base_speed = clamp_base(base_speed);

    PID_Reset(&s_pid);
    s_pid.target = 0.0f;

    s_base_left = Chassis_GetLeftCount();
    s_base_right = Chassis_GetRightCount();
    s_prev_left = 0;
    s_prev_right = 0;

    s_st.base = base_speed;
    s_st.left_cnt = 0;
    s_st.right_cnt = 0;
    s_st.err = 0;
    s_st.running = 1;
}

void GoStraight_SetSpeed(int16_t base_speed)
{
    s_st.base = clamp_base(base_speed);
}

void GoStraight_Step(void)
{
    int32_t left_cnt;
    int32_t right_cnt;
    int32_t d_left;
    int32_t d_right;
    int32_t err;
    float corr;
    int16_t left_pwm;
    int16_t right_pwm;
    int16_t min_mag;
    int16_t base;
    int16_t base_abs;

    if (!s_st.running) {
        return;
    }

    base = s_st.base;
    base_abs = (base >= 0) ? base : (int16_t)(-base);

    left_cnt = Chassis_GetLeftCount() - s_base_left;
    right_cnt = Chassis_GetRightCount() - s_base_right;

    d_left = left_cnt - s_prev_left;
    d_right = right_cnt - s_prev_right;
    s_prev_left = left_cnt;
    s_prev_right = right_cnt;

    err = (int32_t)ENC_ERR_SIGN * (d_left - d_right);
    corr = PID_CalcLinear(&s_pid, (float)err);

    left_pwm = (int16_t)((float)base + corr - (float)SIDE_BIAS);
    right_pwm = (int16_t)((float)base - corr + (float)SIDE_BIAS);

    /* 两侧至少 |base|*70%，防止一侧几乎不转 */
    min_mag = (int16_t)((base_abs * 7) / 10);
    if (min_mag < 8) {
        min_mag = 8;
    }
    if (base > 0) {
        if (left_pwm < min_mag) {
            left_pwm = min_mag;
        }
        if (right_pwm < min_mag) {
            right_pwm = min_mag;
        }
    } else if (base < 0) {
        if (left_pwm > -min_mag) {
            left_pwm = (int16_t)(-min_mag);
        }
        if (right_pwm > -min_mag) {
            right_pwm = (int16_t)(-min_mag);
        }
    }

    left_pwm = clamp_pwm(left_pwm);
    right_pwm = clamp_pwm(right_pwm);

    Chassis_Drive(left_pwm, right_pwm);

    s_st.left_cmd = left_pwm;
    s_st.right_cmd = right_pwm;
    s_st.left_cnt = left_cnt;
    s_st.right_cnt = right_cnt;
    s_st.err = err;
}

void GoStraight_Stop(void)
{
    Chassis_Stop();
    s_st.running = 0;
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
}

const GoStraight_Status_t *GoStraight_GetStatus(void)
{
    return &s_st;
}
