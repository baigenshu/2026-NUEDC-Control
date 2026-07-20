#include "step_motor.h"

/* 剩余脉冲计数（LOAD 中断里递减，单位：电机轴脉冲） */
static volatile uint32_t step_remain_1 = 0;
static volatile uint32_t step_remain_2 = 0;

static float gear_ratio_of(uint8_t stepper_id)
{
    if (stepper_id == 1)
        return STEP_MOTOR1_GEAR_RATIO;
    return STEP_MOTOR2_GEAR_RATIO;
}

/* 负载侧角度 → 电机轴脉冲数 */
static uint32_t load_angle_to_pulses(uint8_t angle_deg, uint8_t stepper_id)
{
    float motor_deg = (float)angle_deg * gear_ratio_of(stepper_id);
    uint32_t pulses = (uint32_t)(motor_deg / STEP_DEG_PER_PULSE);
    return pulses;
}

/* 负载侧角速度 °/s → 电机轴脉冲频率 Hz */
static uint32_t load_speed_to_pulse_hz(uint8_t speed_dps, uint8_t stepper_id)
{
    float motor_dps = (float)speed_dps * gear_ratio_of(stepper_id);
    uint32_t hz     = (uint32_t)(motor_dps / STEP_DEG_PER_PULSE);
    if (hz < 1U)
        hz = 1U;
    return hz;
}

void step_motor_init(void)
{
    /* 电机1：RST/SLP/DCY/DIR 拉高使能 */
    DL_GPIO_setPins(STEP_MOTOR1_PORT, STEP_MOTOR1_RST1_PIN);
    DL_GPIO_setPins(STEP_MOTOR1_PORT, STEP_MOTOR1_SLP1_PIN);
    DL_GPIO_setPins(STEP_MOTOR1_PORT, STEP_MOTOR1_DIR1_PIN);
    DL_GPIO_setPins(STEP_MOTOR1_PORT, STEP_MOTOR1_DCY1_PIN);
    NVIC_EnableIRQ(DCC_100_PWM1_INST_INT_IRQN);

    /* 电机2：同上 */
    DL_GPIO_setPins(STEP_MOTOR2_PORT, STEP_MOTOR2_RST2_PIN);
    DL_GPIO_setPins(STEP_MOTOR2_PORT, STEP_MOTOR2_SLP2_PIN);
    DL_GPIO_setPins(STEP_MOTOR2_PORT, STEP_MOTOR2_DIR2_PIN);
    DL_GPIO_setPins(STEP_MOTOR2_PORT, STEP_MOTOR2_DCY2_PIN);
    NVIC_EnableIRQ(DCC_100_PWM2_INST_INT_IRQN);
}

void step_motor_dir_set(uint8_t direction, uint8_t stepper_id)
{
    if (stepper_id == 1) {
        if (direction == 0)
            DL_GPIO_clearPins(STEP_MOTOR1_PORT, STEP_MOTOR1_DIR1_PIN);
        else
            DL_GPIO_setPins(STEP_MOTOR1_PORT, STEP_MOTOR1_DIR1_PIN);
    } else if (stepper_id == 2) {
        if (direction == 0)
            DL_GPIO_clearPins(STEP_MOTOR2_PORT, STEP_MOTOR2_DIR2_PIN);
        else
            DL_GPIO_setPins(STEP_MOTOR2_PORT, STEP_MOTOR2_DIR2_PIN);
    }
}

void step_motor_start(uint8_t stepper_id)
{
    if (stepper_id == 1) {
        NVIC_EnableIRQ(DCC_100_PWM1_INST_INT_IRQN);
        DL_Timer_startCounter(DCC_100_PWM1_INST);
    } else if (stepper_id == 2) {
        NVIC_EnableIRQ(DCC_100_PWM2_INST_INT_IRQN);
        DL_Timer_startCounter(DCC_100_PWM2_INST);
    }
}

void step_motor_stop(uint8_t stepper_id)
{
    if (stepper_id == 1) {
        DL_Timer_stopCounter(DCC_100_PWM1_INST);
        step_remain_1 = 0;
    } else if (stepper_id == 2) {
        DL_Timer_stopCounter(DCC_100_PWM2_INST);
        step_remain_2 = 0;
    }
}

/* speed：负载侧角速度，°/s；电机1 内部 × 齿轮比 */
void step_set_speed(uint8_t speed, uint8_t stepper_id)
{
    uint32_t frequency;
    uint32_t period;
    uint32_t clk;

    if (speed == 0) {
        step_motor_stop(stepper_id);
        return;
    }

    frequency = load_speed_to_pulse_hz(speed, stepper_id);

    if (stepper_id == 1) {
        clk    = DCC_100_PWM1_INST_CLK_FREQ;
        period = clk / frequency;
        if (period > 65535U)
            period = 65535U;
        if (period < 800U)
            period = 800U;
        DL_Timer_setLoadValue(DCC_100_PWM1_INST, period);
        DL_Timer_setCaptureCompareValue(DCC_100_PWM1_INST, period / 2U,
                                        GPIO_DCC_100_PWM1_C0_IDX);
    } else if (stepper_id == 2) {
        clk    = DCC_100_PWM2_INST_CLK_FREQ;
        period = clk / frequency;
        if (period > 65535U)
            period = 65535U;
        if (period < 800U)
            period = 800U;
        DL_Timer_setLoadValue(DCC_100_PWM2_INST, period);
        DL_Timer_setCaptureCompareValue(DCC_100_PWM2_INST, period / 2U,
                                        GPIO_DCC_100_PWM2_C0_IDX);
    }
}

/* angle：负载侧角度，°；电机1 内部 × 齿轮比 */
void step_motor_set_angle(uint8_t angle, uint8_t stepper_id)
{
    uint32_t pulses = load_angle_to_pulses(angle, stepper_id);

    if (stepper_id == 1) {
        step_remain_1 = pulses;
    } else if (stepper_id == 2) {
        step_remain_2 = pulses;
    }
    step_motor_start(stepper_id);
}

void DCC_100_PWM1_INST_IRQHandler(void)
{
    switch (DL_Timer_getPendingInterrupt(DCC_100_PWM1_INST)) {
    case DL_TIMER_IIDX_LOAD:
        if (step_remain_1 == 0) {
            step_motor_stop(1);
            break;
        }
        step_remain_1--;
        break;
    default:
        break;
    }
}

void DCC_100_PWM2_INST_IRQHandler(void)
{
    switch (DL_Timer_getPendingInterrupt(DCC_100_PWM2_INST)) {
    case DL_TIMER_IIDX_LOAD:
        if (step_remain_2 == 0) {
            step_motor_stop(2);
            break;
        }
        step_remain_2--;
        break;
    default:
        break;
    }
}
