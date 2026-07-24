#include "chassis.h"
#include "motor.h"
#include "encoder.h"

/* 前后轮机械镜像：后轮取反，使四轮“前进”同向 */
#define POL_A   (-1)  /* 右后 */
#define POL_B   (+1)  /* 右前 */
#define POL_C   (+1)  /* 左前 */
#define POL_D   (-1)  /* 左后 */

/* 开环左右配平（%）：右轮偏快则降 RIGHT_TRIM */
#define LEFT_TRIM    100
#define RIGHT_TRIM    85

#define PWM_MIN    (-100)
#define PWM_MAX    (100)

static Chassis_Status_t s_st;

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

static int16_t apply_pol(int16_t cmd, int pol)
{
    return (int16_t)(cmd * pol);
}

static int16_t apply_trim(int16_t cmd, int trim_pct)
{
    int32_t v = ((int32_t)cmd * (int32_t)trim_pct) / 100;
    return clamp_pwm((int16_t)v);
}

void Chassis_Init(void)
{
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
}

void Chassis_Drive(int16_t left, int16_t right)
{
    int16_t l_out;
    int16_t r_out;

    left = clamp_pwm(left);
    right = clamp_pwm(right);

    l_out = apply_trim(left, LEFT_TRIM);
    r_out = apply_trim(right, RIGHT_TRIM);

    MotorC_SetSpeed(apply_pol(l_out, POL_C));
    MotorD_SetSpeed(apply_pol(l_out, POL_D));
    MotorB_SetSpeed(apply_pol(r_out, POL_B));
    MotorA_SetSpeed(apply_pol(r_out, POL_A));

    s_st.left_cmd = l_out;
    s_st.right_cmd = r_out;
}

void Chassis_Stop(void)
{
    Motor_AllStop();
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
}

void Chassis_TurnLeft(int16_t base, int16_t turn)
{
    int16_t left;
    int16_t right;

    if (base < 0) {
        base = 0;
    }
    if (base > 100) {
        base = 100;
    }
    if (turn < 0) {
        turn = 0;
    }
    if (turn > 100) {
        turn = 100;
    }

    left = (int16_t)(base - turn);
    right = (int16_t)(base + (turn / 2));
    Chassis_Drive(left, right);
}

void Chassis_TurnRight(int16_t base, int16_t turn)
{
    int16_t left;
    int16_t right;

    if (base < 0) {
        base = 0;
    }
    if (base > 100) {
        base = 100;
    }
    if (turn < 0) {
        turn = 0;
    }
    if (turn > 100) {
        turn = 100;
    }

    left = (int16_t)(base + (turn / 2));
    right = (int16_t)(base - turn);
    Chassis_Drive(left, right);
}

int32_t Chassis_GetLeftCount(void)
{
    return EncoderC_Count + EncoderD_Count;
}

int32_t Chassis_GetRightCount(void)
{
    return EncoderB_Count + EncoderA_Count;
}

const Chassis_Status_t *Chassis_GetStatus(void)
{
    return &s_st;
}
