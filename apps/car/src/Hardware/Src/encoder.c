#include "encoder.h"
#include "ti_msp_dl_config.h"
#include "bsp_systick.h"

/*
 * 四路 GPIO x4 正交（gray-code 查表）
 *   EncA: PB0  / PB5
 *   EncB: PB23 / PB18
 *   EncC: PB27 / PB22
 *   EncD: PB24 / PB26
 *
 * 全四倍频：每个合法 A/B 沿计 ±1
 */

volatile int32_t EncoderA_Count = 0;
volatile int32_t EncoderB_Count = 0;
volatile int32_t EncoderC_Count = 0;
volatile int32_t EncoderD_Count = 0;
volatile float EncoderA_Speed = 0.0f;
volatile float EncoderB_Speed = 0.0f;
volatile float EncoderC_Speed = 0.0f;
volatile float EncoderD_Speed = 0.0f;

/* last state: bit1=phaseA, bit0=phaseB */
static uint8_t s_prevA;
static uint8_t s_prevB;
static uint8_t s_prevC;
static uint8_t s_prevD;

static int32_t s_prevCountA;
static int32_t s_prevCountB;
static int32_t s_prevCountC;
static int32_t s_prevCountD;
static uint32_t s_prevTick;

/*
 * Index = (old<<2) | new ; value = delta
 * 仅合法 gray 步进为 ±1，噪声/非法 → 0
 */
static const int8_t s_qem[16] = {
    0,  +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

#define ENC_ALL_PINS \
    (GPIO_ENCODERA_E1A_PIN | GPIO_ENCODERA_E1B_PIN | \
     GPIO_ENCODERB_E2A_PIN | GPIO_ENCODERB_E2B_PIN | \
     GPIO_ENCODERC_E3A_PIN | GPIO_ENCODERC_E3B_PIN | \
     GPIO_ENCODERD_E4A_PIN | GPIO_ENCODERD_E4B_PIN)

static uint8_t read_ab(GPIO_Regs *port, uint32_t pinA, uint32_t pinB)
{
    uint8_t a = DL_GPIO_readPins(port, pinA) ? 1u : 0u;
    uint8_t b = DL_GPIO_readPins(port, pinB) ? 1u : 0u;
    return (uint8_t)((a << 1) | b);
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

    if (stA != 0u) {
        uint8_t now = read_ab(GPIO_ENCODERA_PORT,
            GPIO_ENCODERA_E1A_PIN, GPIO_ENCODERA_E1B_PIN);
        EncoderA_Count += s_qem[(s_prevA << 2) | now];
        s_prevA = now;
        DL_GPIO_clearInterruptStatus(GPIO_ENCODERA_PORT, stA);
    }

    if (stB != 0u) {
        uint8_t now = read_ab(GPIO_ENCODERB_PORT,
            GPIO_ENCODERB_E2A_PIN, GPIO_ENCODERB_E2B_PIN);
        EncoderB_Count += s_qem[(s_prevB << 2) | now];
        s_prevB = now;
        DL_GPIO_clearInterruptStatus(GPIO_ENCODERB_PORT, stB);
    }

    if (stC != 0u) {
        uint8_t now = read_ab(GPIO_ENCODERC_PORT,
            GPIO_ENCODERC_E3A_PIN, GPIO_ENCODERC_E3B_PIN);
        EncoderC_Count += s_qem[(s_prevC << 2) | now];
        s_prevC = now;
        DL_GPIO_clearInterruptStatus(GPIO_ENCODERC_PORT, stC);
    }

    if (stD != 0u) {
        uint8_t now = read_ab(GPIO_ENCODERD_PORT,
            GPIO_ENCODERD_E4A_PIN, GPIO_ENCODERD_E4B_PIN);
        EncoderD_Count += s_qem[(s_prevD << 2) | now];
        s_prevD = now;
        DL_GPIO_clearInterruptStatus(GPIO_ENCODERD_PORT, stD);
    }
}

void Encoder_Init(void)
{
    EncoderA_Count = 0;
    EncoderB_Count = 0;
    EncoderC_Count = 0;
    EncoderD_Count = 0;
    EncoderA_Speed = 0.0f;
    EncoderB_Speed = 0.0f;
    EncoderC_Speed = 0.0f;
    EncoderD_Speed = 0.0f;
    s_prevCountA   = 0;
    s_prevCountB   = 0;
    s_prevCountC   = 0;
    s_prevCountD   = 0;
    s_prevTick     = Systick_getTick();

    s_prevA = read_ab(GPIO_ENCODERA_PORT,
        GPIO_ENCODERA_E1A_PIN, GPIO_ENCODERA_E1B_PIN);
    s_prevB = read_ab(GPIO_ENCODERB_PORT,
        GPIO_ENCODERB_E2A_PIN, GPIO_ENCODERB_E2B_PIN);
    s_prevC = read_ab(GPIO_ENCODERC_PORT,
        GPIO_ENCODERC_E3A_PIN, GPIO_ENCODERC_E3B_PIN);
    s_prevD = read_ab(GPIO_ENCODERD_PORT,
        GPIO_ENCODERD_E4A_PIN, GPIO_ENCODERD_E4B_PIN);

    DL_GPIO_clearInterruptStatus(GPIOB, ENC_ALL_PINS);

    NVIC_ClearPendingIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
    NVIC_EnableIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
}

void Encoder_Sample(void)
{
}

/*
 * 约 10~100ms 调用一次。dt 来自 SysTick VAL（须 < ~209ms）
 */
void Encoder_UpdateSpeed(void)
{
    const uint32_t period = SysTickMAX_COUNT + 1U;
    uint32_t now = Systick_getTick();
    uint32_t elapsed_cyc;
    float dt;

    if (s_prevTick >= now)
        elapsed_cyc = s_prevTick - now;
    else
        elapsed_cyc = s_prevTick + period - now;

    dt = (float)elapsed_cyc / (float)SysTickFre;
    if (dt < 0.001f || dt > 0.25f) {
        s_prevCountA = EncoderA_Count;
        s_prevCountB = EncoderB_Count;
        s_prevCountC = EncoderC_Count;
        s_prevCountD = EncoderD_Count;
        s_prevTick   = now;
        return;
    }

    EncoderA_Speed = (float)(EncoderA_Count - s_prevCountA) / dt;
    EncoderB_Speed = (float)(EncoderB_Count - s_prevCountB) / dt;
    EncoderC_Speed = (float)(EncoderC_Count - s_prevCountC) / dt;
    EncoderD_Speed = (float)(EncoderD_Count - s_prevCountD) / dt;

    s_prevCountA = EncoderA_Count;
    s_prevCountB = EncoderB_Count;
    s_prevCountC = EncoderC_Count;
    s_prevCountD = EncoderD_Count;
    s_prevTick   = now;
}

void EncoderA_Reset(void)
{
    EncoderA_Count = 0;
    EncoderA_Speed = 0.0f;
    s_prevCountA   = 0;
    s_prevA = read_ab(GPIO_ENCODERA_PORT,
        GPIO_ENCODERA_E1A_PIN, GPIO_ENCODERA_E1B_PIN);
    s_prevTick = Systick_getTick();
}

void EncoderB_Reset(void)
{
    EncoderB_Count = 0;
    EncoderB_Speed = 0.0f;
    s_prevCountB   = 0;
    s_prevB = read_ab(GPIO_ENCODERB_PORT,
        GPIO_ENCODERB_E2A_PIN, GPIO_ENCODERB_E2B_PIN);
    s_prevTick = Systick_getTick();
}

void EncoderC_Reset(void)
{
    EncoderC_Count = 0;
    EncoderC_Speed = 0.0f;
    s_prevCountC   = 0;
    s_prevC = read_ab(GPIO_ENCODERC_PORT,
        GPIO_ENCODERC_E3A_PIN, GPIO_ENCODERC_E3B_PIN);
    s_prevTick = Systick_getTick();
}

void EncoderD_Reset(void)
{
    EncoderD_Count = 0;
    EncoderD_Speed = 0.0f;
    s_prevCountD   = 0;
    s_prevD = read_ab(GPIO_ENCODERD_PORT,
        GPIO_ENCODERD_E4A_PIN, GPIO_ENCODERD_E4B_PIN);
    s_prevTick = Systick_getTick();
}

void Encoder_AllReset(void)
{
    EncoderA_Reset();
    EncoderB_Reset();
    EncoderC_Reset();
    EncoderD_Reset();
}

int32_t Encoder_GetCount(uint8_t id)
{
    switch (id) {
    case 0: return EncoderA_Count;
    case 1: return EncoderB_Count;
    case 2: return EncoderC_Count;
    case 3: return EncoderD_Count;
    default: return 0;
    }
}
