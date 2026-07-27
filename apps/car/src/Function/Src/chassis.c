#include "chassis.h"
#include "motor.h"
#include "encoder.h"

/* 前后轮机械镜像：后轮取反，使四轮“前进”同向 */
#define POL_A   (-1)  /* 右后 */
#define POL_B   (+1)  /* 右前 */
#define POL_C   (+1)  /* 左前 */
#define POL_D   (-1)  /* 左后 */

#define PWM_MIN  (-100)
#define PWM_MAX  (100)

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

static void drive4(int16_t left, int16_t right, int use_trim)
{
    int16_t l_out;
    int16_t r_out;

    left = clamp_pwm(left);
    right = clamp_pwm(right);

    if (use_trim) {
        l_out = apply_trim(left, ROBOT_LEFT_TRIM);
        r_out = apply_trim(right, ROBOT_RIGHT_TRIM);
    } else {
        l_out = left;
        r_out = right;
    }

    MotorC_SetSpeed(apply_pol(l_out, POL_C));
    MotorD_SetSpeed(apply_pol(l_out, POL_D));
    MotorB_SetSpeed(apply_pol(r_out, POL_B));
    MotorA_SetSpeed(apply_pol(r_out, POL_A));

    s_st.left_cmd = l_out;
    s_st.right_cmd = r_out;
}

void Chassis_Init(void)
{
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
}

void Chassis_Stop(void)
{
    Motor_AllStop();
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
}

void Chassis_Drive(int16_t left, int16_t right)
{
    drive4(left, right, 1);
}

void Chassis_SpinLeft(int16_t speed)
{
    int16_t spd = clamp_pwm(speed);

    if (spd < 0) {
        spd = (int16_t)(-spd);
    }
    /* 左退右进，四轮等速，不配平 */
    drive4((int16_t)(-spd), spd, 0);
}

void Chassis_SpinRight(int16_t speed)
{
    int16_t spd = clamp_pwm(speed);

    if (spd < 0) {
        spd = (int16_t)(-spd);
    }
    drive4(spd, (int16_t)(-spd), 0);
}

void Chassis_TurnLeft(int16_t base, int16_t turn)
{
    int16_t spd = (int16_t)(base + turn);

    if (spd < 0) {
        spd = 0;
    }
    if (spd > 100) {
        spd = 100;
    }
    Chassis_SpinLeft(spd);
}

void Chassis_TurnRight(int16_t base, int16_t turn)
{
    int16_t spd = (int16_t)(base + turn);

    if (spd < 0) {
        spd = 0;
    }
    if (spd > 100) {
        spd = 100;
    }
    Chassis_SpinRight(spd);
}

/*
 * 与 drive4 电机极性一致：后轮机械镜像，编码器原始方向相反。
 * 直接 C+D / B+A 会正负抵消 → 航向/路程恒约 0 → 定角永不停止。
 * 正方向 = 车体前进时该侧累计增加。
 */
int32_t Chassis_GetLeftCount(void)
{
    return (int32_t)(POL_C) * EncoderC_Count + (int32_t)(POL_D) * EncoderD_Count;
}

int32_t Chassis_GetRightCount(void)
{
    return (int32_t)(POL_B) * EncoderB_Count + (int32_t)(POL_A) * EncoderA_Count;
}

const Chassis_Status_t *Chassis_GetStatus(void)
{
    return &s_st;
}
