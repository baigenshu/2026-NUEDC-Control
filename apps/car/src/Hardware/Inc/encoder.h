/**
 * @file encoder.h
 * @brief 四路正交编码器（GPIO 双边沿 + 四倍频查表）
 *
 * ISR 内仅计数；测速/差分在 Chassis_Update 中完成。
 * 计数方向由 ENC_SIGN_* 在 Get 时统一乘入，使「车体前进 → 脉冲增加」。
 */
#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

typedef enum {
    ENC_ID_A = 0,
    ENC_ID_B = 1,
    ENC_ID_C = 2,
    ENC_ID_D = 3,
    ENC_ID_COUNT
} enc_id_t;

void Encoder_Init(void);

/** 原始累计脉冲（已乘 ENC_SIGN，前进为正） */
int32_t Encoder_Get(enc_id_t id);

void Encoder_GetAll(int32_t *a, int32_t *b, int32_t *c, int32_t *d);

/** 清零单轮 / 全部；重锁相状态 */
void Encoder_Reset(enc_id_t id);
void Encoder_ResetAll(void);

#endif /* ENCODER_H */
