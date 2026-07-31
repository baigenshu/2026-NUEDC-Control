/**
 * @file main.c
 * @brief balance：视觉球位闭环停球（静止定点 · 初步要求）
 *
 * 结构：合页 + 凹槽摆杆 + 曲柄连杆
 * 链路：MaixCAM type=0x02 → UART0 → 位置 PD → 曲柄倾角 → 钢珠位置
 * 定点默认 O；type=0x12 或 BallCtrl_SetTarget* 可改任意位置
 */
#include "ti_msp_dl_config.h"
#include "stepper.h"
#include "tmc2209.h"
#include "vision_uart.h"
#include "ball_ctrl.h"

static volatile uint32_t s_sys_ms;

void SysTick_Handler(void)
{
    s_sys_ms++;
    VisionUart_OnMsTick();
    BallCtrl_OnMsTick();
}

int main(void)
{
    SYSCFG_DL_init();

    Stepper_Init();
    Stepper_SetEnable(false);
    TMC2209_Init();

    VisionUart_Init();
    BallCtrl_Init();

    BallCtrl_SetTargetMm_x100(0);
    BallCtrl_Enable(true);

    while (1) {
        VisionUart_Poll();
        BallCtrl_Update();
        __WFI();
    }
}
