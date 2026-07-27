#include "ti_msp_dl_config.h"
#include "motor.h"

/*
 *   A: PWM=PA12 TIMG0_CC0   IN=PB13/PB15
 *   B: PWM=PA21 TIMG6_CC0   IN=PB4/PB6
 *   C: PWM=PA13 TIMA0_CC3_CMPL  IN=PB1/PB2
 *   D: PWM=PA22 TIMA0_CC1       IN=PB3/PB7
 *   STBY=PB16
 */

#define M_TIM_CD            TIMA0

#define M_PWMC_PORT         GPIOA
#define M_PWMC_PIN          DL_GPIO_PIN_13
#define M_PWMC_IOMUX        ((uint32_t)IOMUX_PINCM35)
#define M_PWMC_FUNC         IOMUX_PINCM35_PF_TIMA0_CCP3_CMPL
#define M_PWMC_CC           DL_TIMER_CC_3_INDEX

#define M_PWMD_PORT         GPIOA
#define M_PWMD_PIN          DL_GPIO_PIN_22
#define M_PWMD_IOMUX        ((uint32_t)IOMUX_PINCM47)
#define M_PWMD_FUNC         IOMUX_PINCM47_PF_TIMA0_CCP1
#define M_PWMD_CC           DL_TIMER_CC_1_INDEX

/* C/D 与 A/B 同指令 */
#define CD_GAIN_NUM         10U
#define CD_GAIN_DEN         10U

static uint16_t duty_of(uint16_t speed)
{
    if (speed > 100U) {
        speed = 100U;
    }
    if (speed == 0U) {
        return 0U;
    }
    {
        uint16_t d = (uint16_t)(((uint32_t)speed * MOTOR_PWM_PERIOD) / 100U);
        if (d >= MOTOR_PWM_PERIOD) {
            d = (uint16_t)(MOTOR_PWM_PERIOD - 1U);
        }
        if (d == 0U) {
            d = 1U;
        }
        return d;
    }
}

static uint16_t scale_cd(uint16_t speed)
{
    uint32_t s;
    if (speed == 0U) {
        return 0U;
    }
    s = ((uint32_t)speed * CD_GAIN_NUM) / CD_GAIN_DEN;
    if (s > 100U) {
        s = 100U;
    }
    if (s == 0U) {
        s = 1U;
    }
    return (uint16_t)s;
}

/* PA13=CMPL 反相脚：主通道 CC = period - duty */
static uint16_t duty_cmpl(uint16_t speed)
{
    uint16_t d = duty_of(speed);
    if (d == 0U) {
        return (uint16_t)(MOTOR_PWM_PERIOD - 1U);
    }
    return (uint16_t)(MOTOR_PWM_PERIOD - d);
}

static int16_t clamp_spd(int16_t s)
{
    if (s > 100) {
        return 100;
    }
    if (s < -100) {
        return -100;
    }
    return s;
}

static void stby_on(void)
{
    DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_STBY_PIN);
}

static void stby_off(void)
{
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_STBY_PIN);
}

static void tima0_cd_setup(void)
{
    static const DL_TimerA_ClockConfig clk = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 1U,
    };
    static const DL_TimerA_PWMConfig pwm = {
        .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
        .period = MOTOR_PWM_PERIOD,
        .isTimerWithFourCC = true,
        .startTimer = DL_TIMER_STOP,
    };

    DL_TimerA_reset(M_TIM_CD);
    DL_TimerA_enablePower(M_TIM_CD);
    delay_cycles(POWER_STARTUP_DELAY);

    DL_TimerA_setClockConfig(M_TIM_CD, (DL_TimerA_ClockConfig *)&clk);
    DL_TimerA_initPWMMode(M_TIM_CD, (DL_TimerA_PWMConfig *)&pwm);

    DL_TimerA_setCaptureCompareOutCtl(M_TIM_CD, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
        DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        M_PWMD_CC);
    DL_TimerA_setCaptureCompareOutCtl(M_TIM_CD, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
        DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        M_PWMC_CC);
    DL_TimerA_setCaptCompUpdateMethod(M_TIM_CD,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, M_PWMD_CC);
    DL_TimerA_setCaptCompUpdateMethod(M_TIM_CD,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, M_PWMC_CC);

    DL_TimerA_setCaptureCompareValue(M_TIM_CD, 0, M_PWMD_CC);
    DL_TimerA_setCaptureCompareValue(M_TIM_CD, MOTOR_PWM_PERIOD - 1U, M_PWMC_CC);

    DL_TimerA_enableClock(M_TIM_CD);
    DL_TimerA_setCCPDirection(M_TIM_CD,
        DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT |
            DL_TIMER_CC2_OUTPUT | DL_TIMER_CC3_OUTPUT);

    DL_GPIO_initPeripheralOutputFunction(M_PWMC_IOMUX, M_PWMC_FUNC);
    DL_GPIO_enableOutput(M_PWMC_PORT, M_PWMC_PIN);
    DL_GPIO_initPeripheralOutputFunction(M_PWMD_IOMUX, M_PWMD_FUNC);
    DL_GPIO_enableOutput(M_PWMD_PORT, M_PWMD_PIN);

    DL_TimerA_startCounter(M_TIM_CD);
}

static void ch_a_off(void)
{
    DL_TimerG_setCaptureCompareValue(PWMA_INST, 0, DL_TIMER_CC_0_INDEX);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN1_PIN | GPIO_MOTOR_AIN2_PIN);
}

static void ch_b_off(void)
{
    DL_TimerG_setCaptureCompareValue(PWMB_INST, 0, DL_TIMER_CC_0_INDEX);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN1_PIN | GPIO_MOTOR_BIN2_PIN);
}

static void ch_c_off(void)
{
    DL_TimerA_setCaptureCompareValue(M_TIM_CD, MOTOR_PWM_PERIOD - 1U, M_PWMC_CC);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_CIN1_PIN | GPIO_MOTOR_CIN2_PIN);
}

static void ch_d_off(void)
{
    DL_TimerA_setCaptureCompareValue(M_TIM_CD, 0, M_PWMD_CC);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_DIN1_PIN | GPIO_MOTOR_DIN2_PIN);
}

static void ch_a_run(int dir, uint16_t speed)
{
    uint16_t d = duty_of(speed);
    stby_on();
    if (dir > 0) {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN1_PIN);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN2_PIN);
    } else {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN2_PIN);
    }
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWMA_C0_IOMUX, GPIO_PWMA_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWMA_C0_PORT, GPIO_PWMA_C0_PIN);
    DL_TimerG_setCaptureCompareValue(PWMA_INST, d, DL_TIMER_CC_0_INDEX);
    DL_TimerG_startCounter(PWMA_INST);
}

static void ch_b_run(int dir, uint16_t speed)
{
    uint16_t d = duty_of(speed);
    stby_on();
    if (dir > 0) {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN1_PIN);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN2_PIN);
    } else {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN2_PIN);
    }
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWMB_C0_IOMUX, GPIO_PWMB_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWMB_C0_PORT, GPIO_PWMB_C0_PIN);
    DL_TimerG_setCaptureCompareValue(PWMB_INST, d, DL_TIMER_CC_0_INDEX);
    DL_TimerG_startCounter(PWMB_INST);
}

static void ch_c_run(int dir, uint16_t speed)
{
    uint16_t sp = scale_cd(speed);
    stby_on();
    if (dir > 0) {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_CIN1_PIN);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_CIN2_PIN);
    } else {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_CIN1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_CIN2_PIN);
    }
    DL_GPIO_initPeripheralOutputFunction(M_PWMC_IOMUX, M_PWMC_FUNC);
    DL_GPIO_enableOutput(M_PWMC_PORT, M_PWMC_PIN);
    DL_TimerA_setCaptureCompareValue(M_TIM_CD, duty_cmpl(sp), M_PWMC_CC);
    DL_TimerA_startCounter(M_TIM_CD);
}

static void ch_d_run(int dir, uint16_t speed)
{
    uint16_t sp = scale_cd(speed);
    stby_on();
    if (dir > 0) {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_DIN1_PIN);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_DIN2_PIN);
    } else {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_DIN1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_DIN2_PIN);
    }
    DL_GPIO_initPeripheralOutputFunction(M_PWMD_IOMUX, M_PWMD_FUNC);
    DL_GPIO_enableOutput(M_PWMD_PORT, M_PWMD_PIN);
    DL_TimerA_setCaptureCompareValue(M_TIM_CD, duty_of(sp), M_PWMD_CC);
    DL_TimerA_startCounter(M_TIM_CD);
}

void MotorA_Forward(uint16_t s) { ch_a_run(+1, s); }
void MotorA_Reverse(uint16_t s) { ch_a_run(-1, s); }
void MotorA_Brake(void) { ch_a_off(); }
void MotorA_Stop(void) { ch_a_off(); }
void MotorA_Init(void) { ch_a_off(); }
void MotorA_SetSpeed(int16_t speed)
{
    speed = clamp_spd(speed);
    if (speed > 0) {
        MotorA_Forward((uint16_t)speed);
    } else if (speed < 0) {
        MotorA_Reverse((uint16_t)(-speed));
    } else {
        MotorA_Stop();
    }
}

void MotorB_Forward(uint16_t s) { ch_b_run(+1, s); }
void MotorB_Reverse(uint16_t s) { ch_b_run(-1, s); }
void MotorB_Brake(void) { ch_b_off(); }
void MotorB_Stop(void) { ch_b_off(); }
void MotorB_Init(void) { ch_b_off(); }
void MotorB_SetSpeed(int16_t speed)
{
    speed = clamp_spd(speed);
    if (speed > 0) {
        MotorB_Forward((uint16_t)speed);
    } else if (speed < 0) {
        MotorB_Reverse((uint16_t)(-speed));
    } else {
        MotorB_Stop();
    }
}

void MotorC_Forward(uint16_t s) { ch_c_run(+1, s); }
void MotorC_Reverse(uint16_t s) { ch_c_run(-1, s); }
void MotorC_Brake(void) { ch_c_off(); }
void MotorC_Stop(void) { ch_c_off(); }
void MotorC_Init(void) { ch_c_off(); }
void MotorC_SetSpeed(int16_t speed)
{
    speed = clamp_spd(speed);
    if (speed > 0) {
        MotorC_Forward((uint16_t)speed);
    } else if (speed < 0) {
        MotorC_Reverse((uint16_t)(-speed));
    } else {
        MotorC_Stop();
    }
}

void MotorD_Forward(uint16_t s) { ch_d_run(+1, s); }
void MotorD_Reverse(uint16_t s) { ch_d_run(-1, s); }
void MotorD_Brake(void) { ch_d_off(); }
void MotorD_Stop(void) { ch_d_off(); }
void MotorD_Init(void) { ch_d_off(); }
void MotorD_SetSpeed(int16_t speed)
{
    speed = clamp_spd(speed);
    if (speed > 0) {
        MotorD_Forward((uint16_t)speed);
    } else if (speed < 0) {
        MotorD_Reverse((uint16_t)(-speed));
    } else {
        MotorD_Stop();
    }
}

void Motor_Init(void)
{
    /* A/B 仅用 SysConfig 的 TIMG0/TIMG6，勿改 CC0 配置 */
    DL_TimerG_setCCPDirection(PWMA_INST, DL_TIMER_CC0_OUTPUT);
    DL_TimerG_setCCPDirection(PWMB_INST, DL_TIMER_CC0_OUTPUT);
    DL_TimerG_startCounter(PWMA_INST);
    DL_TimerG_startCounter(PWMB_INST);

    tima0_cd_setup();

    ch_a_off();
    ch_b_off();
    ch_c_off();
    ch_d_off();
    stby_on();
}

void Motor_AllStop(void)
{
    ch_a_off();
    ch_b_off();
    ch_c_off();
    ch_d_off();
}

void Motor_AllBrake(void)
{
    Motor_AllStop();
}

void Motor_Standby(void)
{
    Motor_AllStop();
    stby_off();
}

void Motor_SetPWM(int16_t m1, int16_t m2, int16_t m3, int16_t m4)
{
    MotorA_SetSpeed(m1);
    MotorB_SetSpeed(m2);
    MotorC_SetSpeed(m3);
    MotorD_SetSpeed(m4);
}
