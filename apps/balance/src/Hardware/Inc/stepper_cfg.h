/**
 * @file stepper_cfg.h
 * @brief TMC_A 单轴步进驱动参数（唯一配置入口）
 *
 * 引脚：EN=PA14 DIR=PA13 STEP=PA12 · 时基 TIMG7 10us（见 pins.md / SysConfig）
 * 机械：M5 丝杆，实测 1 圈 ≈ 0.8 mm
 */
#ifndef STEPPER_CFG_H
#define STEPPER_CFG_H

#include <stdint.h>

/* ---------- 微步 / 电机 ---------- */
#ifndef STEPPER_MICROSTEPS
#define STEPPER_MICROSTEPS          (8u)     /* MS1=MS2=GND 常见 1/8 */
#endif

#ifndef STEPPER_FULLSTEPS_PER_REV
#define STEPPER_FULLSTEPS_PER_REV   (200u)   /* 1.8° */
#endif

#define STEPPER_STEPS_PER_REV \
    ((uint32_t)STEPPER_FULLSTEPS_PER_REV * (uint32_t)STEPPER_MICROSTEPS)

/* ---------- 丝杆导程 ---------- */
#ifndef STEPPER_LEAD_UM
#define STEPPER_LEAD_UM             (800u)   /* µm/圈 · 实测 0.8 mm */
#endif

/** 微步/mm：STEPS_PER_REV / (LEAD_UM/1000) */
#define STEPPER_STEPS_PER_MM \
    ((uint32_t)((STEPPER_STEPS_PER_REV * 1000u) / STEPPER_LEAD_UM))

/* ---------- 速度 / 加速度 ---------- */
#define STEPPER_TICK_US             (10u)
#define STEPPER_SPS_HARD_MAX        (1000000u / (STEPPER_TICK_US * 2u)) /* 50000 */

#ifndef STEPPER_DEFAULT_SPS
#define STEPPER_DEFAULT_SPS         (4000u)
#endif

#ifndef STEPPER_MAX_SPS
#define STEPPER_MAX_SPS             (40000u)
#endif

#ifndef STEPPER_START_SPS
#define STEPPER_START_SPS           (800u)
#endif

#ifndef STEPPER_ACCEL_SPS2
#define STEPPER_ACCEL_SPS2          (50000u)
#endif

/* ---------- 方向 / 使能 ---------- */
#ifndef STEPPER_DIR_SIGN
#define STEPPER_DIR_SIGN            (1)      /* 取反方向：-1 */
#endif

#ifndef STEPPER_EN_ACTIVE_LOW
#define STEPPER_EN_ACTIVE_LOW       (1)      /* TMC ENN 低有效 */
#endif

/* ---------- 软限位（微步，相对零点） ---------- */
#ifndef STEPPER_SOFT_MIN_STEPS
#define STEPPER_SOFT_MIN_STEPS      (-40000) /* ~ -20 mm @ 2000 step/mm */
#endif

#ifndef STEPPER_SOFT_MAX_STEPS
#define STEPPER_SOFT_MAX_STEPS      ( 40000)
#endif

#endif /* STEPPER_CFG_H */