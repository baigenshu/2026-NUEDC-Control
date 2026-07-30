/**
 * @file stepper_cfg.h
 * @brief TMC_A 步进 + 42×23 电机 / M5 丝杆
 *
 * 电机铭牌：
 *   尺寸 42×23 · 步距角 1.8°(200 fullstep) · 额定 1.2 A
 *   相电阻 4.2 Ω · 相电感 4.0 mH · 堵转扭矩 0.16 N·m
 *
 * 驱动板：STEP/DIR/EN bit-bang；电流由 TMC 硬件设定（非本固件写寄存器）
 * 建议（降发热，摆杆负载轻）：
 *   IRUN  ≈ 0.70–0.90 A（约 0.6–0.75×额定，够 0.16 N·m 轻载）
 *   IHOLD ≈ 0.25–0.40 A（或软件到位关 EN，本工程 ball_ctrl 已做）
 *   TMC2209 常见：VREF ≈ IRUN_peak × Rsense 相关，按板子丝印/手册调电位器
 *   经验：发烫先降 VREF；丢步再略升。VM=12V 时勿长时间 100% 额定电流静置
 *
 * 引脚：EN=PA14 DIR=PA13 STEP=PA12 · TIMG7 10us
 * 机械：M5 丝杆，实测 1 圈 ≈ 0.8 mm
 */
#ifndef STEPPER_CFG_H
#define STEPPER_CFG_H

#include <stdint.h>

/* ---------- 42 电机电气（文档/标定用，驱动电流在 TMC 硬件） ---------- */
#ifndef STEPPER_MOTOR_RATED_MA
#define STEPPER_MOTOR_RATED_MA      (1200u)
#endif
/* 推荐运行电流 mA（轻载摆杆） */
#ifndef STEPPER_IRUN_MA_RECOMMENDED
#define STEPPER_IRUN_MA_RECOMMENDED (800u)
#endif
#ifndef STEPPER_IHOLD_MA_RECOMMENDED
#define STEPPER_IHOLD_MA_RECOMMENDED (300u)
#endif
#ifndef STEPPER_PHASE_R_MOHM
#define STEPPER_PHASE_R_MOHM        (4200u)  /* 4.2 Ω */
#endif
#ifndef STEPPER_PHASE_L_UH
#define STEPPER_PHASE_L_UH          (4000u)  /* 4.0 mH */
#endif

/* ---------- 微步 / 电机 ---------- */
#ifndef STEPPER_MICROSTEPS
#define STEPPER_MICROSTEPS          (8u)     /* MS1=MS2=GND → 1/8 */
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

#define STEPPER_STEPS_PER_MM \
    ((uint32_t)((STEPPER_STEPS_PER_REV * 1000u) / STEPPER_LEAD_UM))

/* ---------- 速度 / 加速度（匹配 42 小惯量 + 电感 4 mH） ---------- */
#define STEPPER_TICK_US             (10u)
#define STEPPER_SPS_HARD_MAX        (1000000u / (STEPPER_TICK_US * 2u))

/* 默认巡航：勿过高，高微步频率 + 满电流 → 啸叫发热 */
#ifndef STEPPER_DEFAULT_SPS
#define STEPPER_DEFAULT_SPS         (3500u)
#endif

#ifndef STEPPER_MAX_SPS
#define STEPPER_MAX_SPS             (20000u)
#endif

/* 起步略低，减微振 */
#ifndef STEPPER_START_SPS
#define STEPPER_START_SPS           (400u)
#endif

#ifndef STEPPER_ACCEL_SPS2
#define STEPPER_ACCEL_SPS2          (25000u)
#endif

#ifndef STEPPER_DIR_SIGN
#define STEPPER_DIR_SIGN            (1)
#endif

#ifndef STEPPER_EN_ACTIVE_LOW
#define STEPPER_EN_ACTIVE_LOW       (1)
#endif

#ifndef STEPPER_SOFT_MIN_STEPS
#define STEPPER_SOFT_MIN_STEPS      (-40000)
#endif

#ifndef STEPPER_SOFT_MAX_STEPS
#define STEPPER_SOFT_MAX_STEPS      ( 40000)
#endif

#endif /* STEPPER_CFG_H */
