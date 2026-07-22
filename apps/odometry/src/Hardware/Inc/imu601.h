#ifndef __IMU601_H
#define __IMU601_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/*
 * 汇电籽-601 — UART1 + DMA RX
 *
 * 接线: T→PA9(RX)  R→PA8(TX)  V→5V  G→GND
 * 软复位:  AA 55 60 12 00 72
 * 校准yaw: AA 55 60 14 04 66 E6 B4 43 BB
 */

typedef struct {
    float yaw;
    float pitch;
    float roll;
} IMU601_Attitude_t;

extern volatile IMU601_Attitude_t IMU601_Attitude;
extern volatile uint32_t IMU601_FrameCount;

void IMU601_Init(void);
void IMU601_Poll(void); /* parse any pending DMA chunks */

static inline uint8_t IMU601_DataReady(void)
{
    return (IMU601_FrameCount != 0u) ? 1u : 0u;
}

#endif
