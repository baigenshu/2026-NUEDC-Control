#ifndef __CHASSIS_H
#define __CHASSIS_H

#include <stdint.h>
#include "robot_config.h"

/*
 * 四轮差速（开环，加 PID 前版本）
 *
 * 轮位:
 *   左前 C    右前 B
 *   左后 D    右后 A
 *
 * 指令 -100~100，正=车体前进
 * 轮距 16cm，轴距 20cm
 */

typedef struct {
    int16_t left_cmd;
    int16_t right_cmd;
} Chassis_Status_t;

void Chassis_Init(void);
void Chassis_Stop(void);

/* 左右差速 */
void Chassis_Drive(int16_t left, int16_t right);

/* 原地转：左退右进 / 左进右退，speed 为 PWM% */
void Chassis_SpinLeft(int16_t speed);
void Chassis_SpinRight(int16_t speed);

/* 兼容旧接口 */
void Chassis_TurnLeft(int16_t base, int16_t turn);
void Chassis_TurnRight(int16_t base, int16_t turn);

int32_t Chassis_GetLeftCount(void);
int32_t Chassis_GetRightCount(void);
const Chassis_Status_t *Chassis_GetStatus(void);

#endif
