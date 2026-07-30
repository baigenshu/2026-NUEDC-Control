/**
 * @file vision_uart.h
 * @brief 视觉 UART 收包：球位 type=0x02 + 定点 type=0x12
 */
#ifndef VISION_UART_H
#define VISION_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "ball_proto.h"

/** 主机下发定点：与球位同 framing，type=0x12，定长 13 B */
#ifndef BALL_CMD_TYPE_SETPOINT
#define BALL_CMD_TYPE_SETPOINT          BALL_FRAME_TYPE_SETPOINT
#endif

typedef struct {
    bool    valid;
    int16_t target_mm; /* 整 mm，相对 O */
} ball_setpoint_cmd_t;

void VisionUart_Init(void);

/** 主循环调用：抽 RX FIFO → 状态机；有完整帧则缓存 */
void VisionUart_Poll(void);

/** SysTick 1ms 调用：帧超时计时 */
void VisionUart_OnMsTick(void);

bool VisionUart_TakeBallFrame(ball_frame_t *out);
bool VisionUart_TakeSetpoint(ball_setpoint_cmd_t *out);

/** 距上一帧有效球位的 ms；无帧过则为较大值 */
uint32_t VisionUart_MsSinceBall(void);
bool     VisionUart_BallLinkOk(void);

uint32_t VisionUart_GetBallFrameCount(void);
uint32_t VisionUart_GetDropCount(void);

#endif /* VISION_UART_H */
