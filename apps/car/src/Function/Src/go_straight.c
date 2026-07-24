#include "go_straight.h"
#include "chassis.h"
#include "pid.h"

/* 直线差速 PID（作用在 左累计-右累计） */
#define GS_KP       0.06f
#define GS_KI       0.000f
#define GS_KD       0.01f
#define GS_OUT_MAX  8.0f

#define PWM_MIN    (-100)
#define PWM_MAX    (100)

static PID_t s_pid;
static GoStraight_Status_t s_st;
static int32_t s_base_left;
static int32_t s_base_right;

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
}

void GoStraight_Start(int16_t base_speed)
{
    if (base_speed < 0) {
        base_speed = 0;
    }
    if (base_speed > 100) {
        base_speed = 100;
    }

    PID_Reset(&s_pid);
    s_pid.target = 0.0f;

    s_base_left = Chassis_GetLeftCount();
    s_base_right = Chassis_GetRightCount();

    s_st.base = base_speed;
    s_st.left_cnt = 0;
    s_st.right_cnt = 0;
    s_st.err = 0;
    s_st.running = 1;
}

void GoStraight_SetSpeed(int16_t base_speed)
{
    if (base_speed < 0) {
        base_speed = 0;
    }
    if (base_speed > 100) {
        base_speed = 100;
    }
    s_st.base = base_speed;
}

void GoStraight_Step(void)
{
    int32_t left_cnt;
    int32_t right_cnt;
    int32_t err;
    float corr;
    int16_t left_pwm;
    int16_t right_pwm;
    int16_t min_fwd;
    int16_t base;

    if (!s_st.running) {
        return;
    }

    base = s_st.base;
    left_cnt = Chassis_GetLeftCount() - s_base_left;
    right_cnt = Chassis_GetRightCount() - s_base_right;
    err = left_cnt - right_cnt;

    /*
     * target=0, feedback=err → corr = K*(0-err) = -K*err
     * 左超前 err>0 → corr<0 → 左减速、右加速
     */
    corr = PID_CalcLinear(&s_pid, (float)err);

    left_pwm = (int16_t)((float)base + corr);
    right_pwm = (int16_t)((float)base - corr);

    /* 前进时两侧禁止倒转，避免原地打转 */
    min_fwd = (int16_t)(base / 3);
    if (min_fwd < 3) {
        min_fwd = 3;
    }
    if (base > 0) {
        if (left_pwm < min_fwd) {
            left_pwm = min_fwd;
        }
        if (right_pwm < min_fwd) {
            right_pwm = min_fwd;
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
