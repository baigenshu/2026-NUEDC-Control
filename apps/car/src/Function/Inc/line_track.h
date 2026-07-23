#ifndef __LINE_TRACK_H
#define __LINE_TRACK_H

#include <stdint.h>

/* 控制周期建议 10ms 调用一次 LineTrack_Step */

typedef struct {
    int16_t left_cmd;   /* 左轮 PWM 指令 (MotorB) */
    int16_t right_cmd;  /* 右轮 PWM 指令 (MotorA) */
    int16_t error;      /* 灰度偏差 */
    uint8_t sensors;    /* 8 路位图 bit0=S1 … bit7=S8 */
    uint8_t lost;       /* 1=丢线 */
    uint8_t hard_corner;/* 1=直角/仅外侧 */
    int8_t  last_side;  /* -1 左, +1 右, 0 未知 */
} LineTrack_Status_t;

void LineTrack_Init(void);

/* 读传感器 → 算差速 → 驱动电机；返回状态供日志 */
void LineTrack_Step(LineTrack_Status_t *st);

/* 可选：在线改基准速度 */
void LineTrack_SetBaseSpeed(int16_t base);

const LineTrack_Status_t *LineTrack_GetStatus(void);

#endif
