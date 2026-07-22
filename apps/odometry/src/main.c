#include "ti_msp_dl_config.h"
#include "board.h"

/*
 * Odometry MCU (LQFP-48)
 *  - QEI encoders + IMU601 (DMA) → Odom @100Hz (TIMG0)
 *  - UART0 debug text @115200 (PA10 TX / PA11 RX)
 *  - Optional binary uplink: OdomProto on same UART0
 */

static volatile uint8_t s_odom_tick;

void ODOM_TIM_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(ODOM_TIM_INST)) {
    case DL_TIMER_IIDX_ZERO:
        s_odom_tick = 1;
        break;
    default:
        break;
    }
}

int main(void)
{
    uint32_t t_print = 0;

    SYSCFG_DL_init();
    Timebase_Init();
    UartDebug_Init();
    Encoder_Init();
    IMU601_Init();
    Odom_Init();
    OdomProto_Init();

    NVIC_EnableIRQ(ODOM_TIM_INST_INT_IRQN);
    DL_TimerG_startCounter(ODOM_TIM_INST);

    UartDebug_Write("\r\n[odom] LQFP-48 QEI+DMA ready\r\n");
    t_print = millis();

    while (1) {
        const OdomState_t *st;
        uint32_t now;

        IMU601_Poll();

        if (s_odom_tick) {
            s_odom_tick = 0;
            Encoder_Sample();
            Odom_Update(0.01f);
            /* binary uplink optional: OdomProto_SendState(Odom_GetState()); */
        }

        now = millis();
        if ((now - t_print) >= 200u) {
            st = Odom_GetState();
            UartDebug_Printf(
                "F=%lu Y=%.2f  X=%.3f Y=%.3f T=%.1f  EA=%ld EB=%ld\r\n",
                (unsigned long)IMU601_FrameCount,
                (double)IMU601_Attitude.yaw,
                (double)st->x,
                (double)st->y,
                (double)(st->theta * 57.2958f),
                (long)EncoderA_Count,
                (long)EncoderB_Count);
            t_print = now;
        }
    }
}
