#include "ti_msp_dl_config.h"
#include "board.h"

/*
 * TB6612FNG 双路直流电机测试（非步进电机）
 *
 * 测试流程循环：
 *   1. A 正转  → A 反转  → A 停止
 *   2. B 正转  → B 反转  → B 停止
 *   3. A+B 同向正转 → A+B 同向反转 → 刹车
 *
 * 注意：delay_ms 单次上限约 209ms（SysTick 满量程），
 * 长延时请用 delay_ms_long()。
 */

/* 测试用占空比（0~100） */
#define TEST_SPEED      40
#define SLOW_SPEED      25

/* 长延时：拆成多次 delay_ms，规避 ~209ms 上限 */
static void delay_ms_long(uint32_t ms)
{
    while (ms > 200)
    {
        delay_ms(200);
        ms -= 200;
    }
    if (ms)
        delay_ms(ms);
}

int main(void)
{
    SYSCFG_DL_init();
    Motor_Init();     /* STBY=1，A/B 方向脚清零，PWM=0 */

    while (1)
    {
        /* ---- 1. 电机 A 单独测试 ---- */
        MotorA_SetSpeed(TEST_SPEED);
        delay_ms_long(2000);

        MotorA_SetSpeed(-TEST_SPEED);
        delay_ms_long(2000);

        MotorA_Stop();
        delay_ms_long(1000);

        /* ---- 2. 电机 B 单独测试 ---- */
        MotorB_SetSpeed(TEST_SPEED);
        delay_ms_long(2000);

        MotorB_SetSpeed(-TEST_SPEED);
        delay_ms_long(2000);

        MotorB_Stop();
        delay_ms_long(1000);

        /* ---- 3. 双电机同向 ---- */
        MotorA_SetSpeed(SLOW_SPEED);
        MotorB_SetSpeed(SLOW_SPEED);
        delay_ms_long(2000);

        MotorA_SetSpeed(-SLOW_SPEED);
        MotorB_SetSpeed(-SLOW_SPEED);
        delay_ms_long(2000);

        /* 刹车（短接制动） */
        MotorA_Brake();
        MotorB_Brake();
        delay_ms_long(1000);

        MotorA_Stop();
        MotorB_Stop();
        delay_ms_long(1500);
    }
}
