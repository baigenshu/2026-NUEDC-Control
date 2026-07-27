/**
 * @file motor.c
 * @brief 四轮 TB6612 类 H 桥驱动
 */
#include "motor.h"
#include "chassis_cfg.h"
#include "ti_msp_dl_config.h"

typedef struct {
    uint32_t in1_pin;
    uint32_t in2_pin;
    GPTIMER_Regs *pwm_inst;
    DL_TIMER_CC_INDEX  pwm_idx;
} motor_hw_t;

static const motor_hw_t s_hw[MOTOR_ID_COUNT] = {
    /* A 右后: PWMA C0 PA12 */
    { GPIO_MOTOR_AIN1_PIN, GPIO_MOTOR_AIN2_PIN, PWMA_INST, GPIO_PWMA_C0_IDX },
    /* B 右前: PWMB C0 PA21 */
    { GPIO_MOTOR_BIN1_PIN, GPIO_MOTOR_BIN2_PIN, PWMB_INST, GPIO_PWMB_C0_IDX },
    /* C 左前: PWMA C1 PA13 */
    { GPIO_MOTOR_CIN1_PIN, GPIO_MOTOR_CIN2_PIN, PWMA_INST, GPIO_PWMA_C1_IDX },
    /* D 左后: PWMB C1 PA22 */
    { GPIO_MOTOR_DIN1_PIN, GPIO_MOTOR_DIN2_PIN, PWMB_INST, GPIO_PWMB_C1_IDX },
};

static uint16_t clamp_duty(int16_t abs_duty)
{
    if (abs_duty < 0)
        abs_duty = (int16_t)(-abs_duty);
    if (abs_duty > 0 && abs_duty < PWM_DEADZONE)
        abs_duty = (int16_t)PWM_DEADZONE;
    if (abs_duty > PWM_MAX)
        abs_duty = (int16_t)PWM_MAX;
    return (uint16_t)abs_duty;
}

static void set_dir_pwm(const motor_hw_t *hw, int dir, uint16_t duty)
{
    /* dir: +1 正转 IN1=0 IN2=1；-1 反转 IN1=1 IN2=0；0 滑行/刹 */
    if (dir > 0) {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, hw->in1_pin);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, hw->in2_pin);
    } else if (dir < 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, hw->in1_pin);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, hw->in2_pin);
    } else {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, hw->in1_pin | hw->in2_pin);
    }
    DL_TimerG_setCaptureCompareValue(hw->pwm_inst, duty, hw->pwm_idx);
}

void Motor_Init(void)
{
    Motor_StopAll(MOTOR_STOP_COAST);
    Motor_SetEnable(false);
}

void Motor_SetEnable(bool on)
{
    if (on)
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_STBY_PIN);
    else
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_STBY_PIN);
}

void Motor_Set(motor_id_t id, int16_t duty)
{
    const motor_hw_t *hw;
    uint16_t d;

    if ((unsigned)id >= MOTOR_ID_COUNT)
        return;
    hw = &s_hw[id];

    if (duty == 0) {
        set_dir_pwm(hw, 0, 0);
        return;
    }

    d = clamp_duty(duty);
    set_dir_pwm(hw, (duty > 0) ? +1 : -1, d);
}

void Motor_StopAll(motor_stop_mode_t mode)
{
    motor_id_t i;

    for (i = MOTOR_ID_A; i < MOTOR_ID_COUNT; ++i) {
        const motor_hw_t *hw = &s_hw[i];
        DL_TimerG_setCaptureCompareValue(hw->pwm_inst, 0, hw->pwm_idx);
        if (mode == MOTOR_STOP_BRAKE) {
            DL_GPIO_setPins(GPIO_MOTOR_PORT, hw->in1_pin | hw->in2_pin);
        } else {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, hw->in1_pin | hw->in2_pin);
        }
    }
}
