#include "ti_msp_dl_config.h"
#include "motor.h"
#include "encoder.h"
#include "chassis.h"
#include "line_track.h"
#include "OLED.h"
#include "bsp_systick.h"

/*
 * 四轮循迹
 *   左前C 右前B | 左后D 右后A
 *   8 路灰度: S1(左) … S8(右)
 *
 * 周期 15ms 调用 LineTrack_Step
 */
#define CTRL_MS     15
#define BASE_SPD    15

static void ui_show(const LineTrack_Status_t *st)
{
    OLED_ShowString(1, 1, "Track");
    OLED_ShowString(1, 7, st->lost ? "LOST" : (st->hard_corner ? "CORN" : " OK "));

    OLED_ShowString(2, 1, "L:");
    OLED_ShowSignedNum(2, 3, st->left_cmd, 3);
    OLED_ShowString(2, 8, "R:");
    OLED_ShowSignedNum(2, 10, st->right_cmd, 3);

    OLED_ShowString(3, 1, "E:");
    OLED_ShowSignedNum(3, 3, st->error, 5);

    OLED_ShowString(4, 1, "S:");
    OLED_ShowHexNum(4, 3, st->sensors, 2);
}

int main(void)
{
    LineTrack_Status_t st;
    uint32_t ui_div = 0;

    SYSCFG_DL_init();
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(1, 1, "Line Track");
    delay_ms(500);

    Motor_Init();
    Encoder_Init();
    Chassis_Init();
    LineTrack_Init();
    LineTrack_SetBaseSpeed(BASE_SPD);
    Encoder_AllReset();
    OLED_Clear();

    while (1) {
        LineTrack_Step(&st);

        /* OLED 稍慢刷新，少占控制时间 */
        if (++ui_div >= 4U) {
            ui_div = 0;
            ui_show(&st);
        }

        delay_ms(CTRL_MS);
    }
}
