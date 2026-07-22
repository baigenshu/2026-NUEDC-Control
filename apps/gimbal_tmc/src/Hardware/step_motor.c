#include "step_motor.h"

/* 剩余脉冲计数（LOAD 中断里递减；STEP_REMAIN_CONTINUOUS 表示连续转） */
static volatile uint32_t step_remain_1 = 0;
static volatile uint32_t step_remain_2 = 0;

static float gear_ratio_of(uint8_t stepper_id)
{
    if (stepper_id == 1)
        return STEP_MOTOR1_GEAR_RATIO;
    return STEP_MOTOR2_GEAR_RATIO;
}

/* 负载侧角度 → 电机轴脉冲数 */
static uint32_t load_angle_to_pulses(uint16_t angle_deg, uint8_t stepper_id)
{
    float motor_deg = (float)angle_deg * gear_ratio_of(stepper_id);
    uint32_t pulses = (uint32_t)(motor_deg / STEP_DEG_PER_PULSE);
    return pulses;
}

/* 负载侧角速度 °/s → 电机轴脉冲频率 Hz（float 版） */
static uint32_t load_speed_to_pulse_hz_f(float speed_dps, uint8_t stepper_id)
{
    float motor_dps = speed_dps * gear_ratio_of(stepper_id);
    float hz        = motor_dps / STEP_DEG_PER_PULSE;
    if (hz < 1.0f)
        hz = 1.0f;
    return (uint32_t)hz;
}

/* 负载侧角速度 °/s → 电机轴脉冲频率 Hz（uint16 旧版） */
static uint32_t load_speed_to_pulse_hz(uint16_t speed_dps, uint8_t stepper_id)
{
    float motor_dps = (float)speed_dps * gear_ratio_of(stepper_id);
    uint32_t hz     = (uint32_t)(motor_dps / STEP_DEG_PER_PULSE);
    if (hz < 1U)
        hz = 1U;
    return hz;
}

static void apply_pwm_period(uint8_t stepper_id, uint32_t frequency)
{
    uint32_t period;
    uint32_t clk;

    if (frequency < 1U)
        frequency = 1U;

    if (stepper_id == 1) {
        clk    = TMC_PWM1_INST_CLK_FREQ;
        period = clk / frequency;
        if (period > 65535U)
            period = 65535U;
        if (period < 800U)
            period = 800U;
        DL_Timer_setLoadValue(TMC_PWM1_INST, period);
        DL_Timer_setCaptureCompareValue(TMC_PWM1_INST, period / 2U,
                                        GPIO_TMC_PWM1_C0_IDX);
    } else if (stepper_id == 2) {
        clk    = TMC_PWM2_INST_CLK_FREQ;
        period = clk / frequency;
        if (period > 65535U)
            period = 65535U;
        if (period < 800U)
            period = 800U;
        DL_Timer_setLoadValue(TMC_PWM2_INST, period);
        DL_Timer_setCaptureCompareValue(TMC_PWM2_INST, period / 2U,
                                        GPIO_TMC_PWM2_C0_IDX);
    }
}

/* MS1/MS2 表：1/8, 1/16, 1/32, 1/64 */
static void apply_microstep_pins(GPIO_Regs *port, uint32_t ms1_pin, uint32_t ms2_pin,
                                 tmc_microstep_t ms)
{
    switch (ms) {
    case TMC_MICROSTEP_1_8:
        DL_GPIO_clearPins(port, ms1_pin);
        DL_GPIO_clearPins(port, ms2_pin);
        break;
    case TMC_MICROSTEP_1_16:
        DL_GPIO_setPins(port, ms1_pin);
        DL_GPIO_setPins(port, ms2_pin);
        break;
    case TMC_MICROSTEP_1_32:
        DL_GPIO_setPins(port, ms1_pin);
        DL_GPIO_clearPins(port, ms2_pin);
        break;
    case TMC_MICROSTEP_1_64:
        DL_GPIO_clearPins(port, ms1_pin);
        DL_GPIO_setPins(port, ms2_pin);
        break;
    default:
        DL_GPIO_setPins(port, ms1_pin);
        DL_GPIO_clearPins(port, ms2_pin);
        break;
    }
}

void step_motor_set_microstep(tmc_microstep_t ms)
{
    apply_microstep_pins(TMC1_PORT, TMC1_MS1_1_PIN, TMC1_MS2_1_PIN, ms);
    apply_microstep_pins(TMC2_PORT, TMC2_MS1_2_PIN, TMC2_MS2_2_PIN, ms);
}

void step_motor_enable(uint8_t stepper_id, uint8_t enable)
{
    /* EN 低有效 */
    if (stepper_id == 1) {
        if (enable)
            DL_GPIO_clearPins(TMC1_PORT, TMC1_EN1_PIN);
        else
            DL_GPIO_setPins(TMC1_PORT, TMC1_EN1_PIN);
    } else if (stepper_id == 2) {
        if (enable)
            DL_GPIO_clearPins(TMC2_PORT, TMC2_EN2_PIN);
        else
            DL_GPIO_setPins(TMC2_PORT, TMC2_EN2_PIN);
    }
}

void step_motor_init(void)
{
    /* 保证 STEP 脚为定时器 PWM */
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_TMC_PWM1_C0_IOMUX, GPIO_TMC_PWM1_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_TMC_PWM1_C0_PORT, GPIO_TMC_PWM1_C0_PIN);
    DL_Timer_setCCPDirection(TMC_PWM1_INST, DL_TIMER_CC0_OUTPUT);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_TMC_PWM2_C0_IOMUX, GPIO_TMC_PWM2_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_TMC_PWM2_C0_PORT, GPIO_TMC_PWM2_C0_PIN);
    DL_Timer_setCCPDirection(TMC_PWM2_INST, DL_TIMER_CC0_OUTPUT);

    /* 默认 1/32 细分 → 6400 pulse/rev */
    step_motor_set_microstep(TMC_MICROSTEP_1_32);

    /* DIR 默认高；使能两路驱动 */
    DL_GPIO_setPins(TMC1_PORT, TMC1_DIR1_PIN);
    DL_GPIO_setPins(TMC2_PORT, TMC2_DIR2_PIN);
    step_motor_enable(1, 1);
    step_motor_enable(2, 1);

    NVIC_EnableIRQ(TMC_PWM1_INST_INT_IRQN);
    NVIC_EnableIRQ(TMC_PWM2_INST_INT_IRQN);
}

void step_motor_dir_set(uint8_t direction, uint8_t stepper_id)
{
    if (stepper_id == 1) {
        if (direction == 0)
            DL_GPIO_clearPins(TMC1_PORT, TMC1_DIR1_PIN);
        else
            DL_GPIO_setPins(TMC1_PORT, TMC1_DIR1_PIN);
    } else if (stepper_id == 2) {
        if (direction == 0)
            DL_GPIO_clearPins(TMC2_PORT, TMC2_DIR2_PIN);
        else
            DL_GPIO_setPins(TMC2_PORT, TMC2_DIR2_PIN);
    }
}

void step_motor_start(uint8_t stepper_id)
{
    if (stepper_id == 1) {
        NVIC_EnableIRQ(TMC_PWM1_INST_INT_IRQN);
        DL_Timer_startCounter(TMC_PWM1_INST);
    } else if (stepper_id == 2) {
        NVIC_EnableIRQ(TMC_PWM2_INST_INT_IRQN);
        DL_Timer_startCounter(TMC_PWM2_INST);
    }
}

void step_motor_stop(uint8_t stepper_id)
{
    if (stepper_id == 1) {
        DL_Timer_stopCounter(TMC_PWM1_INST);
        step_remain_1 = 0;
    } else if (stepper_id == 2) {
        DL_Timer_stopCounter(TMC_PWM2_INST);
        step_remain_2 = 0;
    }
}

void step_set_speed(uint16_t speed, uint8_t stepper_id)
{
    uint32_t frequency;

    if (speed == 0) {
        step_motor_stop(stepper_id);
        return;
    }

    frequency = load_speed_to_pulse_hz(speed, stepper_id);
    apply_pwm_period(stepper_id, frequency);
}

void step_motor_set_angle(uint16_t angle, uint8_t stepper_id)
{
    uint32_t pulses = load_angle_to_pulses(angle, stepper_id);

    if (stepper_id == 1) {
        step_remain_1 = pulses;
    } else if (stepper_id == 2) {
        step_remain_2 = pulses;
    }
    step_motor_start(stepper_id);
}

void step_set_velocity_f(float speed_dps, uint8_t stepper_id)
{
    float    abs_spd;
    uint32_t frequency;
    volatile uint32_t *remain_ptr;

    if (speed_dps > -0.05f && speed_dps < 0.05f) {
        step_motor_stop(stepper_id);
        return;
    }

    if (speed_dps > 0.0f) {
        step_motor_dir_set(1, stepper_id);
        abs_spd = speed_dps;
    } else {
        step_motor_dir_set(0, stepper_id);
        abs_spd = -speed_dps;
    }

    frequency = load_speed_to_pulse_hz_f(abs_spd, stepper_id);
    apply_pwm_period(stepper_id, frequency);

    remain_ptr  = (stepper_id == 1) ? &step_remain_1 : &step_remain_2;
    *remain_ptr = STEP_REMAIN_CONTINUOUS;

    step_motor_start(stepper_id);
}

void step_motor_set_velocity(int16_t speed_dps, uint8_t stepper_id)
{
    step_set_velocity_f((float)speed_dps, stepper_id);
}

uint8_t step_motor_is_busy(uint8_t stepper_id)
{
    if (stepper_id == 1)
        return step_remain_1 != 0U ? 1U : 0U;
    if (stepper_id == 2)
        return step_remain_2 != 0U ? 1U : 0U;
    return 0U;
}

void TMC_PWM1_INST_IRQHandler(void)
{
    switch (DL_Timer_getPendingInterrupt(TMC_PWM1_INST)) {
    case DL_TIMER_IIDX_LOAD:
        if (step_remain_1 == 0) {
            step_motor_stop(1);
            break;
        }
        if (step_remain_1 != STEP_REMAIN_CONTINUOUS)
            step_remain_1--;
        break;
    default:
        break;
    }
}

void TMC_PWM2_INST_IRQHandler(void)
{
    switch (DL_Timer_getPendingInterrupt(TMC_PWM2_INST)) {
    case DL_TIMER_IIDX_LOAD:
        if (step_remain_2 == 0) {
            step_motor_stop(2);
            break;
        }
        if (step_remain_2 != STEP_REMAIN_CONTINUOUS)
            step_remain_2--;
        break;
    default:
        break;
    }
}
