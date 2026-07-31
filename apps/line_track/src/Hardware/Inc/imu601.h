#ifndef __IMU601_H
#define __IMU601_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/*
 * 汇电籽-601 — UART1 + DMA RX
 * 接线: T→PA9(RX)  R→PA8(TX)  V→5V  G→GND
 */

typedef struct {
    float yaw;
    float pitch;
    float roll;
} IMU601_Attitude_t;

extern volatile IMU601_Attitude_t IMU601_Attitude;
extern volatile uint32_t IMU601_FrameCount;
extern volatile uint32_t IMU601_DmaIrqCount; /* 诊断：DMA_DONE_RX 中断次数（每 32 字节+1） */

/* 软复位 + 校准 + 等收敛（须水平静止） */
void IMU601_Init(void);

/* 再次校准（同样须静止），清空姿态缓存 */
void IMU601_Calibrate(void);

/* 主循环调用：从 DMA 环解析姿态 */
void IMU601_Poll(void);

static inline uint8_t IMU601_DataReady(void)
{
    return (IMU601_FrameCount != 0u) ? 1u : 0u;
}

#endif
