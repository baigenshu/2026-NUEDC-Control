#include "kinematics.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static float s_circumference;
static float s_wheelbase;
static float s_track;
static float s_max_rpm;

void Kine_Init(float wheel_diameter_m, float wheelbase_m, float track_m,
               int motor_max_rpm)
{
    s_circumference = (float)M_PI * wheel_diameter_m;
    s_wheelbase = wheelbase_m;
    s_track = track_m;
    s_max_rpm = (float)motor_max_rpm;
    if (s_circumference < 1e-4f) {
        s_circumference = 1e-4f;
    }
    if (s_max_rpm < 1.0f) {
        s_max_rpm = 1.0f;
    }
}

/*
 * linorobot getRPM，差速用 L/2 标准式（与 README 原理一致）:
 *   v_L = vx - ω*(L/2),  v_R = vx + ω*(L/2)
 * 再加 y 分量以兼容麦克纳姆形式（本车 y 恒 0）
 */
KineWheels_t Kine_GetRPM(float linear_x, float linear_y, float angular_z)
{
    float vx_m = linear_x * 60.0f;
    float vy_m = linear_y * 60.0f;
    float half_L = s_track * 0.5f;
    float half_W = s_wheelbase * 0.5f;
    float x_rpm;
    float y_rpm;
    float tan_rpm;
    KineWheels_t w;

    (void)half_W;
    x_rpm = vx_m / s_circumference;
    y_rpm = vy_m / s_circumference;
    /* 纯差速: v_tang = ω*(L/2) → RPM */
    tan_rpm = (angular_z * 60.0f) * half_L / s_circumference;

    /* FL / FR / RL / RR */
    w.fl = x_rpm - y_rpm - tan_rpm;
    w.fr = x_rpm + y_rpm + tan_rpm;
    w.rl = x_rpm + y_rpm - tan_rpm;
    w.rr = x_rpm - y_rpm + tan_rpm;

    return w;
}

float Kine_RpmToPwm(float rpm)
{
    float p = (rpm / s_max_rpm) * 100.0f;
    if (p > 100.0f) {
        p = 100.0f;
    }
    if (p < -100.0f) {
        p = -100.0f;
    }
    return p;
}

KineWheels_t Kine_GetPWM(float linear_x, float linear_y, float angular_z)
{
    KineWheels_t rpm = Kine_GetRPM(linear_x, linear_y, angular_z);
    KineWheels_t pwm;

    pwm.fl = Kine_RpmToPwm(rpm.fl);
    pwm.fr = Kine_RpmToPwm(rpm.fr);
    pwm.rl = Kine_RpmToPwm(rpm.rl);
    pwm.rr = Kine_RpmToPwm(rpm.rr);
    return pwm;
}

KineVel_t Kine_GetVelocities(float fl_rpm, float fr_rpm, float rl_rpm, float rr_rpm)
{
    KineVel_t vel;
    float avg_x;
    float avg_a;
    float half_L = s_track * 0.5f;

    avg_x = (fl_rpm + fr_rpm + rl_rpm + rr_rpm) * 0.25f;
    vel.linear_x = (avg_x / 60.0f) * s_circumference;

    avg_a = (-fl_rpm + fr_rpm - rl_rpm + rr_rpm) * 0.25f;
    vel.linear_y = 0.0f;
    if (half_L > 1e-4f) {
        vel.angular_z = ((avg_a / 60.0f) * s_circumference) / half_L;
    } else {
        vel.angular_z = 0.0f;
    }
    return vel;
}
