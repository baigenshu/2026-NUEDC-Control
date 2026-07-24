#ifndef __LINE_TRACK_H
#define __LINE_TRACK_H

#include <stdint.h>

/*
 * 8 路灰度循迹（差速）
 * 依赖: chassis + grayscale_sensor + pid
 * 建议 10~20ms 调用一次 LineTrack_Step()
 *
 * 误差: 负=线偏左需左转, 正=线偏右需右转
 */

typedef struct {
    int16_t left_cmd;    /* 左侧 PWM -100~100 */
    int16_t right_cmd;   /* 右侧 PWM -100~100 */
    int16_t error;       /* 灰度偏差 -3500~3500 */
    uint8_t sensors;     /* bit0=S1 … bit7=S8，1=黑线 */
    uint8_t lost;        /* 1=丢线 */
    uint8_t hard_corner; /* 1=直角/仅外侧 */
    int8_t  last_side;   /* -1 左, +1 右, 0 未知 */
} LineTrack_Status_t;

void LineTrack_Init(void);

/* 读传感器 → PID 差速 → Chassis_Drive；st 可为 NULL */
void LineTrack_Step(LineTrack_Status_t *st);

void LineTrack_Stop(void);

/* 基准速度 0~40 建议 */
void LineTrack_SetBaseSpeed(int16_t base);

const LineTrack_Status_t *LineTrack_GetStatus(void);

#endif
