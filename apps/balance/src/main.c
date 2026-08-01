/**
 * @file main.c
 * @brief balance：视觉闭环、普通平衡与 0 → +50 → -50 mm 预设运动
 *
 * 结构：合页 + 凹槽摆杆 + 曲柄连杆
 * 链路：MaixCAM type=0x02 → UART0 → 位置 PID → 曲柄倾角 → 钢珠位置
 * PB8/0x13 Start 机械置零后在 O 点平衡；0x13 Preset 启动往返运动
 */
#include "ti_msp_dl_config.h"
#include "stepper.h"
#include "tmc2209.h"
#include "vision_uart.h"
#include "ball_ctrl.h"
#include "preset_motion.h"

static volatile uint32_t s_sys_ms;

#define BALANCE_KEY_DEBOUNCE_MS (20u)

static bool     s_key_raw_pressed;
static bool     s_key_stable_pressed;
static uint32_t s_key_changed_ms;
static bool     s_control_action_seen;
static uint8_t  s_last_control_action;
static uint32_t s_last_control_action_ms;

#define BALANCE_CONTROL_DEBOUNCE_MS (300u)

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
    PresetMotion_Cancel();
    if (BallCtrl_IsEnabled())
        BallCtrl_Enable(false);
    BallCtrl_SetTargetMm_x100(0);
    BallCtrl_ZeroArmAndStart();
}

static void balance_start_preset(void)
{
    PresetMotion_Start(s_sys_ms);
}

static void balance_reset(void)
{
    PresetMotion_Cancel();
    BallCtrl_Enable(false);
    BallCtrl_SetTargetMm_x100(0);
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
        if (s_control_action_seen &&
            control.action == s_last_control_action &&
            s_sys_ms - s_last_control_action_ms <
                BALANCE_CONTROL_DEBOUNCE_MS)
            return;

        s_control_action_seen = true;
        s_last_control_action = control.action;
        s_last_control_action_ms = s_sys_ms;
        if (control.action == BALL_CONTROL_ACTION_START)
            balance_start();
        else if (control.action == BALL_CONTROL_ACTION_PRESET)
            balance_start_preset();
        else
            balance_reset();
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
    PresetMotion_Init();

    BallCtrl_SetTargetMm_x100(0);
    s_key_raw_pressed = balance_key_pressed();
    s_key_stable_pressed = s_key_raw_pressed;
    s_key_changed_ms = s_sys_ms;
    s_control_action_seen = false;
    s_last_control_action = BALL_CONTROL_ACTION_RESET;
    s_last_control_action_ms = 0u;

    while (1) {
        VisionUart_Poll();
        balance_start_control_poll();
        BallCtrl_Update(!PresetMotion_IsActive());
        PresetMotion_Update(s_sys_ms);
        __WFI();
    }
}
