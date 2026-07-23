#include "ti_msp_dl_config.h"
#include "board.h"

/*
 * 完整里程计联调 — MSPM0G3507 LQFP-48
 *  EncA PB6/7  EncB PB15/16  IMU PA8/9  DEBUG PA10 TX @115200
 *
 * 上电: IMU 复位+校准(静止) → 等姿态帧 → Odom 零点 → 100Hz 融合
 * 串口: F / Y(0.01°) / Xmm Ymm / Td(0.1°) / EA EB
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
    uint32_t t_print;
    uint32_t t0;

    SYSCFG_DL_init();
    Timebase_Init();
    UartDebug_Init();
    Encoder_Init();

    UartDebug_Write("\r\n[odom] IMU reset+calibrate, keep STILL...\r\n");
    IMU601_Init();

    t0 = millis();
    while (!IMU601_DataReady() && (millis() - t0) < 3000u)
        IMU601_Poll();

    if (IMU601_DataReady())
        UartDebug_Write("[odom] IMU ready\r\n");
    else
        UartDebug_Write("[odom] IMU timeout (check PA8/PA9)\r\n");

    Odom_Init();
    OdomProto_Init();

    NVIC_EnableIRQ(ODOM_TIM_INST_INT_IRQN);
    DL_TimerG_startCounter(ODOM_TIM_INST);

    UartDebug_Write("[odom] running @100Hz\r\n");
    t_print = millis();

    while (1) {
        const OdomState_t *st;
        uint32_t now;
        int32_t xi, yi, ti, y100;

        IMU601_Poll();

        if (s_odom_tick) {
            s_odom_tick = 0;
            Encoder_Sample();
            Odom_Update(0.01f);
        }

        now = millis();
        if ((now - t_print) >= 200u) {
            st = Odom_GetState();
            y100 = (int32_t)(IMU601_Attitude.yaw * 100.0f);
            xi = (int32_t)(st->x * 1000.0f);
            yi = (int32_t)(st->y * 1000.0f);
            ti = (int32_t)(st->theta * 572.958f);

            UartDebug_Printf(
                "F=%lu Y=%ld Xmm=%ld Ymm=%ld Td=%ld EA=%ld EB=%ld\r\n",
                (unsigned long)IMU601_FrameCount,
                (long)y100, (long)xi, (long)yi, (long)ti,
                (long)EncoderA_Count, (long)EncoderB_Count);
            t_print = now;
        }
    }
}
