#ifndef __CHASSIS_H
#define __CHASSIS_H

#include <stdint.h>

/*
 * 四轮底盘差速驱动
 *
 * 轮位:
 *   左前 C    右前 B
 *   左后 D    右后 A
 *
 * 左: C+D   右: B+A
 * 指令: -100~100，正=车体前进
 */

typedef struct {
    int16_t left_cmd;
    int16_t right_cmd;
} Chassis_Status_t;

void Chassis_Init(void);

/* 差速驱动 */
void Chassis_Drive(int16_t left, int16_t right);
void Chassis_Stop(void);

/* 开环转向: base 前进基准, turn 0~100 越大越急 */
void Chassis_TurnLeft(int16_t base, int16_t turn);
void Chassis_TurnRight(int16_t base, int16_t turn);

/* 左右累计编码器脉冲（用于上层闭环） */
int32_t Chassis_GetLeftCount(void);   /* EncC + EncD */
int32_t Chassis_GetRightCount(void);  /* EncB + EncA */

const Chassis_Status_t *Chassis_GetStatus(void);

#endif
