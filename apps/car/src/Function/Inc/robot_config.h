#ifndef __ROBOT_CONFIG_H
#define __ROBOT_CONFIG_H

/* 几何（实测） */
#define ROBOT_WHEEL_DIAMETER_M  0.048f   /* 轮径 48mm */
#define ROBOT_TRACK_M           0.160f   /* 左右轮距 16cm */
#define ROBOT_WHEELBASE_M       0.200f   /* 前后轴距 20cm */

/* 电机 MG310：霍尔 13 线 ×4 倍频 × 减速 20 */
#define ROBOT_ENC_PPR_MOTOR     13
#define ROBOT_ENC_QUAD          4
#define ROBOT_GEAR_RATIO        20
#define ROBOT_PULSES_PER_REV \
    ((float)(ROBOT_ENC_PPR_MOTOR * ROBOT_ENC_QUAD * ROBOT_GEAR_RATIO))
#define ROBOT_MOTOR_MAX_RPM     400

#define ROBOT_LEFT_TRIM         90
#define ROBOT_RIGHT_TRIM        100

/* 派生：米/脉冲（单轮） */
#ifndef ROBOT_PI
#define ROBOT_PI                3.14159265f
#endif
#define ROBOT_WHEEL_CIRCUM_M \
    (ROBOT_PI * ROBOT_WHEEL_DIAMETER_M)
#define ROBOT_M_PER_PULSE \
    (ROBOT_WHEEL_CIRCUM_M / ROBOT_PULSES_PER_REV)

/*
 * 原地转角标定：目标脉冲 *= 本系数
 * 实测：指令 90° 只转约 30° → 90/30 = 3
 * 再调：new = old * (指令角 / 实际角)
 */
#define ROBOT_TURN_SCALE        3.0f

/*
 * 定距标定：目标脉冲 *= 本系数
 * 判停已改为“有效轮平均”，几何尺度下默认 1.0
 * 再调：new = old * (指令距离 / 实际距离)
 * 例：指令 50cm 实际 200cm → 1.0 * (50/200) = 0.25
 */
#define ROBOT_DIST_SCALE        1.0f

#endif
