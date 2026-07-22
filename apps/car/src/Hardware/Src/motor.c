#include "ti_msp_dl_config.h"
#include "motor.h"

/*
 * TB6612FNG Motor Driver — Dual Motor (A + B)
 *
 * Pin Mapping:
 *   Motor A: AIN1=PB13, AIN2=PB15, PWMA=TIMG0/PA12
 *   Motor B: BIN1=PB4,  BIN2=PB6,  PWMB=TIMG6/PA21
 *   Common:  STBY=PB16
 *
 * Control Logic:
 *   IN1=0, IN2=1 → Forward
 *   IN1=1, IN2=0 → Reverse
 *   IN1=1, IN2=1 → Brake (short)
 *   IN1=0, IN2=0 → Stop (coast)
 *   STBY=1 → Enable, STBY=0 → Standby
 */

/* ============ Motor A helpers ============ */

static inline void motorA_AIN1_HIGH(void)
    { DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN1_PIN); }
static inline void motorA_AIN1_LOW(void)
    { DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN1_PIN); }
static inline void motorA_AIN2_HIGH(void)
    { DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN2_PIN); }
static inline void motorA_AIN2_LOW(void)
    { DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN2_PIN); }
static inline void motorA_SetPWM(uint16_t value)
    { DL_TimerG_setCaptureCompareValue(PWMA_INST, value, DL_TIMER_CC_0_INDEX); }

/* ============ Motor B helpers ============ */

static inline void motorB_BIN1_HIGH(void)
    { DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN1_PIN); }
static inline void motorB_BIN1_LOW(void)
    { DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN1_PIN); }
static inline void motorB_BIN2_HIGH(void)
    { DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN2_PIN); }
static inline void motorB_BIN2_LOW(void)
    { DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN2_PIN); }
static inline void motorB_SetPWM(uint16_t value)
    { DL_TimerG_setCaptureCompareValue(PWMB_INST, value, DL_TIMER_CC_0_INDEX); }

/* ============ Static: set speed for one motor ============ */

static void motor_SetSpeedRaw(uint8_t motor,
    void (*in1_h)(void), void (*in1_l)(void),
    void (*in2_h)(void), void (*in2_l)(void),
    void (*set_pwm)(uint16_t),
    int16_t speed)
{
    if (speed > 100)  speed = 100;
    if (speed < -100) speed = -100;

    if (speed > 0)
    {
        in1_l(); in2_h();
        set_pwm((uint16_t)((uint32_t)speed * MOTOR_PWM_PERIOD / 100));
    }
    else if (speed < 0)
    {
        in1_h(); in2_l();
        set_pwm((uint16_t)((uint32_t)(-speed) * MOTOR_PWM_PERIOD / 100));
    }
    else
    {
        in1_l(); in2_l();
        set_pwm(0);
    }
}

/* ============ Motor A API ============ */

void MotorA_Init(void)
{
    motorA_AIN1_LOW();
    motorA_AIN2_LOW();
    motorA_SetPWM(0);
}

void MotorA_SetSpeed(int16_t speed) {
    motor_SetSpeedRaw('A',
        motorA_AIN1_HIGH, motorA_AIN1_LOW,
        motorA_AIN2_HIGH, motorA_AIN2_LOW,
        motorA_SetPWM, speed);
}

void MotorA_Forward(uint16_t speed) {
    if (speed > 100) speed = 100;
    motorA_AIN1_LOW(); motorA_AIN2_HIGH();
    motorA_SetPWM((uint16_t)((uint32_t)speed * MOTOR_PWM_PERIOD / 100));
}

void MotorA_Reverse(uint16_t speed) {
    if (speed > 100) speed = 100;
    motorA_AIN1_HIGH(); motorA_AIN2_LOW();
    motorA_SetPWM((uint16_t)((uint32_t)speed * MOTOR_PWM_PERIOD / 100));
}

void MotorA_Brake(void) {
    motorA_AIN1_HIGH(); motorA_AIN2_HIGH();
    motorA_SetPWM(0);
}

void MotorA_Stop(void) {
    motorA_AIN1_LOW(); motorA_AIN2_LOW();
    motorA_SetPWM(0);
}

/* ============ Motor B API ============ */

void MotorB_Init(void)
{
    motorB_BIN1_LOW();
    motorB_BIN2_LOW();
    motorB_SetPWM(0);
}

void MotorB_SetSpeed(int16_t speed) {
    motor_SetSpeedRaw('B',
        motorB_BIN1_HIGH, motorB_BIN1_LOW,
        motorB_BIN2_HIGH, motorB_BIN2_LOW,
        motorB_SetPWM, speed);
}

void MotorB_Forward(uint16_t speed) {
    if (speed > 100) speed = 100;
    motorB_BIN1_LOW(); motorB_BIN2_HIGH();
    motorB_SetPWM((uint16_t)((uint32_t)speed * MOTOR_PWM_PERIOD / 100));
}

void MotorB_Reverse(uint16_t speed) {
    if (speed > 100) speed = 100;
    motorB_BIN1_HIGH(); motorB_BIN2_LOW();
    motorB_SetPWM((uint16_t)((uint32_t)speed * MOTOR_PWM_PERIOD / 100));
}

void MotorB_Brake(void) {
    motorB_BIN1_HIGH(); motorB_BIN2_HIGH();
    motorB_SetPWM(0);
}

void MotorB_Stop(void) {
    motorB_BIN1_LOW(); motorB_BIN2_LOW();
    motorB_SetPWM(0);
}

/* ============ Common API ============ */

void Motor_Init(void)
{
    /* 先使能TB6612 */
    DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_STBY_PIN);

    /* 初始化两个电机的方向和占空比 */
    MotorA_Init();
    MotorB_Init();

    /* 启动两个PWM定时器 */
    DL_TimerG_startCounter(PWMA_INST);
    DL_TimerG_startCounter(PWMB_INST);
}

void Motor_Standby(void)
{
    MotorA_Stop();
    MotorB_Stop();
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_STBY_PIN); /* STBY = 0 */
}
