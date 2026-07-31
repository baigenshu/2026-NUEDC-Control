/**
 * @file motor.c
 */
#include "motor.h"
#include "motor_cfg.h"
#include "ti_msp_dl_config.h"

typedef struct {
    uint32_t in1_pin;
    uint32_t in2_pin;
    GPTIMER_Regs *pwm_inst;
    DL_TIMER_CC_INDEX pwm_idx;
} motor_hw_t;

static const motor_hw_t s_hw[MOTOR_ID_COUNT] = {
    { GPIO_MOTOR_AIN1_PIN, GPIO_MOTOR_AIN2_PIN, PWMA_INST, GPIO_PWMA_C0_IDX },
    { GPIO_MOTOR_BIN1_PIN, GPIO_MOTOR_BIN2_PIN, PWMB_INST, GPIO_PWMB_C0_IDX },
    { GPIO_MOTOR_CIN1_PIN, GPIO_MOTOR_CIN2_PIN, PWMA_INST, GPIO_PWMA_C1_IDX },
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

static void ensure_pwm(void)
{
    DL_TimerG_setCaptureCompareCtl(PWMA_INST, DL_TIMER_CC_MODE_COMPARE, 0,
                                   DL_TIMER_CC_0_INDEX);
    DL_TimerG_setCaptureCompareCtl(PWMA_INST, DL_TIMER_CC_MODE_COMPARE, 0,
                                   DL_TIMER_CC_1_INDEX);
    DL_TimerG_setCaptureCompareCtl(PWMB_INST, DL_TIMER_CC_MODE_COMPARE, 0,
                                   DL_TIMER_CC_0_INDEX);
    DL_TimerG_setCaptureCompareCtl(PWMB_INST, DL_TIMER_CC_MODE_COMPARE, 0,
                                   DL_TIMER_CC_1_INDEX);

    DL_TimerG_setCaptureCompareAction(
        PWMA_INST, (DL_TIMER_CC_ZACT_CCP_HIGH | DL_TIMER_CC_CUACT_CCP_LOW),
        DL_TIMER_CC_0_INDEX);
    DL_TimerG_setCaptureCompareAction(
        PWMA_INST, (DL_TIMER_CC_ZACT_CCP_HIGH | DL_TIMER_CC_CUACT_CCP_LOW),
        DL_TIMER_CC_1_INDEX);
    DL_TimerG_setCaptureCompareAction(
        PWMB_INST, (DL_TIMER_CC_ZACT_CCP_HIGH | DL_TIMER_CC_CUACT_CCP_LOW),
        DL_TIMER_CC_0_INDEX);
    DL_TimerG_setCaptureCompareAction(
        PWMB_INST, (DL_TIMER_CC_ZACT_CCP_HIGH | DL_TIMER_CC_CUACT_CCP_LOW),
        DL_TIMER_CC_1_INDEX);

    DL_TimerG_setCCPDirection(PWMA_INST, DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT);
    DL_TimerG_setCCPDirection(PWMB_INST, DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT);
    DL_TimerG_enableClock(PWMA_INST);
    DL_TimerG_enableClock(PWMB_INST);
    DL_TimerG_startCounter(PWMA_INST);
    DL_TimerG_startCounter(PWMB_INST);
}

static void set_dir_pwm(const motor_hw_t *hw, int dir, uint16_t duty)
{
    if (dir > 0) {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, hw->in1_pin);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, hw->in2_pin);
    } else if (dir < 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, hw->in1_pin);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, hw->in2_pin);
    } else {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, hw->in1_pin | hw->in2_pin);
    }
    DL_TimerG_setCaptureCompareValue(hw->pwm_inst, (uint32_t)duty, hw->pwm_idx);
}

void Motor_Init(void)
{
    ensure_pwm();
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

void Motor_SetAll(int16_t a, int16_t b, int16_t c, int16_t d)
{
    Motor_Set(MOTOR_ID_A, a);
    Motor_Set(MOTOR_ID_B, b);
    Motor_Set(MOTOR_ID_C, c);
    Motor_Set(MOTOR_ID_D, d);
}

void Motor_StopAll(motor_stop_mode_t mode)
{
    motor_id_t i;

    for (i = MOTOR_ID_A; i < MOTOR_ID_COUNT; ++i) {
        const motor_hw_t *hw = &s_hw[i];
        DL_TimerG_setCaptureCompareValue(hw->pwm_inst, 0, hw->pwm_idx);
        if (mode == MOTOR_STOP_BRAKE)
            DL_GPIO_setPins(GPIO_MOTOR_PORT, hw->in1_pin | hw->in2_pin);
        else
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, hw->in1_pin | hw->in2_pin);
    }
}
