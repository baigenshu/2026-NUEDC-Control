#include "encoder.h"
#include "ti_msp_dl_config.h"
#include "bsp_systick.h"

/*
 * Dual GPIO x4 quadrature (gray-code table) — same algorithm as apps/odometry
 *   EncA: PB0 / PB5
 *   EncB: PB23 / PB18
 *
 * Full 4x: every valid A/B edge counts ±1 (no else-if drop).
 */

volatile int32_t EncoderA_Count = 0;
volatile int32_t EncoderB_Count = 0;
volatile float EncoderA_Speed = 0.0f;
volatile float EncoderB_Speed = 0.0f;

/* last state: bit1=phaseA, bit0=phaseB */
static uint8_t s_prevA;
static uint8_t s_prevB;

/* speed: previous counts + SysTick VAL */
static int32_t s_prevCountA;
static int32_t s_prevCountB;
static uint32_t s_prevTick;

/*
 * Index = (old<<2) | new ; value = delta
 * Only legal gray-code steps are ±1; noise/illegal → 0
 */
static const int8_t s_qem[16] = {
    0,  +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

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
}

void Encoder_Init(void)
{
    EncoderA_Count = 0;
    EncoderB_Count = 0;
    EncoderA_Speed = 0.0f;
    EncoderB_Speed = 0.0f;
    s_prevCountA   = 0;
    s_prevCountB   = 0;
    s_prevTick     = Systick_getTick();

    s_prevA = read_ab(GPIO_ENCODERA_PORT,
        GPIO_ENCODERA_E1A_PIN, GPIO_ENCODERA_E1B_PIN);
    s_prevB = read_ab(GPIO_ENCODERB_PORT,
        GPIO_ENCODERB_E2A_PIN, GPIO_ENCODERB_E2B_PIN);

    DL_GPIO_clearInterruptStatus(GPIOB,
        GPIO_ENCODERA_E1A_PIN | GPIO_ENCODERA_E1B_PIN |
        GPIO_ENCODERB_E2A_PIN | GPIO_ENCODERB_E2B_PIN);

    NVIC_ClearPendingIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
    NVIC_EnableIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
}

void Encoder_Sample(void)
{
}

/*
 * Call every ~10~100ms. dt from SysTick VAL (must be < ~209ms).
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
        s_prevTick   = now;
        return;
    }

    EncoderA_Speed = (float)(EncoderA_Count - s_prevCountA) / dt;
    EncoderB_Speed = (float)(EncoderB_Count - s_prevCountB) / dt;

    s_prevCountA = EncoderA_Count;
    s_prevCountB = EncoderB_Count;
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
