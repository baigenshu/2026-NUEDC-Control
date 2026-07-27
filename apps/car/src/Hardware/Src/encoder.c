/**
 * @file encoder.c
 * @brief 四路 GPIO 正交编码器 · 四倍频查表
 *
 * GROUP1 ISR：只读相、查表、累加；禁止浮点/PID/Motor_Set。
 */
#include "encoder.h"
#include "chassis_cfg.h"
#include "ti_msp_dl_config.h"

/* 原始累计（ISR 写，Get 时乘 ENC_SIGN） */
static volatile int32_t s_raw[ENC_ID_COUNT];
/* 上一拍 AB 状态：bit1=A, bit0=B */
static uint8_t s_prev[ENC_ID_COUNT];

/*
 * 索引 = (old<<2)|new；合法格雷码步进 ±1，非法 0
 */
static const int8_t s_qem[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

static const int8_t s_sign[ENC_ID_COUNT] = {
    (int8_t)ENC_SIGN_A,
    (int8_t)ENC_SIGN_B,
    (int8_t)ENC_SIGN_C,
    (int8_t)ENC_SIGN_D,
};

typedef struct {
    GPIO_Regs *port;
    uint32_t   pin_a;
    uint32_t   pin_b;
} enc_pins_t;

static const enc_pins_t s_pins[ENC_ID_COUNT] = {
    { GPIO_ENCODERA_PORT, GPIO_ENCODERA_E1A_PIN, GPIO_ENCODERA_E1B_PIN },
    { GPIO_ENCODERB_PORT, GPIO_ENCODERB_E2A_PIN, GPIO_ENCODERB_E2B_PIN },
    { GPIO_ENCODERC_PORT, GPIO_ENCODERC_E3A_PIN, GPIO_ENCODERC_E3B_PIN },
    { GPIO_ENCODERD_PORT, GPIO_ENCODERD_E4A_PIN, GPIO_ENCODERD_E4B_PIN },
};

static uint8_t read_ab(const enc_pins_t *p)
{
    uint8_t a = DL_GPIO_readPins(p->port, p->pin_a) ? 1u : 0u;
    uint8_t b = DL_GPIO_readPins(p->port, p->pin_b) ? 1u : 0u;
    return (uint8_t)((a << 1) | b);
}

static void update_one(enc_id_t id, uint32_t st)
{
    uint8_t now;

    if (st == 0u)
        return;
    now = read_ab(&s_pins[id]);
    s_raw[id] += s_qem[(s_prev[id] << 2) | now];
    s_prev[id] = now;
    DL_GPIO_clearInterruptStatus(s_pins[id].port, st);
}

void GROUP1_IRQHandler(void)
{
    uint32_t stA = DL_GPIO_getEnabledInterruptStatus(
        GPIO_ENCODERA_PORT,
        GPIO_ENCODERA_E1A_PIN | GPIO_ENCODERA_E1B_PIN);
    uint32_t stB = DL_GPIO_getEnabledInterruptStatus(
        GPIO_ENCODERB_PORT,
        GPIO_ENCODERB_E2A_PIN | GPIO_ENCODERB_E2B_PIN);
    uint32_t stC = DL_GPIO_getEnabledInterruptStatus(
        GPIO_ENCODERC_PORT,
        GPIO_ENCODERC_E3A_PIN | GPIO_ENCODERC_E3B_PIN);
    uint32_t stD = DL_GPIO_getEnabledInterruptStatus(
        GPIO_ENCODERD_PORT,
        GPIO_ENCODERD_E4A_PIN | GPIO_ENCODERD_E4B_PIN);

    update_one(ENC_ID_A, stA);
    update_one(ENC_ID_B, stB);
    update_one(ENC_ID_C, stC);
    update_one(ENC_ID_D, stD);
}

void Encoder_Init(void)
{
    enc_id_t i;
    uint32_t all_pins;

    for (i = ENC_ID_A; i < ENC_ID_COUNT; ++i) {
        s_raw[i]  = 0;
        s_prev[i] = read_ab(&s_pins[i]);
    }

    all_pins =
        GPIO_ENCODERA_E1A_PIN | GPIO_ENCODERA_E1B_PIN |
        GPIO_ENCODERB_E2A_PIN | GPIO_ENCODERB_E2B_PIN |
        GPIO_ENCODERC_E3A_PIN | GPIO_ENCODERC_E3B_PIN |
        GPIO_ENCODERD_E4A_PIN | GPIO_ENCODERD_E4B_PIN;

    DL_GPIO_clearInterruptStatus(GPIOB, all_pins);
    NVIC_ClearPendingIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
    NVIC_EnableIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
}

int32_t Encoder_Get(enc_id_t id)
{
    int32_t raw;
    if ((unsigned)id >= ENC_ID_COUNT)
        return 0;
    raw = s_raw[id];
    return raw * (int32_t)s_sign[id];
}

void Encoder_GetAll(int32_t *a, int32_t *b, int32_t *c, int32_t *d)
{
    if (a) *a = Encoder_Get(ENC_ID_A);
    if (b) *b = Encoder_Get(ENC_ID_B);
    if (c) *c = Encoder_Get(ENC_ID_C);
    if (d) *d = Encoder_Get(ENC_ID_D);
}

void Encoder_Reset(enc_id_t id)
{
    if ((unsigned)id >= ENC_ID_COUNT)
        return;
    s_raw[id]  = 0;
    s_prev[id] = read_ab(&s_pins[id]);
}

void Encoder_ResetAll(void)
{
    enc_id_t i;
    for (i = ENC_ID_A; i < ENC_ID_COUNT; ++i)
        Encoder_Reset(i);
}
