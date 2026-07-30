/**
 * @file stepper.h
 * @brief TMC_A 单轴开环步进驱动（STEP/DIR/EN + 梯形加减速）
 *
 * 引脚：STEP=PA12 · DIR=PA13 · EN=PA14
 * 脉冲：TIMG7（STEP_TIM）10 µs 节拍，IRQ 内 bit-bang
 * 标定：stepper_cfg.h
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

/** 丝杆直线位移 API（内部按 STEPPER_LEAD_UM 换算微步） */
void Stepper_MoveUm(int32_t delta_um);
void Stepper_SetTargetUm(int32_t target_um);
void Stepper_MoveMm_x100(int32_t delta_mm_x100);   /* 0.01 mm */
void Stepper_SetTargetMm_x100(int32_t target_mm_x100);

int32_t Stepper_GetPositionUm(void);
int32_t Stepper_GetPositionMm_x100(void);

uint32_t Stepper_GetPulseCount(void);
void     Stepper_ClearPulseCount(void);

/** STEP_TIM ZERO 中断中调用（stepper.c 已实现 IRQHandler） */
void Stepper_OnTimerTick(void);

#endif /* STEPPER_H */