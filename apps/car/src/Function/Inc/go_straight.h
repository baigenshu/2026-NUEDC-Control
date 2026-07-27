#ifndef __GO_STRAIGHT_H
#define __GO_STRAIGHT_H

#include <stdint.h>

/*
 * 编码器差速闭环走直线
 *
 * 依赖: chassis + encoder + pid
 * 建议控制周期 10~20ms 调用 GoStraight_Step()
 *
 * 用法:
 *   GoStraight_Init();
 *   GoStraight_Start(15);          // 基准速度 0~100
 *   while (...) {
 *       GoStraight_Step();         // 周期调用
 *   }
 *   GoStraight_Stop();
 */

typedef struct {
    int16_t base;       /* 基准速度 */
    int16_t left_cmd;   /* 实际左指令 */
    int16_t right_cmd;  /* 实际右指令 */
    int32_t left_cnt;   /* 相对累计左脉冲 */
    int32_t right_cnt;  /* 相对累计右脉冲 */
    int32_t err;        /* left_cnt - right_cnt */
    uint8_t running;    /* 1=运行中 */
} GoStraight_Status_t;

void GoStraight_Init(void);

/* 设定基准速度并清零相对编码器，开始走直线；base 可负（后退） */
void GoStraight_Start(int16_t base_speed);

/* 控制周期内调用一次；未 Start 时无动作 */
void GoStraight_Step(void);

/* 停车并结束 */
void GoStraight_Stop(void);

/* 仅改速度，不重置编码器基准 */
void GoStraight_SetSpeed(int16_t base_speed);

const GoStraight_Status_t *GoStraight_GetStatus(void);

#endif
