/**
 * @file speed_ctrl.h
 * @brief 四轮独立速度 PID 闭环
 *
 * 每路：目标速度（encoder pulses/10ms）→ 编码器反馈 → PID → PWM 占空比
 * 内部处理电机极性（POL_A/B/C/D），上层只需给定前进为正的速度值。
 */
#ifndef SPEED_CTRL_H
#define SPEED_CTRL_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SPEED_ID_A = 0,
    SPEED_ID_B = 1,
    SPEED_ID_C = 2,
    SPEED_ID_D = 3,
    SPEED_ID_COUNT
} speed_id_t;

void SpeedCtrl_Init(void);
void SpeedCtrl_Reset(void);

void SpeedCtrl_SetEnable(bool on);

/** 单路速度目标（encoder pulses / control period） */
void SpeedCtrl_SetTarget(speed_id_t id, int16_t spd);

/** 批量设左右侧：A/B = left, C/D = right */
void SpeedCtrl_SetTargetLR(int16_t left_spd, int16_t right_spd);

/**
 * 每控制周期调用：
 *   读编码器增量 → 四路独立 PID → Motor_Set（含 POL 修正）
 */
void SpeedCtrl_Update(void);

/** 最近一次编码器增量（pulses/sample，前进为正） */
int32_t SpeedCtrl_GetEncoderDelta(speed_id_t id);

#endif /* SPEED_CTRL_H */
