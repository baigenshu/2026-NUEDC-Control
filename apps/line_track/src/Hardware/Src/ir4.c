/**
 * @file ir4.c
 */
#include "ir4.h"
#include "ti_msp_dl_config.h"

#ifndef IR4_ACTIVE_LOW
#define IR4_ACTIVE_LOW  (0)
#endif

typedef struct {
    GPIO_Regs *port;
    uint32_t   pin;
} ir_pin_t;

static const ir_pin_t s_pins[IR4_CH_COUNT] = {
    { GPIO_IR_P1_PORT, GPIO_IR_P1_PIN },
    { GPIO_IR_P2_PORT, GPIO_IR_P2_PIN },
    { GPIO_IR_P3_PORT, GPIO_IR_P3_PIN },
    { GPIO_IR_P4_PORT, GPIO_IR_P4_PIN },
};

void Ir4_Init(void)
{
    /* 引脚已在 SysConfig 配置为上拉输入 */
}

void Ir4_ReadRaw(uint8_t s[IR4_CH_COUNT])
{
    uint8_t i;

    if (!s)
        return;
    for (i = 0; i < IR4_CH_COUNT; ++i) {
        uint32_t raw = DL_GPIO_readPins(s_pins[i].port, s_pins[i].pin);
#if IR4_ACTIVE_LOW
        s[i] = (raw == 0u) ? 1u : 0u;
#else
        s[i] = (raw != 0u) ? 1u : 0u;
#endif
    }
}

uint8_t Ir4_ReadMask(void)
{
    uint8_t raw[IR4_CH_COUNT];
    uint8_t mask = 0;
    uint8_t i;

    Ir4_ReadRaw(raw);
    for (i = 0; i < IR4_CH_COUNT; ++i) {
        if (raw[i])
            mask |= (uint8_t)(1u << i);
    }
    return mask;
}
