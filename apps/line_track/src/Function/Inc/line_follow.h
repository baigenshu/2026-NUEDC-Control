/**
 * @file line_follow.h
 * @brief 四路红外巡线 · 编码器闭环速度控制
 *
 * 每 10ms 控制周期：
 *   传感器读数 → 加权线位置 → P 控制器修正 → 左右速度目标 → 四轮速度 PID
 */
#ifndef LINE_FOLLOW_H
#define LINE_FOLLOW_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LF_STATE_IDLE  = 0,
    LF_STATE_TRACK = 1,
} lf_state_t;

void LineFollow_Init(void);
void LineFollow_Reset(void);

void LineFollow_SetEnable(bool on);
bool LineFollow_IsEnabled(void);

/** 基础速度目标（encoder pulses / 10ms），由 KEY_SPD 写入 */
void LineFollow_SetBaseSpd(int16_t spd);
int16_t LineFollow_GetBaseSpd(void);

/** 每控制周期调用：读 IR → 算位置 → 设速度目标 → 速度 PID → 电机 */
void LineFollow_Update(void);

/* ---- 遥测 ---- */
float    LineFollow_GetError(void);
uint8_t  LineFollow_GetMask(void);
lf_state_t LineFollow_GetState(void);

/** 最近一次编码器增量（pulses / 10ms，前进为正），用于 OLED/UART */
void LineFollow_GetEncoderDeltas(int32_t *a, int32_t *b,
                                 int32_t *c, int32_t *d);

#endif /* LINE_FOLLOW_H */
