/**
 * @file main.c
 * @brief balance：人工调平校零后启动视觉球位闭环
 *
 * 结构：合页 + 凹槽摆杆 + 曲柄连杆
 * 链路：MaixCAM type=0x02 → UART0 → 位置 PID → 曲柄倾角 → 钢珠位置
 * 定点默认 O；type=0x12 或 BallCtrl_SetTarget* 可改任意位置
 */
#include "ti_msp_dl_config.h"
#include "stepper.h"
#include "tmc2209.h"
#include "vision_uart.h"
#include "ball_ctrl.h"

static volatile uint32_t s_sys_ms;

#define BALANCE_KEY_DEBOUNCE_MS (20u)

static bool     s_key_raw_pressed;
static bool     s_key_stable_pressed;
static uint32_t s_key_changed_ms;

void SysTick_Handler(void)
{
    s_sys_ms++;
    VisionUart_OnMsTick();
    BallCtrl_OnMsTick();
}

static bool balance_key_pressed(void)
{
    return (DL_GPIO_readPins(
        GPIO_BALANCE_KEY_PORT, GPIO_BALANCE_KEY_START_PIN) &
        GPIO_BALANCE_KEY_START_PIN) == 0u;
}

static void balance_start(void)
{
    if (BallCtrl_IsEnabled())
        return;

    BallCtrl_RequestCalibratedStart();
}

static void balance_stop(void)
{
    if (BallCtrl_IsEnabled())
        BallCtrl_Enable(false);
}

static void balance_start_control_poll(void)
{
    ball_control_cmd_t control;
    bool pressed = balance_key_pressed();

    if (pressed != s_key_raw_pressed) {
        s_key_raw_pressed = pressed;
        s_key_changed_ms = s_sys_ms;
    } else if (pressed != s_key_stable_pressed &&
               s_sys_ms - s_key_changed_ms >= BALANCE_KEY_DEBOUNCE_MS) {
        s_key_stable_pressed = pressed;
        if (pressed)
            balance_start();
    }

    if (VisionUart_TakeControl(&control) && control.valid) {
        if (control.start)
            balance_start();
        else
            balance_stop();
    }
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
    s_key_raw_pressed = balance_key_pressed();
    s_key_stable_pressed = s_key_raw_pressed;
    s_key_changed_ms = s_sys_ms;

    while (1) {
        VisionUart_Poll();
        balance_start_control_poll();
        BallCtrl_Update();
        __WFI();
    }
}
