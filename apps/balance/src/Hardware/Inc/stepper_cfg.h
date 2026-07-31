/**
 * @file stepper_cfg.h
 * @brief TMC_A 步进 + 曲柄连杆摆杆执行机构
 *
 * 机械（按 CAD / 赛题结构）：
 *   左端合页铰支 → 水平凹槽摆杆（约 25 cm）→ 右端曲柄连杆抬升/下压
 *   步进电机驱动蓝色曲柄，经连杆改变摆杆倾角，从而控制钢珠一维位置
 *
 * 电机：42 系 1.8° · 1/16 微步（UART MRES 覆盖 MS 脚）· TMC2209
 * 引脚：EN=PA14 DIR=PA13 STEP=PA12 · TIMG7 10us
 *
 * 控制接口仍用“抽象倾角单位”0.01 unit：
 *   BallCtrl 输出 rod_x100 → Stepper_SetTargetMm_x100()
 *   1.00 unit 对应 STEPPER_STEPS_PER_UNIT 微步（近水平线性化，可标定）
 */
#ifndef STEPPER_CFG_H
#define STEPPER_CFG_H

#include <stdint.h>

/* ---------- 电机电气（电流在 tmc2209_cfg.h） ---------- */
#ifndef STEPPER_MOTOR_RATED_MA
#define STEPPER_MOTOR_RATED_MA      (1200u)
#endif

/* ---------- 微步 / 电机 ---------- */
#ifndef STEPPER_MICROSTEPS
#define STEPPER_MICROSTEPS          (16u)    /* UART → 1/16，减段落感 */
#endif

#ifndef STEPPER_FULLSTEPS_PER_REV
#define STEPPER_FULLSTEPS_PER_REV   (200u)   /* 1.8° */
#endif

#define STEPPER_STEPS_PER_REV \
    ((uint32_t)STEPPER_FULLSTEPS_PER_REV * (uint32_t)STEPPER_MICROSTEPS)

/* 每微步电机轴转角 0.1125° @ 1/16 → 0.01° 量化为 11 */
#define STEPPER_MOTOR_DEG_X100_PER_STEP (11)

/* ---------- 抽象倾角单位 → 微步（曲柄近水平标定） ---------- */
/* 1.00 unit = 160 microsteps @1/16 ≈ 18° 电机轴（与旧 80@1/8 同角） */
#ifndef STEPPER_STEPS_PER_UNIT
#define STEPPER_STEPS_PER_UNIT      (160u)
#endif

/* 兼容旧 API 名：Mm_x100 实为 tilt_x100（0.01 unit） */
#define STEPPER_LEAD_UM             (1000u)  /* 占位，不再按丝杆解释 */

/* ---------- 速度 / 加速度（抬高，减梯形段落感） ---------- */
#define STEPPER_TICK_US             (10u)
#define STEPPER_SPS_HARD_MAX        (1000000u / (STEPPER_TICK_US * 2u))

#ifndef STEPPER_DEFAULT_SPS
#define STEPPER_DEFAULT_SPS         (6000u)
#endif

#ifndef STEPPER_MAX_SPS
#define STEPPER_MAX_SPS             (14000u)
#endif

#ifndef STEPPER_START_SPS
#define STEPPER_START_SPS           (800u)
#endif

#ifndef STEPPER_ACCEL_SPS2
#define STEPPER_ACCEL_SPS2          (50000u)
#endif

/* 若倾角方向与视觉 + 反向：改为 -1 */
#ifndef STEPPER_DIR_SIGN
#define STEPPER_DIR_SIGN            (1)
#endif

#ifndef STEPPER_EN_ACTIVE_LOW
#define STEPPER_EN_ACTIVE_LOW       (1)
#endif

/* 曲柄硬软限位：约 ±20° 电机轴 @ 1/16 → ±180 step */
#ifndef STEPPER_SOFT_MIN_STEPS
#define STEPPER_SOFT_MIN_STEPS      (-180)
#endif

#ifndef STEPPER_SOFT_MAX_STEPS
#define STEPPER_SOFT_MAX_STEPS      ( 180)
#endif

#endif /* STEPPER_CFG_H */
