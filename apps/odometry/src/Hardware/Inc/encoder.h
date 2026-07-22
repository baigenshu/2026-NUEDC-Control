#ifndef __ENCODER_H
#define __ENCODER_H

#include <stdint.h>

/*
 * EncA: hardware QEI TIMG8  PB6/PB7
 * EncB: GPIO both-edge IRQ  PB0/PB5
 *
 * MSPM0G3507 has only ONE QEI-capable timer (TIMG8).
 * Dual pure-QEI is not available on this silicon.
 */

extern volatile int32_t EncoderA_Count;
extern volatile int32_t EncoderB_Count;

void Encoder_Init(void);
void Encoder_Sample(void);
void EncoderA_Reset(void);
void EncoderB_Reset(void);

#endif
