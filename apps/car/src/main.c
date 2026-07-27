#include "ti_msp_dl_config.h"
#include "motor.h"
#include "encoder.h"
#include "chassis.h"
#include "motion.h"
#include "OLED.h"
#include "bsp_systick.h"

/* 前进指定距离 → 原路返回 */
#define BASE_PWM    18
#define STEP_MS     10
#define PAUSE_MS    800
#define GO_M        0.50f   /* 单程距离 m */

static void ui_show(const char *tag)
{
    const Motion_Status_t *m = Motion_GetStatus();

    OLED_ShowString(1, 1, tag);
    OLED_ShowString(2, 1, "Tm:");
    OLED_ShowSignedNum(2, 4, (int32_t)(m->target_m * 100.0f), 3);
    OLED_ShowString(2, 9, "Fm:");
    OLED_ShowSignedNum(2, 12, (int32_t)(m->feedback_m * 100.0f), 3);
    OLED_ShowString(3, 1, "P:");
    OLED_ShowNum(3, 3, (uint32_t)m->pulse_now, 4);
    OLED_ShowString(3, 8, "/");
    OLED_ShowNum(3, 9, (uint32_t)m->pulse_target, 4);
    OLED_ShowString(4, 1, "done:");
    OLED_ShowNum(4, 6, (uint32_t)m->done, 1);
}

int main(void)
{
    SYSCFG_DL_init();
    OLED_Init();
    OLED_Clear();

    Motor_Init();
    Encoder_Init();
    Chassis_Init();
    Motion_Init();
    Encoder_AllReset();

    OLED_ShowString(1, 1, "GoReturn");
    delay_ms(400);
    OLED_Clear();

    while (1) {
        /* 前进 */
        ui_show("Go Fwd   ");
        Motion_GoDistance_Wait(GO_M, BASE_PWM, STEP_MS);
        ui_show("Hold     ");
        delay_ms(PAUSE_MS);

        /* 原路返回（后退同距离） */
        ui_show("Go Back  ");
        Motion_GoDistance_Wait(-GO_M, BASE_PWM, STEP_MS);
        ui_show("Hold     ");
        delay_ms(PAUSE_MS);
    }
}
