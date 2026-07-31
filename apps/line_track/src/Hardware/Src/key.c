/**
 * @file key.c
 * @brief 低有效按键 · 连续采样消抖
 */
#include "key.h"
#include "ti_msp_dl_config.h"

#ifndef KEY_DEBOUNCE_MS
#define KEY_DEBOUNCE_MS 30u
#endif

typedef struct {
    uint32_t pin;
    uint8_t  stable;     /* 消抖后电平：1=按下 */
    uint8_t  cand;       /* 候选电平 */
    uint32_t cand_ms;    /* 候选开始时间 */
    uint8_t  pressed;    /* 边沿已消费标志，避免长按连发 */
} key_ch_t;

static key_ch_t s_ch[KEY_ID_COUNT];

void Key_Init(void)
{
    key_id_t i;
    uint8_t down;

    s_ch[KEY_ID_RUN].pin = GPIO_KEY_RUN_PIN;
    s_ch[KEY_ID_SPD].pin = GPIO_KEY_SPD_PIN;

    for (i = KEY_ID_RUN; i < KEY_ID_COUNT; ++i) {
        down = Key_IsDown(i) ? 1u : 0u;
        s_ch[i].stable  = down;
        s_ch[i].cand    = down;
        s_ch[i].cand_ms = 0;
        s_ch[i].pressed = down; /* 上电已按下则不立即触发 */
    }
}

bool Key_IsDown(key_id_t id)
{
    uint32_t v;

    if ((unsigned)id >= KEY_ID_COUNT)
        return false;
    /* DIN 中对应位为 0 → 按下（内部上拉 + 按键接 GND） */
    v = DL_GPIO_readPins(GPIO_KEY_PORT, s_ch[id].pin);
    return v == 0u;
}

bool Key_PollPress(key_id_t id, uint32_t now_ms)
{
    key_ch_t *k;
    uint8_t raw;

    if ((unsigned)id >= KEY_ID_COUNT)
        return false;

    k = &s_ch[id];
    raw = Key_IsDown(id) ? 1u : 0u;

    if (raw != k->cand) {
        k->cand = raw;
        k->cand_ms = now_ms;
        return false;
    }

    if ((now_ms - k->cand_ms) < KEY_DEBOUNCE_MS)
        return false;

    /* 候选已稳定 */
    if (raw != k->stable) {
        k->stable = raw;
        if (raw) {
            /* 按下边沿：仅当未处于 pressed 锁存 */
            if (!k->pressed) {
                k->pressed = 1u;
                return true;
            }
        } else {
            k->pressed = 0u; /* 松开，允许下次按下 */
        }
    }
    return false;
}
