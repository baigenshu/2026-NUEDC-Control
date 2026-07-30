/**
 * @file chassis_cfg.h
 * @brief car 唯一配置入口
 *
 * 目标（H 题第 2 项）：
 *   A 点按键启动 → 8 路灰度顺时针巡线一圈 → 停回 A 点 → OLED 显示总时间
 *   成绩：总时间 ≤ 20 s，停车偏差 ≤ 2 cm
 *
 * 轮位：A 左后 · B 左前 · C 右前 · D 右后；左=A+B · 右=C+D
 * 符号：线速度 +前 -后；转向 +左 -右
 */
#ifndef CHASSIS_CFG_H
#define CHASSIS_CFG_H

/* ========== 几何 / MG310 霍尔编码器 ========== */
#define WHEEL_DIAMETER_MM              (65)
#define WHEELBASE_MM                   (200)     /* 左右轮中心距 */
#define MG310_HALL_CPR                 (13)
#define MG310_GEAR_RATIO               (30)
#define ENCODER_PPR                    (MG310_HALL_CPR * 4 * MG310_GEAR_RATIO)
#define MM_PER_PULSE                   (3.1415926f * (float)WHEEL_DIAMETER_MM / (float)ENCODER_PPR)

/* 车体前进时脉冲应增加 */
#define ENC_SIGN_A                     (+1)
#define ENC_SIGN_B                     (+1)
#define ENC_SIGN_C                     (+1)
#define ENC_SIGN_D                     (+1)

/* ========== 电机 PWM ========== */
#define PWM_PERIOD                     (4000)
#define PWM_MAX                        (3800)
#define PWM_DEADZONE                   (80)
/* 实车标定：A/D 与 B/C 极性相反 */
#define POL_A                          (-1)
#define POL_B                          (+1)
#define POL_C                          (+1)
#define POL_D                          (-1)
#define LEFT_TRIM                      (100)     /* /100 */
#define RIGHT_TRIM                     (100)

/* 速度 %：限幅 */
#define CHASSIS_SPEED_MAX              (55)

/* ========== 灰度 / 巡线 ==========
 * 物理从左到右 G1..G8 = bit0..bit7
 * 关键：|turn| 不得超过 base，否则一侧反转 → 原地抖 */
#define GRAY_ACTIVE_LOW                (1)

/* G1(左)…G8(右)：中心轻、外侧重 */
#define GRAY_WEIGHT_0                  (-2800)
#define GRAY_WEIGHT_1                  (-1800)
#define GRAY_WEIGHT_2                  (-1000)
#define GRAY_WEIGHT_3                  (-350)
#define GRAY_WEIGHT_4                  (350)
#define GRAY_WEIGHT_5                  (1000)
#define GRAY_WEIGHT_6                  (1800)
#define GRAY_WEIGHT_7                  (2800)

#define LT_BASE_SPEED_DEFAULT          (30)
#define LT_KP                          (0.028f)
#define LT_KD                          (0.016f)
#define LT_TURN_SIGN                   (-1)      /* 线偏左 → 左转 */
#define LT_TURN_LIMIT                  (22)      /* 绝对上限，再被 base 二次限制 */
#define LT_TURN_SLEW                   (5)       /* 每周期转向变化上限 */
#define LT_ERROR_DEADZONE              (200)
#define LT_ERROR_FILTER                (0.40f)
#define LT_LOST_DEBOUNCE               (8)

/* ========== 一圈任务（H 题第 2 项）==========
 * 停车线 = 垂直横条：多路灰度同时黑
 * 必须先离开起点，再跑过最小里程，才允许认终点 */
#define LAP_TRACK_SPEED                (30)
#define LAP_START_SPEED                (18)
#define LAP_START_MS                   (300u)
#define LAP_FINISH_SPEED               (20)
#define LAP_TRACK_LENGTH_CM            (614.0f)  /* 2×1.5 + 2π×0.5 */
#define LAP_LEAVE_START_CM             (25.0f)
#define LAP_MIN_DISTANCE_CM            (350.0f)  /* < 整圈，> 半圈防误停 */
#define LAP_FINISH_SLOWDOWN_CM         (80.0f)
#define LAP_MARKER_MIN_ACTIVE          (4u)
#define LAP_MARKER_CONFIRM             (3u)
#define LAP_TIME_LIMIT_MS              (25000u)  /* 安全兜底；成绩仍显示实际时间 */

#endif /* CHASSIS_CFG_H */
