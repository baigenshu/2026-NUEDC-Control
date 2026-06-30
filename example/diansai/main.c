#include "ti_msp_dl_config.h"
#include "board.h"

int main(void)
{
    int16_t s;

    SYSCFG_DL_init();
    OLED_Init();
    OLED_Clear();
    Motor_Init();
    Encoder_Init();

    OLED_ShowString(1, 1, "Encoder Test");

    while (1)
    {
        /* ---- Forward test: both motors ---- */
        OLED_ShowString(2, 1, "A FWD  B FWD");
        for (s = 20; s <= 60; s += 10)
        {
            MotorA_SetSpeed(s);
            MotorB_SetSpeed(s);
            Encoder_UpdateSpeed();
            OLED_ShowString(3, 1, "CA:");
            OLED_ShowNum(3, 4, (uint32_t)EncoderA_Count, 6);
            OLED_ShowString(4, 1, "CB:");
            OLED_ShowNum(4, 4, (uint32_t)EncoderB_Count, 6);
            delay_ms(800);
        }
        MotorA_Brake(); MotorB_Brake();
        delay_ms(1500);

        /* ---- Reverse test ---- */
        OLED_ShowString(2, 1, "A REV  B REV");
        for (s = 20; s <= 60; s += 10)
        {
            MotorA_SetSpeed(-s);
            MotorB_SetSpeed(-s);
            Encoder_UpdateSpeed();
            OLED_ShowString(3, 1, "CA:");
            OLED_ShowSignedNum(3, 4, (int32_t)EncoderA_Count, 6);
            OLED_ShowString(4, 1, "CB:");
            OLED_ShowSignedNum(4, 4, (int32_t)EncoderB_Count, 6);
            delay_ms(800);
        }
        MotorA_Brake(); MotorB_Brake();
        delay_ms(1500);

        /* ---- Reset and loop ---- */
        EncoderA_Reset();
        EncoderB_Reset();
    }
}
