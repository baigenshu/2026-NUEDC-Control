#ifndef __MOTION_H
#define __MOTION_H

#include <stdint.h>

/*
 * 运动原语（Function 层）
 *
 * 已实现:
 *   转指定角度  Motion_TurnAngle / Motion_TurnAngle_Wait
 *   走指定距离  Motion_GoDistance / Motion_GoDistance_Wait
 *
 * 标定: robot_config.h 中 ROBOT_TURN_SCALE / ROBOT_DIST_SCALE
 *
 * 非阻塞:
 *   Motion_GoDistance(0.5f, 18);
 *   while (!Motion_IsDone()) { Motion_Step(10); delay_ms(10); }
 *
 * 阻塞:
 *   Motion_GoDistance_Wait(0.5f, 18, 10);  // +前进 / -后退，米，PWM，周期ms
 *   Motion_TurnAngle_Wait(90.0f, 22, 10);
 */

typedef enum {
    MOTION_IDLE = 0,
    MOTION_GO_DIST,
    MOTION_TURN,
    MOTION_DONE,
    MOTION_ABORTED
} Motion_State_t;

typedef struct {
    Motion_State_t state;
    float target_m;       /* GO_DIST 目标 m */
    float feedback_m;     /* GO_DIST 已走路程 m */
    float target_deg;     /* TURN 目标 deg */
    float feedback_deg;   /* TURN 已转 deg */
    int16_t cmd_pwm;      /* 基准 PWM 幅值 */
    int16_t left_cmd;
    int16_t right_cmd;
    int32_t pulse_now;
    int32_t pulse_target;
    uint8_t done;
} Motion_Status_t;

void Motion_Init(void);

/* meters>0 前进，<0 后退；base_pwm 幅值 0~100 */
void Motion_GoDistance(float meters, int16_t base_pwm);

/* deg>0 左转(CCW)，deg<0 右转(CW)；spin_pwm 幅值 0~100 */
void Motion_TurnAngle(float deg, int16_t spin_pwm);

/* 周期调用；period_ms 用于超时计时 */
void Motion_Step(uint16_t period_ms);

uint8_t Motion_IsDone(void);
void Motion_Abort(void);
const Motion_Status_t *Motion_GetStatus(void);

void Motion_GoDistance_Wait(float meters, int16_t base_pwm, uint16_t period_ms);
void Motion_TurnAngle_Wait(float deg, int16_t spin_pwm, uint16_t period_ms);

#endif
