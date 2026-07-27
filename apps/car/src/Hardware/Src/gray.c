/**
 * @file gray.c
 * @brief 8 路灰度：mask + 加权位置
 */
#include "gray.h"
#include "chassis_cfg.h"
#include "ti_msp_dl_config.h"

typedef struct {
    GPIO_Regs *port;
    uint32_t   pin;
} gray_pin_t;

static const gray_pin_t s_pins[8] = {
    { GPIO_GRAY_PIN_0_PORT, GPIO_GRAY_PIN_0_PIN },
    { GPIO_GRAY_PIN_1_PORT, GPIO_GRAY_PIN_1_PIN },
    { GPIO_GRAY_PIN_2_PORT, GPIO_GRAY_PIN_2_PIN },
    { GPIO_GRAY_PIN_3_PORT, GPIO_GRAY_PIN_3_PIN },
    { GPIO_GRAY_PIN_4_PORT, GPIO_GRAY_PIN_4_PIN },
    { GPIO_GRAY_PIN_5_PORT, GPIO_GRAY_PIN_5_PIN },
    { GPIO_GRAY_PIN_6_PORT, GPIO_GRAY_PIN_6_PIN },
    { GPIO_GRAY_PIN_7_PORT, GPIO_GRAY_PIN_7_PIN },
};

static const int16_t s_weight[8] = {
    (int16_t)GRAY_WEIGHT_0,
    (int16_t)GRAY_WEIGHT_1,
    (int16_t)GRAY_WEIGHT_2,
    (int16_t)GRAY_WEIGHT_3,
    (int16_t)GRAY_WEIGHT_4,
    (int16_t)GRAY_WEIGHT_5,
    (int16_t)GRAY_WEIGHT_6,
    (int16_t)GRAY_WEIGHT_7,
};

void Gray_Init(void)
{
    /* 引脚已在 SysConfig 中配置为上拉输入 */
}

uint8_t Gray_ReadMask(void)
{
    uint8_t mask = 0;
    uint8_t i;

    for (i = 0; i < 8u; ++i) {
        /* 上拉：黑线接地 → 读到 0 → 软件视黑线为 1 */
        if (DL_GPIO_readPins(s_pins[i].port, s_pins[i].pin) == 0u)
            mask |= (uint8_t)(1u << i);
    }
    return mask;
}

int32_t Gray_GetPosition(void)
{
    uint8_t mask = Gray_ReadMask();
    int32_t sum  = 0;
    int32_t n    = 0;
    uint8_t i;

    for (i = 0; i < 8u; ++i) {
        if (mask & (uint8_t)(1u << i)) {
            sum += (int32_t)s_weight[i];
            n++;
        }
    }
    if (n == 0)
        return 0;
    return sum / n;
}
