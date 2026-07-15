#include "ti_msp_dl_config.h"
#include "board.h"

/*
 * Gimbal dual-stepper smoke test
 *
 * 接线 (板子已引出脚):
 *   S1: PA0(STEP) PA1(DIR) PA7(DCY) PA8(SLP) PA9(RST)
 *   S2: PA15(STEP) PA13(DIR) PA14(DCY) PA12(SLP) PA16(RST)
 *
 * 驱动芯片: DCC-100v3, 6400 pulse/rev, 0.05625 deg/pulse
 * 循环: 两路同向 90° → 反向 90° → 差速 180°
 */
int main(void)
{
    SYSCFG_DL_init();
    Stepper_Init();

    while (1)
    {
        /* Phase 0: both CW 90° @ 30 deg/s */
        Stepper_SetDir(STEPPER_1, DIR_CW);
        Stepper_SetDir(STEPPER_2, DIR_CW);
        Stepper_SetSpeed(STEPPER_1, 30);
        Stepper_SetSpeed(STEPPER_2, 30);
        Stepper_SetAngle(STEPPER_1, 90);
        Stepper_SetAngle(STEPPER_2, 90);
        while (!Stepper_IsDone(STEPPER_1) || !Stepper_IsDone(STEPPER_2))
            ;
        delay_ms(2000);

        /* Phase 1: both CCW 90° @ 30 deg/s */
        Stepper_SetDir(STEPPER_1, DIR_CCW);
        Stepper_SetDir(STEPPER_2, DIR_CCW);
        Stepper_SetAngle(STEPPER_1, 90);
        Stepper_SetAngle(STEPPER_2, 90);
        while (!Stepper_IsDone(STEPPER_1) || !Stepper_IsDone(STEPPER_2))
            ;
        delay_ms(2000);

        /* Phase 2: differential S1 CW / S2 CCW 180° @ 60 deg/s */
        Stepper_SetDir(STEPPER_1, DIR_CW);
        Stepper_SetDir(STEPPER_2, DIR_CCW);
        Stepper_SetSpeed(STEPPER_1, 60);
        Stepper_SetSpeed(STEPPER_2, 60);
        Stepper_SetAngle(STEPPER_1, 180);
        Stepper_SetAngle(STEPPER_2, 180);
        while (!Stepper_IsDone(STEPPER_1) || !Stepper_IsDone(STEPPER_2))
            ;
        delay_ms(2000);
    }
}
