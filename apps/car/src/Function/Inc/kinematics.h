#ifndef __KINEMATICS_H
#define __KINEMATICS_H

/*
 * 4WD 差速运动学（移植自 linorobot/kinematics，无 Arduino 依赖）
 * 参考: D:\4WD_Car_Steering_OpenSource\01_linorobot_kinematics
 *
 * 轮序（库内编号）:
 *   motor1 前左 FL    motor2 前右 FR
 *   motor3 后左 RL    motor4 后右 RR
 *
 * 本车映射: FL=C  FR=B  RL=D  RR=A
 *
 * 差速（linear_y=0）:
 *   v_L = v_x - ω * (L/2)
 *   v_R = v_x + ω * (L/2)
 *   原地转: v_x=0, ω≠0 → 左右反向
 */

typedef struct {
    float fl; /* 前左  转速 RPM 或 PWM 视接口 */
    float fr;
    float rl;
    float rr;
} KineWheels_t;

typedef struct {
    float linear_x;  /* m/s  前+ */
    float linear_y;  /* m/s  左+（差速车为 0） */
    float angular_z; /* rad/s 左转 CCW + */
} KineVel_t;

void Kine_Init(float wheel_diameter_m, float wheelbase_m, float track_m,
               int motor_max_rpm);

/* 逆解：车体速度 → 四轮目标 RPM（有符号，+前进） */
KineWheels_t Kine_GetRPM(float linear_x, float linear_y, float angular_z);

/* 逆解：→ PWM 百分比 -100~100 */
KineWheels_t Kine_GetPWM(float linear_x, float linear_y, float angular_z);

/* 正解：四轮 RPM → 车体速度（可选） */
KineVel_t Kine_GetVelocities(float fl_rpm, float fr_rpm, float rl_rpm, float rr_rpm);

float Kine_RpmToPwm(float rpm);

#endif
