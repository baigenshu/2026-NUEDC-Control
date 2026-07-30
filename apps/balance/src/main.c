/**
 * @file main.c
 * @brief balance：视觉球位闭环停球
 *
 * MaixCAM type=0x02 → UART0 → PD → 丝杆倾角
 * 定点默认 O；type=0x12 或 BallCtrl_SetTarget* 可改任意位置
 */
#include "ti_msp_dl_config.h"
#include "stepper.h"
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

    VisionUart_Init();
    BallCtrl_Init();

    /* 默认停在视觉 O；赛题定点例如 +50.0 mm → SetTargetMm_x100(5000) */
    BallCtrl_SetTargetMm_x100(0);
    BallCtrl_Enable(true);

    while (1) {
        VisionUart_Poll();
        BallCtrl_Update();
        __WFI();
    }
}