#ifndef __ENCODER_H
#define __ENCODER_H

#include <stdint.h>

/*
 * Dual GPIO x4 quadrature (gray-code) — LQFP-48
 *   EncA: PB6 / PB7
 *   EncB: PB8  / PB9
 */

extern volatile int32_t EncoderA_Count;
extern volatile int32_t EncoderB_Count;

void Encoder_Init(void);
void Encoder_Sample(void);
void EncoderA_Reset(void);
void EncoderB_Reset(void);

#endif
