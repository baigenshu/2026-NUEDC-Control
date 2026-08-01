/**
 * @file ball_ctrl.h
 * @brief 视觉球位位置 PID → 曲柄/摆杆倾角
 *
 * 初步目标：小车静止时，把钢珠停在 O 或任意定点。
 */
#ifndef BALL_CTRL_H
#define BALL_CTRL_H

#include <stdint.h>
#include <stdbool.h>
#include "ball_proto.h"

typedef enum {
    BALL_CTRL_STATE_IDLE = 0,   /* 未使能 */
    BALL_CTRL_STATE_RUN,        /* 闭环中 */
    BALL_CTRL_STATE_LOST,       /* 丢球/超时，倾角回 0 */
    BALL_CTRL_STATE_SETTLED,    /* 已在死区内 */
} ball_ctrl_state_t;

void BallCtrl_Init(void);

/** SysTick 1ms 调用 */
void BallCtrl_OnMsTick(void);

/** 使能闭环（自动 Stepper_SetEnable） */
void BallCtrl_Enable(bool on);
bool BallCtrl_IsEnabled(void);

/** 将当前摆臂位置设为机械零点，然后立即启动闭环 */
void BallCtrl_ZeroArmAndStart(void);

/**
 * 固定轨迹期间直接指定摆臂目标，单位 0.01 抽象倾角单位。
 * 正负方向与 PID 控制误差一致，内部自动应用 BALL_CTRL_SIGN。
 * 启用后视觉仍更新观测和 trace，但不会用 PID 覆盖该执行器目标。
 */
void BallCtrl_SetCommandOverrideMm_x100(int32_t command_mm_x100);
void BallCtrl_ClearCommandOverride(void);
bool BallCtrl_IsCommandOverrideActive(void);
void BallCtrl_SetPresetControl(bool on);
void BallCtrl_SetHoldBiasMm_x100(int32_t bias_mm_x100);
void BallCtrl_ClearHoldBias(void);

/** 停球目标，单位 0.01 mm（相对视觉 O） */
void    BallCtrl_SetTargetMm_x100(int32_t mm_x100);
int32_t BallCtrl_GetTargetMm_x100(void);
void    BallCtrl_SetTrackingTargetMm_x100(int32_t mm_x100);

/** 停球目标，单位整 mm（与视觉协议一致） */
void BallCtrl_SetTargetMm(int16_t pos_mm);

/** 主循环：VisionUart_Poll 之后调用；accept_setpoint=false 时忽略 0x12 */
void BallCtrl_Update(bool accept_setpoint);

ball_ctrl_state_t BallCtrl_GetState(void);
int32_t BallCtrl_GetBallMm_x100(void);     /* 滤波后球位 */
float   BallCtrl_GetVelocityMm_s(void);    /* 滤波后球速 */
int32_t BallCtrl_GetRodMm_x100(void);      /* 当前开环执行命令，不是反馈 */
bool    BallCtrl_IsSettled(void);

#endif /* BALL_CTRL_H */
