#include "ti_msp_dl_config.h"
#include "motor.h"
#include "bsp_systick.h"
#include "encoder.h"
#include "line_track.h"
#include "uart_debug.h"

#define CONTROL_MS   10
#define LOG_EVERY_N  5

static void System_Init(void)
{
    SYSCFG_DL_init();
    UartDebug_Init();
    Motor_Init();
    Encoder_Init();
    LineTrack_Init();

    UartDebug_Write("\r\n[car] line track ready\r\n");
    delay_ms(500);
}

int main(void)
{
    uint32_t n = 0;
    LineTrack_Status_t st;

    System_Init();

    while (1) {
        LineTrack_Step(&st);

        if ((n % LOG_EVERY_N) == 0u) {
            long spL = (long)EncoderB_Speed;
            long spR = (long)EncoderA_Speed;
            if (spL < 0) spL = -spL;
            if (spR < 0) spR = -spR;

            UartDebug_Printf(
                "L=%d R=%d | S=%02X E=%d side=%d lost=%u hard=%u | spd %ld/%ld\r\n",
                (int)st.left_cmd, (int)st.right_cmd,
                st.sensors, (int)st.error, (int)st.last_side,
                (unsigned)st.lost, (unsigned)st.hard_corner,
                spL, spR);
        }

        n++;
        delay_ms(CONTROL_MS);
    }
}
