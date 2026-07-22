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

/* 负载侧角速度 °/s → 电机轴脉冲频率 Hz（float 版，精细速度） */
static uint32_t load_speed_to_pulse_hz_f(float speed_dps, uint8_t stepper_id)
{
    float motor_dps = speed_dps * gear_ratio_of(stepper_id);
    float hz        = motor_dps / STEP_DEG_PER_PULSE;
    if (hz < 1.0f)
        hz = 1.0f;
    return (uint32_t)hz;
}

/* 负载侧角速度 °/s → 电机轴脉冲频率 Hz（uint16 旧版，保留兼容） */
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
        clk    = DCC_100_PWM1_INST_CLK_FREQ;
        period = clk / frequency;
        if (period > 65535U)
            period = 65535U;
        /* 下限越小允许脉冲越快（可丢步，换速度） */
        if (period < 200U)
            period = 200U;
        DL_Timer_setLoadValue(DCC_100_PWM1_INST, period);
        DL_Timer_setCaptureCompareValue(DCC_100_PWM1_INST, period / 2U,
                                        GPIO_DCC_100_PWM1_C0_IDX);
    } else if (stepper_id == 2) {
        clk    = DCC_100_PWM2_INST_CLK_FREQ;
        period = clk / frequency;
        if (period > 65535U)
            period = 65535U;
        if (period < 200U)
            period = 200U;
        DL_Timer_setLoadValue(DCC_100_PWM2_INST, period);
        DL_Timer_setCaptureCompareValue(DCC_100_PWM2_INST, period / 2U,
                                        GPIO_DCC_100_PWM2_C0_IDX);
    }
}

void step_motor_init(void)
{
    /* 保证 STEP 脚为定时器 PWM（避免被 GPIO 测试改过） */
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_DCC_100_PWM1_C0_IOMUX, GPIO_DCC_100_PWM1_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_DCC_100_PWM1_C0_PORT, GPIO_DCC_100_PWM1_C0_PIN);
    DL_Timer_setCCPDirection(DCC_100_PWM1_INST, DL_TIMER_CC0_OUTPUT);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_DCC_100_PWM2_C0_IOMUX, GPIO_DCC_100_PWM2_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_DCC_100_PWM2_C0_PORT, GPIO_DCC_100_PWM2_C0_PIN);
    DL_Timer_setCCPDirection(DCC_100_PWM2_INST, DL_TIMER_CC0_OUTPUT);

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

/* angle：负载侧角度，°；电机1 内部 × 齿轮比；走完自动停 */
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

/*
 * 连续速度（float 版）— 跟踪主接口
 * speed_dps：负载侧 °/s，带符号
 */
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

    /* 连续转：中断不递减到 0 */
    remain_ptr = (stepper_id == 1) ? &step_remain_1 : &step_remain_2;
    *remain_ptr = STEP_REMAIN_CONTINUOUS;

    step_motor_start(stepper_id);
}

/* 旧 int16 接口，转调 float 版 */
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

void DCC_100_PWM1_INST_IRQHandler(void)
{
    switch (DL_Timer_getPendingInterrupt(DCC_100_PWM1_INST)) {
    case DL_TIMER_IIDX_LOAD:
        if (step_remain_1 == 0) {
            step_motor_stop(1);
            break;
        }
        /* 连续模式：保持 remain，不递减 */
        if (step_remain_1 != STEP_REMAIN_CONTINUOUS)
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
        if (step_remain_2 != STEP_REMAIN_CONTINUOUS)
            step_remain_2--;
        break;
    default:
        break;
    }
}
