#include "encoder.h"
//#include "mpu6050.h"
#include "bsp_systick.h"

/* Pulse counts — updated by ISR, read by application
 * (MPU6050 INT / PB1 removed after switching to IMU601 over UART) */
volatile int32_t EncoderA_Count = 0;
volatile int32_t EncoderB_Count = 0;

/* Speed (pulses/sec) — updated by Encoder_UpdateSpeed() */
volatile float EncoderA_Speed = 0.0f;
volatile float EncoderB_Speed = 0.0f;

/* Previous counts for speed calculation */
static int32_t prev_countA = 0;
static int32_t prev_countB = 0;
static uint32_t prev_tick = 0;

/*
 * GROUP1_IRQHandler — handles all GPIOB interrupts:
 *   PB0  (EncoderA PhaseA) → quadrature decode
 *   PB5  (EncoderA PhaseB)
 *   PB12 (EncoderB PhaseA)
 *   PB18 (EncoderB PhaseB)
 */
void GROUP1_IRQHandler(void)
{
    uint32_t status;

    status = DL_GPIO_getEnabledInterruptStatus(GPIOB,
        GPIO_ENCODERA_E1A_PIN | GPIO_ENCODERA_E1B_PIN |
        GPIO_ENCODERB_E2A_PIN | GPIO_ENCODERB_E2B_PIN);

    /* --- Encoder A Phase A (PB0, both edges) --- */
    if (status & GPIO_ENCODERA_E1A_PIN)
    {
        if (DL_GPIO_readPins(GPIO_ENCODERA_PORT, GPIO_ENCODERA_E1B_PIN))
            EncoderA_Count++;
        else
            EncoderA_Count--;
    }

    /* --- Encoder A Phase B (PB5, both edges) --- */
    if (status & GPIO_ENCODERA_E1B_PIN)
    {
        if (DL_GPIO_readPins(GPIO_ENCODERA_PORT, GPIO_ENCODERA_E1A_PIN))
            EncoderA_Count--;
        else
            EncoderA_Count++;
    }

    /* --- Encoder B Phase A (PB12, both edges) --- */
    if (status & GPIO_ENCODERB_E2A_PIN)
    {
        if (DL_GPIO_readPins(GPIO_ENCODERB_PORT, GPIO_ENCODERB_E2B_PIN))
            EncoderB_Count++;
        else
            EncoderB_Count--;
    }

    /* --- Encoder B Phase B (PB18, both edges) --- */
    if (status & GPIO_ENCODERB_E2B_PIN)
    {
        if (DL_GPIO_readPins(GPIO_ENCODERB_PORT, GPIO_ENCODERB_E2A_PIN))
            EncoderB_Count--;
        else
            EncoderB_Count++;
    }

    DL_GPIO_clearInterruptStatus(GPIOB, status);
}

/*
 * Initialize encoder subsystem.
 * GPIO init is done by SYSCFG_DL_GPIO_init(); this just resets counters.
 */
void Encoder_Init(void)
{
    EncoderA_Count  = 0;
    EncoderB_Count  = 0;
    EncoderA_Speed  = 0.0f;
    EncoderB_Speed  = 0.0f;
    prev_countA     = 0;
    prev_countB     = 0;
    prev_tick       = Systick_getTick();

    /* NVIC enable — GPIO init already sets pin-level interrupt,
     * but the Cortex-M0+ NVIC must be explicitly enabled. */
    NVIC_EnableIRQ(GPIOB_INT_IRQn);
}

/*
 * Call periodically (~every 10ms) to compute speed from pulse counts.
 */
void Encoder_UpdateSpeed(void)
{
    uint32_t now = Systick_getTick();
    float dt;

    if (prev_tick == 0)
    {
        prev_tick = now;
        return;
    }

    dt = (float)(now - prev_tick) / 1000.0f;  /* seconds */
    if (dt <= 0.0f || dt > 1.0f)
    {
        prev_countA = EncoderA_Count;
        prev_countB = EncoderB_Count;
        prev_tick   = now;
        return;
    }

    EncoderA_Speed = (float)(EncoderA_Count - prev_countA) / dt;
    EncoderB_Speed = (float)(EncoderB_Count - prev_countB) / dt;

    prev_countA = EncoderA_Count;
    prev_countB = EncoderB_Count;
    prev_tick   = now;
}

void EncoderA_Reset(void) { EncoderA_Count = 0; EncoderA_Speed = 0.0f; }
void EncoderB_Reset(void) { EncoderB_Count = 0; EncoderB_Speed = 0.0f; }
