#ifndef __ENCODER_H
#define __ENCODER_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/* Encoder pulse counts (updated by GPIOB interrupt handler) */
extern volatile int32_t EncoderA_Count;
extern volatile int32_t EncoderB_Count;

/* Speed in pulses per second (computed by Encoder_UpdateSpeed) */
extern volatile float EncoderA_Speed;
extern volatile float EncoderB_Speed;

/* Initialize encoder GPIO interrupts */
void Encoder_Init(void);

/* Call periodically (e.g. every 10ms) to update speed values */
void Encoder_UpdateSpeed(void);

/* Reset counts to zero */
void EncoderA_Reset(void);
void EncoderB_Reset(void);

#endif /* __ENCODER_H */
