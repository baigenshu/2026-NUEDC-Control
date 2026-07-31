/**
 * @file key.c
 */
#include "key.h"
#include "ti_msp_dl_config.h"

#ifndef KEY_DEBOUNCE_MS
#define KEY_DEBOUNCE_MS 20u
#endif

typedef struct {
    uint32_t pin;
    uint8_t  stable;
    uint8_t  raw_last;
    uint32_t edge_ms;
} key_ch_t;

static key_ch_t s_ch[KEY_ID_COUNT];

void Key_Init(void)
{
    key_id_t i;

    s_ch[KEY_ID_RUN].pin = GPIO_KEY_RUN_PIN;
    s_ch[KEY_ID_SPD].pin = GPIO_KEY_SPD_PIN;

    for (i = KEY_ID_RUN; i < KEY_ID_COUNT; ++i) {
        s_ch[i].stable   = 0;
        s_ch[i].raw_last = Key_IsDown(i) ? 1u : 0u;
        s_ch[i].edge_ms  = 0;
        s_ch[i].stable   = s_ch[i].raw_last;
    }
}

bool Key_IsDown(key_id_t id)
{
    if ((unsigned)id >= KEY_ID_COUNT)
        return false;
    return DL_GPIO_readPins(GPIO_KEY_PORT, s_ch[id].pin) == 0u;
}

bool Key_PollPress(key_id_t id, uint32_t now_ms)
{
    key_ch_t *k;
    uint8_t raw;

    if ((unsigned)id >= KEY_ID_COUNT)
        return false;

    k = &s_ch[id];
    raw = Key_IsDown(id) ? 1u : 0u;

    if (raw != k->raw_last) {
        k->raw_last = raw;
        k->edge_ms = now_ms;
        return false;
    }
    if ((now_ms - k->edge_ms) < KEY_DEBOUNCE_MS)
        return false;
    if (raw != k->stable) {
        k->stable = raw;
        if (k->stable)
            return true;
    }
    return false;
}
