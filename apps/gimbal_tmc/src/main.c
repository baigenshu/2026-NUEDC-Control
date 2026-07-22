#include "ti_msp_dl_config.h"
#include "delay.h"
#include "step_motor.h"

/*
 * TMC2209 双路步进测试
 *
 * 接线同 apps/gimbal：
 *   电机1: PA0 STEP / PA1 DIR / PA7 EN / PA8 MS1 / PA9 MS2
 *   电机2: PA12 STEP / PA13 DIR / PA14 EN / PA15 MS1 / PA16 MS2
 *   PDN_UART 硬件拉高，CLK 悬空
 *
 * 上电后两路各以 30°/s 正转 2s，再反转 2s，循环。
 */
int main(void)
{
    SYSCFG_DL_init();
    systick_init();
    step_motor_init();

    while (1) {
        step_set_velocity_f(30.0f, 1);
        step_set_velocity_f(30.0f, 2);
        delay_ms(2000);

        step_set_velocity_f(-30.0f, 1);
        step_set_velocity_f(-30.0f, 2);
        delay_ms(2000);

        step_motor_stop(1);
        step_motor_stop(2);
        delay_ms(500);
    }
}
