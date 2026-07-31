/**
 * @file ball_ctrl.h
 * @brief 视觉球位单环位置 PD → 曲柄/摆杆倾角
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

/** 停球目标，单位 0.01 mm（相对视觉 O） */
void    BallCtrl_SetTargetMm_x100(int32_t mm_x100);
int32_t BallCtrl_GetTargetMm_x100(void);

/** 停球目标，单位整 mm（与视觉协议一致） */
void BallCtrl_SetTargetMm(int16_t pos_mm);

/** 主循环：VisionUart_Poll 之后调用 */
void BallCtrl_Update(void);

ball_ctrl_state_t BallCtrl_GetState(void);
int32_t BallCtrl_GetBallMm_x100(void);     /* 滤波后球位 */
int32_t BallCtrl_GetRodMm_x100(void);      /* 当前倾角指令（0.01 unit） */
bool    BallCtrl_IsSettled(void);

#endif /* BALL_CTRL_H */
