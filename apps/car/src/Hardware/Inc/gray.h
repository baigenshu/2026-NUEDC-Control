/**
 * @file gray.h
 * @brief 8 路灰度传感器
 *
 * bit0=G1 … bit7=G8；黑线=1、浅色=0。
 * 位置 = 各 bit 与 GRAY_WEIGHT_* 加权和（权重在 chassis_cfg.h）。
 */
#ifndef GRAY_H
#define GRAY_H

#include <stdint.h>

void Gray_Init(void);

/** 返回 8bit mask：bit0=G1 … bit7=G8 */
uint8_t Gray_ReadMask(void);

/**
 * 加权位置；无传感器触发时返回 0。
 * 量纲与 GRAY_WEIGHT_* 一致（约 -3500..+3500）。
 */
int32_t Gray_GetPosition(void);

#endif /* GRAY_H */
