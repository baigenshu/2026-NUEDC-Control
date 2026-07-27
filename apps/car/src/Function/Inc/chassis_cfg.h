/**
 * @file chassis_cfg.h
 * @brief 底盘配置唯一入口（宏 / 常量表）
 *
 * 类型与 API 在 chassis.h；本文件只放宏。
 * 数值需上电实测后标定（见 plan §7.4）。
 */
#ifndef CHASSIS_CFG_H
#define CHASSIS_CFG_H

#include <stdint.h>

/* ========== 几何 / 编码器 ========== */
#define WHEEL_DIAMETER_MM              (65)      /* 实测 */
#define WHEELBASE_MM                   (160)     /* 左右轮距 */
#define ENCODER_PPR                    (1040)    /* 四倍频后 脉冲/转 */
#define MM_PER_PULSE                   (3.1415926f * (float)WHEEL_DIAMETER_MM / (float)ENCODER_PPR)

/* 计数方向：车体前进时该轮脉冲应增加；与 POL 独立 */
#define ENC_SIGN_A                     (+1)
#define ENC_SIGN_B                     (+1)
#define ENC_SIGN_C                     (+1)
#define ENC_SIGN_D                     (+1)

/* ========== 电机 / PWM ========== */
#define PWM_PERIOD                     (4000)    /* 与 SysConfig timerCount 一致 */
#define PWM_MAX                        (3800)
#define PWM_DEADZONE                   (80)
#define POL_A                          (-1)      /* 右后，点动后改 */
#define POL_B                          (+1)      /* 右前 */
#define POL_C                          (+1)      /* 左前 */
#define POL_D                          (-1)      /* 左后 */
#define LEFT_TRIM                      (92)      /* /100 */
#define RIGHT_TRIM                     (100)

/* ========== 默认速度（百分比）========== */
#define CHASSIS_SPEED_DEFAULT          (35)
#define CHASSIS_SPEED_SLOW             (20)
#define CHASSIS_SPEED_FAST             (55)
#define CHASSIS_SPEED_MAX              (70)
#define CHASSIS_TURN_SPEED_DEFAULT     (30)

/* ========== 运动到位 / 超时 ========== */
#define MOTION_DIST_TOL_CM             (1.0f)
#define MOTION_ANGLE_TOL_DEG           (3.0f)
#define MOTION_TIMEOUT_MS_DEFAULT      (8000)
#define MOTION_STRAIGHT_KP             (0.15f)
#define MOTION_SLOWDOWN_CM             (8.0f)    /* 末端降速区；0=关闭 */

/* ========== 速度环（P4 可选，默认开环）========== */
#define CHASSIS_DEFAULT_SPEED_MODE     (0)       /* 0=OPENLOOP */
#define SPEED_LOOP_DT_MS               (10)      /* 仅参考；实际用累加 dt */
#define SPEED_KP                       (1.2f)
#define SPEED_KI                       (0.15f)
#define SPEED_I_LIMIT                  (1500.f)  /* 抗饱和 */
#define SPEED_OUT_LIMIT                (PWM_MAX)
#define SPEED_PCT_TO_COUNTS_PER_SEC    (50.0f)   /* 100%↔侧向平均 counts/s */

/* ========== 停车 ========== */
/* 0=DEFAULT 1=COAST 2=BRAKE — 与 chassis_stop_mode_t 一致 */
#define CHASSIS_DEFAULT_STOP_MODE      (2)       /* BRAKE */
#define CHASSIS_MOTION_DONE_STOP_MODE  (2)       /* 到位/超时 */

/* ========== 地面档 ========== */
/* 顺序：turn_scale, spin_scale, angle_gain, speed_limit */
#define SURF_NORMAL  { 1.00f, 1.00f, 1.00f, CHASSIS_SPEED_MAX }
#define SURF_LOW     { 0.85f, 0.80f, 1.00f, 45 }
#define SURF_HIGH    { 1.10f, 1.05f, 1.00f, CHASSIS_SPEED_MAX }

#define CHASSIS_SURFACE_DEFAULT        (0)       /* NORMAL */
#define CHASSIS_TURN_BIAS              (0)

/* ========== IMU / 航向（P5，默认关）========== */
#define IMU_ENABLED                    (0)
#define CHASSIS_HEADING_SOURCE         (0)       /* 0=ENC 1=IMU；2=禁止默认开启 */
#define IMU_YAW_SIGN                   (+1)
#define IMU_YAW_RATE_SIGN              (+1)
#define IMU_UPDATE_HZ                   (100)
#define CHASSIS_IMU_STRAIGHT_KP        (1.2f)
#define CHASSIS_IMU_TURN_KP            (2.0f)
#define IMU_BUS_UART                   (0)
#define IMU_BUS_I2C                    (0)

/* ========== 巡线 ========== */
#define LT_BASE_SPEED_DEFAULT          (CHASSIS_SPEED_DEFAULT)
#define LT_KP                          (0.08f)
#define LT_KI                          (0.0f)
#define LT_KD                          (0.12f)
#define LT_TURN_LIMIT                  (40)      /* |pid turn| 限幅 % */
#define LT_LOST_LINE_POLICY            (0)       /* 0STOP 1HOLD 2SEARCH */
#define LT_LOST_DEBOUNCE               (5)       /* 连续丢线拍数 */
#define LT_SEARCH_TURN                 (25)      /* SEARCH 自旋 turn% */
#define LT_SEARCH_TIMEOUT_MS           (1500)
/* 权重以本 cfg 为准（与 pins G1..G8 对照） */
#define GRAY_WEIGHT_0                  (-3500)
#define GRAY_WEIGHT_1                  (-2500)
#define GRAY_WEIGHT_2                  (-1500)
#define GRAY_WEIGHT_3                  (-500)
#define GRAY_WEIGHT_4                  (500)
#define GRAY_WEIGHT_5                  (1500)
#define GRAY_WEIGHT_6                  (2500)
#define GRAY_WEIGHT_7                  (3500)

#endif /* CHASSIS_CFG_H */
