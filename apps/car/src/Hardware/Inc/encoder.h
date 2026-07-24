#ifndef __ENCODER_H
#define __ENCODER_H

#include <stdint.h>

/*
 * 四路霍尔编码器 GPIO 四倍频正交（gray-code 查表）
 *   EncA: PB0  / PB5
 *   EncB: PB23 / PB18
 *   EncC: PB27 / PB22
 *   EncD: PB24 / PB26
 */

extern volatile int32_t EncoderA_Count;
extern volatile int32_t EncoderB_Count;
extern volatile int32_t EncoderC_Count;
extern volatile int32_t EncoderD_Count;

/* 脉冲/秒，由 Encoder_UpdateSpeed 更新 */
extern volatile float EncoderA_Speed;
extern volatile float EncoderB_Speed;
extern volatile float EncoderC_Speed;
extern volatile float EncoderD_Speed;

void Encoder_Init(void);
void Encoder_Sample(void);
void Encoder_UpdateSpeed(void);
void EncoderA_Reset(void);
void EncoderB_Reset(void);
void EncoderC_Reset(void);
void EncoderD_Reset(void);
void Encoder_AllReset(void);

/* 读取累计脉冲（不清零） */
int32_t Encoder_GetCount(uint8_t id); /* id: 0=A,1=B,2=C,3=D */

#endif
