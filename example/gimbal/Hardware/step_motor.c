#include "step_motor.h"
#include "bsp_systick.h"

/*
 * DCC-100v3 双步进电机控制 - 基于定时器生成 STEP 脉冲
 *
 * 步进电机 1：PA0(STEP=TIMA0_CCP0)  PA1(DIR)  PA7(DCY)  PA8(SLP)  PA9(RST)
 * 步进电机 2：PA15(STEP=TIMA1_CCP0) PA13(DIR) PA14(DCY) PA12(SLP) PA16(RST)
 *
 * STEP 脉冲由定时器 PWM 以 50% 占空比输出。
 * 中断服务函数统计剩余脉冲，完成后停止定时器（非阻塞）。
 */

/* ---- 每个电机的运行状态 ---- */
typedef struct {
    uint32_t         remain; /* 剩余步数（在中断中递减） */
    volatile uint8_t done;   /* 1 = 已完成 */
} stepper_ctx_t;

static stepper_ctx_t ctx1, ctx2;

static stepper_ctx_t *ctx_of(uint8_t id)
{
    return (id == STEPPER_1) ? &ctx1 : &ctx2;
}

/* ============ 引脚辅助 ============ */

#define S1_PORT  STEPPER1_PORT
#define S1_DIR   STEPPER1_DIR1_PIN
#define S1_DCY   STEPPER1_DCY1_PIN
#define S1_SLP   STEPPER1_SLP1_PIN
#define S1_RST   STEPPER1_RST1_PIN

#define S2_PORT  STEPPER2_PORT
#define S2_DIR   STEPPER2_DIR2_PIN
#define S2_DCY   STEPPER2_DCY2_PIN
#define S2_SLP   STEPPER2_SLP2_PIN
#define S2_RST   STEPPER2_RST2_PIN

static void timer_set_cc(uint8_t id, uint16_t val)
{
    DL_TimerA_setCaptureCompareValue(
        (id == STEPPER_1) ? STEPPER1_PWM_INST : STEPPER2_PWM_INST,
        val, DL_TIMER_CC_0_INDEX);
}

/* ============ 公共接口 ============ */

void Stepper_Init(void)
{
    ctx1.remain = 0;
    ctx1.done   = 1;
    ctx2.remain = 0;
    ctx2.done   = 1;

    /* RST=1, SLP=1, DCY=1, DIR=0（GPIO 初始化时已将 RST/SLP/DCY 置为高） */
    DL_GPIO_clearPins(S1_PORT, S1_DIR);
    DL_GPIO_clearPins(S2_PORT, S2_DIR);

    /* 开启 LOAD 中断用于步数统计（SYSCFG 的定时器初始化中也已使能） */
    DL_TimerA_enableInterrupt(STEPPER1_PWM_INST, DL_TIMER_INTERRUPT_LOAD_EVENT);
    DL_TimerA_enableInterrupt(STEPPER2_PWM_INST, DL_TIMER_INTERRUPT_LOAD_EVENT);
    NVIC_EnableIRQ(STEPPER1_PWM_INST_INT_IRQN);
    NVIC_EnableIRQ(STEPPER2_PWM_INST_INT_IRQN);
}

void Stepper_SetDir(uint8_t id, uint8_t dir)
{
    if (id == STEPPER_1) {
        if (dir == DIR_CW)
            DL_GPIO_clearPins(S1_PORT, S1_DIR);
        else
            DL_GPIO_setPins(S1_PORT, S1_DIR);
    } else {
        if (dir == DIR_CW)
            DL_GPIO_clearPins(S2_PORT, S2_DIR);
        else
            DL_GPIO_setPins(S2_PORT, S2_DIR);
    }
}

/*
 * 设置步进电机速度，单位为 deg/s（0 = 停止）。
 * 单脉冲分辨率 = 0.05625 deg（6400 脉冲/转）。
 *
 * 定时器时钟 = 80 MHz。选择合适的预分频，使周期落在 16 位范围内。
 *   speed <= 30  → 预分频 15（5 MHz）
 *   speed <= 120 → 预分频 3 （20 MHz）
 *   speed > 120  → 预分频 0 （80 MHz）
 */
static void timer_set_freq(GPTIMER_Regs *tim, uint8_t id, uint32_t clk_freq,
                           uint32_t target_hz)
{
    uint32_t period = clk_freq / target_hz;
    if (period > 65535)
        period = 65535;
    if (period < 800)
        period = 800;

    DL_Timer_setLoadValue(tim, period);
    /* 50% 占空比 */
    timer_set_cc(id, (uint16_t)(period / 2));
}

void Stepper_SetSpeed(uint8_t id, uint16_t deg_per_sec)
{
    if (deg_per_sec == 0) {
        Stepper_Stop(id);
        return;
    }

    float    pulse_hz = (float)deg_per_sec / STEPPER_DEG_PER_PULSE;
    uint32_t target   = (uint32_t)pulse_hz;
    if (target < 10)
        target = 10;

    GPTIMER_Regs *tim = (id == STEPPER_1) ? STEPPER1_PWM_INST
                                          : STEPPER2_PWM_INST;

    if (deg_per_sec <= 30) {
        DL_TimerA_ClockConfig clk = {
            .clockSel    = DL_TIMER_CLOCK_BUSCLK,
            .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
            .prescale    = 15U /* 预分频为 16，得到 5 MHz */
        };
        DL_TimerA_setClockConfig(tim, &clk);
        timer_set_freq(tim, id, 5000000, target);
    } else if (deg_per_sec <= 120) {
        DL_TimerA_ClockConfig clk = {
            .clockSel    = DL_TIMER_CLOCK_BUSCLK,
            .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
            .prescale    = 3U /* 预分频为 4，得到 20 MHz */
        };
        DL_TimerA_setClockConfig(tim, &clk);
        timer_set_freq(tim, id, 20000000, target);
    } else {
        DL_TimerA_ClockConfig clk = {
            .clockSel    = DL_TIMER_CLOCK_BUSCLK,
            .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
            .prescale    = 0U /* 预分频为 1，得到 80 MHz */
        };
        DL_TimerA_setClockConfig(tim, &clk);
        timer_set_freq(tim, id, 80000000, target);
    }
}

void Stepper_Start(uint8_t id)
{
    ctx_of(id)->done = 0;
    DL_Timer_startCounter(
        (id == STEPPER_1) ? STEPPER1_PWM_INST : STEPPER2_PWM_INST);
}

void Stepper_Stop(uint8_t id)
{
    DL_Timer_stopCounter(
        (id == STEPPER_1) ? STEPPER1_PWM_INST : STEPPER2_PWM_INST);
    ctx_of(id)->done   = 1;
    ctx_of(id)->remain = 0;
}

void Stepper_SetPulses(uint8_t id, uint32_t pulses)
{
    ctx_of(id)->remain = pulses;
    ctx_of(id)->done   = 0;
    Stepper_Start(id);
}

void Stepper_SetAngle(uint8_t id, uint16_t angle_deg)
{
    uint32_t pulses = (uint32_t)((float)angle_deg / STEPPER_DEG_PER_PULSE);
    Stepper_SetPulses(id, pulses);
}

uint8_t Stepper_IsDone(uint8_t id)
{
    return ctx_of(id)->done;
}

/* ---- 阻塞式辅助接口 ---- */

void Stepper_RunPulses(uint8_t id, uint32_t pulses, uint8_t dir,
                       uint16_t deg_per_sec)
{
    Stepper_SetDir(id, dir);
    Stepper_SetSpeed(id, deg_per_sec);
    Stepper_SetPulses(id, pulses);
    while (!Stepper_IsDone(id)) { /* 等待 */ }
}

void Stepper_RunAngle(uint8_t id, uint16_t angle_deg, uint8_t dir,
                      uint16_t deg_per_sec)
{
    Stepper_SetDir(id, dir);
    Stepper_SetSpeed(id, deg_per_sec);
    Stepper_SetAngle(id, angle_deg);
    while (!Stepper_IsDone(id)) { /* 等待 */ }
}

/* ---- SLP / RST 控制 ---- */

void Stepper_Enable(uint8_t id)
{
    if (id == STEPPER_1)
        DL_GPIO_setPins(S1_PORT, S1_SLP);
    else
        DL_GPIO_setPins(S2_PORT, S2_SLP);
}

void Stepper_Disable(uint8_t id)
{
    if (id == STEPPER_1)
        DL_GPIO_clearPins(S1_PORT, S1_SLP);
    else
        DL_GPIO_clearPins(S2_PORT, S2_SLP);
}

void Stepper_Reset(uint8_t id)
{
    if (id == STEPPER_1) {
        DL_GPIO_clearPins(S1_PORT, S1_RST);
        delay_us(10);
        DL_GPIO_setPins(S1_PORT, S1_RST);
    } else {
        DL_GPIO_clearPins(S2_PORT, S2_RST);
        delay_us(10);
        DL_GPIO_setPins(S2_PORT, S2_RST);
    }
}

/* ============ 定时器中断服务函数 ============ */

void STEPPER1_PWM_INST_IRQHandler(void)
{
    switch (DL_Timer_getPendingInterrupt(STEPPER1_PWM_INST)) {
    case DL_TIMER_IIDX_LOAD:
        if (ctx1.remain == 0) {
            DL_Timer_stopCounter(STEPPER1_PWM_INST);
            ctx1.done = 1;
            break;
        }
        ctx1.remain--;
        break;
    default:
        break;
    }
}

void STEPPER2_PWM_INST_IRQHandler(void)
{
    switch (DL_Timer_getPendingInterrupt(STEPPER2_PWM_INST)) {
    case DL_TIMER_IIDX_LOAD:
        if (ctx2.remain == 0) {
            DL_Timer_stopCounter(STEPPER2_PWM_INST);
            ctx2.done = 1;
            break;
        }
        ctx2.remain--;
        break;
    default:
        break;
    }
}
