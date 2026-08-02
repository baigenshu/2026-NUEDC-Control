/**
 * @file key.h
 * @brief 外接按键（低有效 · 内部上拉 · 软件消抖）
 *
 * KEY_RUN PA17 · KEY_SPD PA15（勿用 PA18，BSL 调用脚影响烧录）
 */
#ifndef KEY_H
#define KEY_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KEY_ID_RUN = 0,
    KEY_ID_SPD = 1,
    KEY_ID_COUNT
} key_id_t;

void Key_Init(void);

/** 当前是否按下（低有效，未消抖） */
bool Key_IsDown(key_id_t id);

/**
 * 轮询消抖后的按下边沿
 * @param now_ms  当前毫秒时基
 * @return true  本次检测到一次有效按下
 */
bool Key_PollPress(key_id_t id, uint32_t now_ms);

#endif /* KEY_H */
