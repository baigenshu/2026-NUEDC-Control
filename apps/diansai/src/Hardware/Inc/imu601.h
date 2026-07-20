#ifndef __IMU601_H
#define __IMU601_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/*
 * 汇电籽-601 IMU 模块驱动
 *
 * 接线:
 *   汇电籽-601    MSPM0G3507
 *   V            5V
 *   G            GND
 *   T (TX)       PA9  (UART1 RX)
 *   R (RX)       PA8  (UART1 TX)
 *
 * SysConfig 配置:
 *   添加 UART → 命名为 "IMU601" → 选择 UART1
 *   TX → PA8, RX → PA9, Baud=115200, 8N1
 *   中断: RX (每收到 1 字节触发), 在 SysConfig (empty.syscfg) 中已使能。
 *
 * 协议 (姿态帧, 固定 12 字节):
 *   0xAA 0x55 | ID(1) CMD(1) Len(1) | Data(6) | Checksum(1)
 *   Data: yaw(u16, 小端, ×100) pitch(s16, 小端, ×100) roll(s16, 小端, ×100)
 *   Checksum = (ID + CMD + Len + Data[...]) & 0xFF
 */

/* ---- 姿态数据结构 ---- */
typedef struct {
    float yaw;    /* 偏航角 (°) */
    float pitch;  /* 俯仰角 (°) */
    float roll;   /* 横滚角 (°) */
} IMU601_Attitude_t;

/* 全局姿态 (ISR 中更新, 应用层只读) */
extern volatile IMU601_Attitude_t IMU601_Attitude;

/* ---- API ---- */

/* 初始化: 发送复位 + 校准命令, 切换为 RX 中断, 使能接收 */
void IMU601_Init(void);

/* 设置输出频率 (Hz) */
void IMU601_SetRate(uint16_t freq_hz);

/* 解除波特率锁定 (模块波特率自动调整时需要) */
void IMU601_UnlockBaud(void);

/* 发送原始命令 (底层, 供扩展使用) */
void IMU601_SendCmd(uint8_t id, uint8_t cmd, const uint8_t *data, uint8_t len);

#endif /* __IMU601_H */
