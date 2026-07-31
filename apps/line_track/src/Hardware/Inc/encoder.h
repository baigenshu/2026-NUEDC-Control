/**
 * @file encoder.h
 * @brief 四路正交编码器（GPIO 双边沿 + 四倍频查表）
 *
 * 对齐 D24A Read_Encoder：Encoder_ReadDelta 返回采样周期内脉冲并更新基线
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

/** 累计脉冲（已乘 ENC_SIGN，前进为正） */
int32_t Encoder_Get(enc_id_t id);

void Encoder_GetAll(int32_t *a, int32_t *b, int32_t *c, int32_t *d);

/**
 * 单位时间内脉冲增量（已乘 ENC_SIGN），读后更新基线
 * 等价 D24A Read_Encoder(TIMx)
 */
int32_t Encoder_ReadDelta(enc_id_t id);

void Encoder_ReadDeltaAll(int32_t *a, int32_t *b, int32_t *c, int32_t *d);

void Encoder_Reset(enc_id_t id);
void Encoder_ResetAll(void);

#endif /* ENCODER_H */
