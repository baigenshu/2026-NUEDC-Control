#include "encoder.h"
#include "ti_msp_dl_config.h"

volatile int32_t EncoderA_Count = 0;
volatile int32_t EncoderB_Count = 0;

static uint16_t s_prevA;
static uint8_t s_a_inited;

void GROUP1_IRQHandler(void)
{
    uint32_t status = DL_GPIO_getEnabledInterruptStatus(GPIOB,
        GPIO_ENCODERB_E2A_PIN | GPIO_ENCODERB_E2B_PIN);

    if (status & GPIO_ENCODERB_E2A_PIN) {
        if (DL_GPIO_readPins(GPIO_ENCODERB_PORT, GPIO_ENCODERB_E2B_PIN))
            EncoderB_Count++;
        else
            EncoderB_Count--;
    }
    if (status & GPIO_ENCODERB_E2B_PIN) {
        if (DL_GPIO_readPins(GPIO_ENCODERB_PORT, GPIO_ENCODERB_E2A_PIN))
            EncoderB_Count--;
        else
            EncoderB_Count++;
    }
    DL_GPIO_clearInterruptStatus(GPIOB, status);
}

void Encoder_Init(void)
{
    s_prevA = 0;
    s_a_inited = 0;
    EncoderA_Count = 0;
    EncoderB_Count = 0;

    DL_TimerG_setTimerCount(QEI_A_INST, 0);
    DL_TimerG_startCounter(QEI_A_INST);

    NVIC_EnableIRQ(GPIO_ENCODERB_INT_IRQN);
}

void Encoder_Sample(void)
{
    uint16_t nowA = (uint16_t)DL_TimerG_getTimerCount(QEI_A_INST);
    int16_t dA;

    if (!s_a_inited) {
        s_prevA = nowA;
        s_a_inited = 1;
        return;
    }
    dA = (int16_t)(nowA - s_prevA);
    s_prevA = nowA;
    EncoderA_Count += (int32_t)dA;
}

void EncoderA_Reset(void)
{
    EncoderA_Count = 0;
    s_prevA = (uint16_t)DL_TimerG_getTimerCount(QEI_A_INST);
}

void EncoderB_Reset(void)
{
    EncoderB_Count = 0;
}
