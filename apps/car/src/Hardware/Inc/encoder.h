#ifndef __ENCODER_H
#define __ENCODER_H

#include <stdint.h>

/*
 * Dual GPIO x4 quadrature (gray-code) — car pin map
 *   EncA: PB0 / PB5
 *   EncB: PB23 / PB18
 */

extern volatile int32_t EncoderA_Count;
extern volatile int32_t EncoderB_Count;

/* Speed in pulses/sec (updated by Encoder_UpdateSpeed) */
extern volatile float EncoderA_Speed;
extern volatile float EncoderB_Speed;

void Encoder_Init(void);
void Encoder_Sample(void);
void Encoder_UpdateSpeed(void);
void EncoderA_Reset(void);
void EncoderB_Reset(void);

#endif
