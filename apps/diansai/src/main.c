#include "ti_msp_dl_config.h"
#include "board.h"

/*
 * 仅测试汇电籽-601：OLED 显示 yaw / pitch / roll（单位 0.01°）
 * 无电机、无里程计。转板子看数值是否变化；F 为有效帧计数。
 */

#define OLED_PERIOD_MS  100u

/* 显示 0.01° 整数，例如 12345 → 123.45° */
static void oled_show_imu(void)
{
    int32_t y = (int32_t)(IMU601_Attitude.yaw   * 100.0f);
    int32_t p = (int32_t)(IMU601_Attitude.pitch * 100.0f);
    int32_t r = (int32_t)(IMU601_Attitude.roll  * 100.0f);
    uint32_t f = IMU601_FrameCount;

    OLED_ShowString(1, 1, "IMU601 ");
    OLED_ShowString(1, 8, "F:");
    OLED_ShowNum(1, 10, f % 10000u, 4);

    OLED_ShowString(2, 1, "Y:");
    OLED_ShowSignedNum(2, 3, y, 6);

    OLED_ShowString(3, 1, "P:");
    OLED_ShowSignedNum(3, 3, p, 6);

    OLED_ShowString(4, 1, "R:");
    OLED_ShowSignedNum(4, 3, r, 6);
}

int main(void)
{
    uint32_t t_oled;

    SYSCFG_DL_init();
    Timebase_Init();
    IMU601_Init();
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(1, 1, "IMU wait");
    delay_ms(500);

    t_oled = millis();

    while (1) {
        uint32_t now = millis();
        if ((now - t_oled) >= OLED_PERIOD_MS) {
            oled_show_imu();
            t_oled = now;
        }
    }
}
