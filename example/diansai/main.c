#include "ti_msp_dl_config.h"
#include "board.h"

int main(void)
{
    uint8_t phase = 0;

    SYSCFG_DL_init();
    OLED_Init();
    OLED_Clear();
    Stepper_Init();

    OLED_ShowString(1, 1, "Stepper PWM Test");

    while (1)
    {
        /* Phase 0: S1 CW 90deg @ 30deg/s, S2 CW 90deg @ 30deg/s */
        OLED_ShowString(2, 1, "Both CW 90@30");
        Stepper_SetDir(STEPPER_1, DIR_CW);
        Stepper_SetDir(STEPPER_2, DIR_CW);
        Stepper_SetSpeed(STEPPER_1, 30);
        Stepper_SetSpeed(STEPPER_2, 30);
        Stepper_SetAngle(STEPPER_1, 90);
        Stepper_SetAngle(STEPPER_2, 90);
        OLED_ShowString(4, 1, "Running...");
        while (!Stepper_IsDone(STEPPER_1) || !Stepper_IsDone(STEPPER_2));
        OLED_ShowString(4, 1, "Done. Wait...");
        delay_ms(2000);

        /* Phase 1: Both CCW 90deg */
        OLED_ShowString(2, 1, "Both CCW 90@30");
        Stepper_SetDir(STEPPER_1, DIR_CCW);
        Stepper_SetDir(STEPPER_2, DIR_CCW);
        Stepper_SetAngle(STEPPER_1, 90);
        Stepper_SetAngle(STEPPER_2, 90);
        OLED_ShowString(4, 1, "Running...");
        while (!Stepper_IsDone(STEPPER_1) || !Stepper_IsDone(STEPPER_2));
        OLED_ShowString(4, 1, "Done. Wait...");
        delay_ms(2000);

        /* Phase 2: Differential (S1 CW, S2 CCW) at faster speed */
        OLED_ShowString(2, 1, "Diff S1> S2< 60");
        Stepper_SetDir(STEPPER_1, DIR_CW);
        Stepper_SetDir(STEPPER_2, DIR_CCW);
        Stepper_SetSpeed(STEPPER_1, 60);
        Stepper_SetSpeed(STEPPER_2, 60);
        Stepper_SetAngle(STEPPER_1, 180);
        Stepper_SetAngle(STEPPER_2, 180);
        OLED_ShowString(4, 1, "Running...");
        while (!Stepper_IsDone(STEPPER_1) || !Stepper_IsDone(STEPPER_2));
        OLED_ShowString(4, 1, "Done. Wait...");
        delay_ms(2000);
    }
}
