/**
 * @file stepper.h
 * @brief TMC_A 单轴开环步进（STEP/DIR/EN + 梯形加减速）
 *
 * 引脚：STEP=PA12 · DIR=PA13 · EN=PA14
 * 脉冲：TIMG7（STEP_TIM）10 µs 节拍，IRQ 内 bit-bang
 * 机械：曲柄连杆 → 摆杆倾角（非丝杆）
 * 标定：stepper_cfg.h
 *
 * 位置 API：
 *   *Steps     — 微步（曲柄电机轴）
 *   *Deg_x100  — 电机轴角 0.01°
 *   *Mm_x100   — 抽象倾角单位 0.01 unit（BallCtrl 使用）
 */
#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>
#include <stdbool.h>

void Stepper_Init(void);

void Stepper_SetEnable(bool on);
bool Stepper_IsEnabled(void);

void Stepper_Stop(void);
void Stepper_EmergencyStop(void);

void Stepper_MoveSteps(int32_t delta_steps);
void Stepper_SetTargetSteps(int32_t target_steps);
void Stepper_SetZero(void);

int32_t Stepper_GetPositionSteps(void);
int32_t Stepper_GetTargetSteps(void);
bool    Stepper_IsBusy(void);

void     Stepper_SetSpeedSps(uint32_t sps);
uint32_t Stepper_GetSpeedSps(void);
uint32_t Stepper_GetCurrentSps(void);

void     Stepper_SetAccel(uint32_t sps2);
uint32_t Stepper_GetAccel(void);

/** 电机轴角度 API，单位 0.01° */
void Stepper_MoveDeg_x100(int32_t delta_deg_x100);
void Stepper_SetTargetDeg_x100(int32_t target_deg_x100);
int32_t Stepper_GetPositionDeg_x100(void);

/**
 * 抽象倾角单位 API（历史名 Mm_x100，实为 tilt_x100）
 * 1.00 unit = STEPPER_STEPS_PER_UNIT 微步
 */
void Stepper_MoveMm_x100(int32_t delta_mm_x100);
void Stepper_SetTargetMm_x100(int32_t target_mm_x100);
int32_t Stepper_GetPositionMm_x100(void);

/* 兼容旧 um 名：按 0.01 unit * 10 解释，不建议新代码使用 */
void Stepper_MoveUm(int32_t delta_um);
void Stepper_SetTargetUm(int32_t target_um);
int32_t Stepper_GetPositionUm(void);

uint32_t Stepper_GetPulseCount(void);
void     Stepper_ClearPulseCount(void);

void Stepper_OnTimerTick(void);

#endif /* STEPPER_H */
