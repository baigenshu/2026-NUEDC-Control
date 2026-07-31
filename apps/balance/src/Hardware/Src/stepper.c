/**
 * @file stepper.c
 * @brief TMC_A open-loop stepper with trapezoid accel (crank-linkage boom)
 *
 * Tick: STEP_TIM 10 us. Each microstep = HIGH then LOW phase.
 * Mechanical: motor crank → linkage → boom tilt (not leadscrew).
 * Speed profile: start at START_SPS, ramp to cruise, decelerate near target
 * and on reverse to cut lost-steps at high-frequency direction changes.
 */
#include "stepper.h"
#include "stepper_cfg.h"
#include "ti_msp_dl_config.h"

typedef enum {
    STEP_PHASE_IDLE = 0,
    STEP_PHASE_HIGH,
    STEP_PHASE_LOW,
} step_phase_t;

static volatile int32_t  s_pos;
static volatile int32_t  s_target;
static volatile uint32_t s_cruise_sps;
static volatile uint32_t s_cur_sps;
static volatile uint32_t s_accel;
static volatile uint32_t s_period_ticks;
static volatile uint32_t s_tick_cnt;
static volatile uint32_t s_pulse_count;
static volatile uint32_t s_sps_accum; /* for accel integration per step */
static volatile step_phase_t s_phase;
static volatile bool     s_enabled;
static volatile bool     s_busy;
static volatile int8_t   s_move_dir; /* +1 / -1 mechanical steps */

static uint32_t clampu(uint32_t v, uint32_t lo, uint32_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static void apply_dir_pin(int8_t move_dir)
{
    int pin = (int)move_dir * (int)STEPPER_DIR_SIGN;
    if (pin > 0)
        DL_GPIO_setPins(GPIO_STEPPER_PORT, GPIO_STEPPER_DIR_PIN);
    else
        DL_GPIO_clearPins(GPIO_STEPPER_PORT, GPIO_STEPPER_DIR_PIN);
}

static void apply_period_from_sps(uint32_t sps)
{
    uint32_t ticks;

    sps = clampu(sps, 1u, STEPPER_MAX_SPS);
    if (sps > STEPPER_SPS_HARD_MAX)
        sps = STEPPER_SPS_HARD_MAX;

    /* period_us = 1e6/sps; ticks = period_us / TICK_US */
    ticks = (1000000u / sps) / STEPPER_TICK_US;
    if (ticks < 2u)
        ticks = 2u;
    s_period_ticks = ticks;
    s_cur_sps = sps;
}

static int32_t clamp_soft(int32_t steps)
{
    if (steps < (int32_t)STEPPER_SOFT_MIN_STEPS)
        return (int32_t)STEPPER_SOFT_MIN_STEPS;
    if (steps > (int32_t)STEPPER_SOFT_MAX_STEPS)
        return (int32_t)STEPPER_SOFT_MAX_STEPS;
    return steps;
}

static uint32_t steps_to_stop(uint32_t sps)
{
    /* s = v^2 / (2a) */
    uint64_t v2;
    uint32_t a = s_accel;

    if (a < 1u)
        a = 1u;
    if (sps <= STEPPER_START_SPS)
        return 1u;
    v2 = (uint64_t)sps * (uint64_t)sps;
    return (uint32_t)(v2 / (2ull * (uint64_t)a)) + 1u;
}

static void update_speed_profile(void)
{
    int32_t remain;
    uint32_t need_stop;
    uint32_t sps = s_cur_sps;
    uint32_t cruise = s_cruise_sps;
    uint32_t a = s_accel;

    if (a < 1u)
        a = 1u;

    remain = s_target - s_pos;
    if (remain < 0)
        remain = -remain;

    need_stop = steps_to_stop(sps);

    if ((uint32_t)remain <= need_stop) {
        /* decelerate toward START */
        if (sps > STEPPER_START_SPS) {
            uint32_t dec = a / (sps > 0u ? sps : 1u);
            if (dec < 1u)
                dec = 1u;
            if (sps > STEPPER_START_SPS + dec)
                sps -= dec;
            else
                sps = STEPPER_START_SPS;
        }
    } else if (sps < cruise) {
        uint32_t inc = a / (sps > 0u ? sps : 1u);
        if (inc < 1u)
            inc = 1u;
        if (sps + inc < cruise)
            sps += inc;
        else
            sps = cruise;
    } else if (sps > cruise) {
        uint32_t dec = a / (sps > 0u ? sps : 1u);
        if (dec < 1u)
            dec = 1u;
        if (sps > cruise + dec)
            sps -= dec;
        else
            sps = cruise;
    }

    apply_period_from_sps(sps);
}

static void arm_motion(int32_t target)
{
    int32_t delta;

    target = clamp_soft(target);
    s_target = target;
    delta = s_target - s_pos;

    if (delta == 0) {
        s_busy = false;
        s_phase = STEP_PHASE_IDLE;
        s_move_dir = 0;
        DL_GPIO_clearPins(GPIO_STEPPER_PORT, GPIO_STEPPER_STEP_PIN);
        DL_TimerG_stopCounter(STEP_TIM_INST);
        return;
    }

    /* A running pulse owns DIR; the ISR handles a reversal at a step boundary. */
    if (s_busy && s_move_dir != 0) {
        if ((delta > 0 && s_move_dir > 0) ||
            (delta < 0 && s_move_dir < 0)) {
            update_speed_profile();
        }
        return;
    }

    s_move_dir = (delta > 0) ? 1 : -1;
    apply_dir_pin(s_move_dir);

    /* fresh move: start from low speed unless already spinning same way */
    if (!s_busy || s_cur_sps < STEPPER_START_SPS)
        apply_period_from_sps(STEPPER_START_SPS);
    else
        apply_period_from_sps(s_cur_sps);

    s_tick_cnt = 0;
    s_phase = STEP_PHASE_HIGH;
    s_busy = true;

    if (!s_enabled)
        Stepper_SetEnable(true);

    DL_TimerG_startCounter(STEP_TIM_INST);
}

/* tilt_x100: 0.01 abstract unit → microsteps */
static int32_t tilt_x100_to_steps(int32_t tilt_x100)
{
    int64_t num = (int64_t)tilt_x100 * (int64_t)STEPPER_STEPS_PER_UNIT;
    if (num >= 0)
        return (int32_t)((num + 50) / 100);
    return (int32_t)((num - 50) / 100);
}

static int32_t steps_to_tilt_x100(int32_t steps)
{
    int64_t num = (int64_t)steps * 100;
    if (num >= 0)
        return (int32_t)((num + (int64_t)STEPPER_STEPS_PER_UNIT / 2) /
                         (int64_t)STEPPER_STEPS_PER_UNIT);
    return (int32_t)((num - (int64_t)STEPPER_STEPS_PER_UNIT / 2) /
                     (int64_t)STEPPER_STEPS_PER_UNIT);
}

static int32_t deg_x100_to_steps(int32_t deg_x100)
{
    /* 1 microstep ≈ STEPPER_MOTOR_DEG_X100_PER_STEP (0.01°) */
    if (deg_x100 >= 0)
        return (int32_t)((deg_x100 + STEPPER_MOTOR_DEG_X100_PER_STEP / 2) /
                         STEPPER_MOTOR_DEG_X100_PER_STEP);
    return (int32_t)((deg_x100 - STEPPER_MOTOR_DEG_X100_PER_STEP / 2) /
                     STEPPER_MOTOR_DEG_X100_PER_STEP);
}

static int32_t steps_to_deg_x100(int32_t steps)
{
    return steps * STEPPER_MOTOR_DEG_X100_PER_STEP;
}

void Stepper_Init(void)
{
    s_pos = 0;
    s_target = 0;
    s_cruise_sps = STEPPER_DEFAULT_SPS;
    s_accel = STEPPER_ACCEL_SPS2;
    s_phase = STEP_PHASE_IDLE;
    s_busy = false;
    s_enabled = false;
    s_tick_cnt = 0;
    s_pulse_count = 0;
    s_move_dir = 0;
    apply_period_from_sps(STEPPER_START_SPS);

    DL_GPIO_clearPins(GPIO_STEPPER_PORT, GPIO_STEPPER_STEP_PIN | GPIO_STEPPER_DIR_PIN);
    DL_GPIO_setPins(GPIO_STEPPER_PORT, GPIO_STEPPER_EN_PIN);

    NVIC_EnableIRQ(STEP_TIM_INST_INT_IRQN);
}

void Stepper_SetEnable(bool on)
{
    s_enabled = on;
#if STEPPER_EN_ACTIVE_LOW
    if (on)
        DL_GPIO_clearPins(GPIO_STEPPER_PORT, GPIO_STEPPER_EN_PIN);
    else
        DL_GPIO_setPins(GPIO_STEPPER_PORT, GPIO_STEPPER_EN_PIN);
#else
    if (on)
        DL_GPIO_setPins(GPIO_STEPPER_PORT, GPIO_STEPPER_EN_PIN);
    else
        DL_GPIO_clearPins(GPIO_STEPPER_PORT, GPIO_STEPPER_EN_PIN);
#endif
}

bool Stepper_IsEnabled(void)
{
    return s_enabled;
}

void Stepper_Stop(void)
{
    s_target = s_pos;
    s_busy = false;
    s_phase = STEP_PHASE_IDLE;
    s_move_dir = 0;
    DL_GPIO_clearPins(GPIO_STEPPER_PORT, GPIO_STEPPER_STEP_PIN);
    DL_TimerG_stopCounter(STEP_TIM_INST);
}

void Stepper_EmergencyStop(void)
{
    Stepper_Stop();
    Stepper_SetEnable(false);
    apply_period_from_sps(STEPPER_START_SPS);
}

void Stepper_MoveSteps(int32_t delta_steps)
{
    arm_motion(s_pos + delta_steps);
}

void Stepper_SetTargetSteps(int32_t target_steps)
{
    arm_motion(target_steps);
}

void Stepper_SetZero(void)
{
    s_pos = 0;
    s_target = 0;
}

int32_t Stepper_GetPositionSteps(void)
{
    return s_pos;
}

int32_t Stepper_GetTargetSteps(void)
{
    return s_target;
}

bool Stepper_IsBusy(void)
{
    return s_busy;
}

void Stepper_SetSpeedSps(uint32_t sps)
{
    s_cruise_sps = clampu(sps, 1u, STEPPER_MAX_SPS);
}

uint32_t Stepper_GetSpeedSps(void)
{
    return s_cruise_sps;
}

uint32_t Stepper_GetCurrentSps(void)
{
    return s_cur_sps;
}

void Stepper_SetAccel(uint32_t sps2)
{
    if (sps2 < 1u)
        sps2 = 1u;
    s_accel = sps2;
}

uint32_t Stepper_GetAccel(void)
{
    return s_accel;
}

void Stepper_MoveDeg_x100(int32_t delta_deg_x100)
{
    Stepper_MoveSteps(deg_x100_to_steps(delta_deg_x100));
}

void Stepper_SetTargetDeg_x100(int32_t target_deg_x100)
{
    Stepper_SetTargetSteps(deg_x100_to_steps(target_deg_x100));
}

int32_t Stepper_GetPositionDeg_x100(void)
{
    return steps_to_deg_x100(s_pos);
}

void Stepper_MoveMm_x100(int32_t delta_mm_x100)
{
    Stepper_MoveSteps(tilt_x100_to_steps(delta_mm_x100));
}

void Stepper_SetTargetMm_x100(int32_t target_mm_x100)
{
    Stepper_SetTargetSteps(tilt_x100_to_steps(target_mm_x100));
}

int32_t Stepper_GetPositionMm_x100(void)
{
    return steps_to_tilt_x100(s_pos);
}

/* 兼容旧 um API：按 0.01 unit * 10 解释 */
void Stepper_MoveUm(int32_t delta_um)
{
    Stepper_MoveMm_x100(delta_um / 10);
}

void Stepper_SetTargetUm(int32_t target_um)
{
    Stepper_SetTargetMm_x100(target_um / 10);
}

int32_t Stepper_GetPositionUm(void)
{
    return Stepper_GetPositionMm_x100() * 10;
}

uint32_t Stepper_GetPulseCount(void)
{
    return s_pulse_count;
}

void Stepper_ClearPulseCount(void)
{
    s_pulse_count = 0;
}

void Stepper_OnTimerTick(void)
{
    uint32_t half;

    if (s_phase == STEP_PHASE_IDLE || !s_busy)
        return;

    half = s_period_ticks / 2u;
    if (half < 1u)
        half = 1u;

    s_tick_cnt++;

    if (s_phase == STEP_PHASE_HIGH) {
        if (s_tick_cnt == 1u)
            DL_GPIO_setPins(GPIO_STEPPER_PORT, GPIO_STEPPER_STEP_PIN);
        if (s_tick_cnt >= half) {
            DL_GPIO_clearPins(GPIO_STEPPER_PORT, GPIO_STEPPER_STEP_PIN);
            s_phase = STEP_PHASE_LOW;
        }
        return;
    }

    if (s_tick_cnt >= s_period_ticks) {
        s_tick_cnt = 0;
        s_pulse_count++;

        if (s_move_dir > 0)
            s_pos++;
        else if (s_move_dir < 0)
            s_pos--;

        if (s_pos == s_target) {
            s_busy = false;
            s_phase = STEP_PHASE_IDLE;
            s_move_dir = 0;
            DL_GPIO_clearPins(GPIO_STEPPER_PORT, GPIO_STEPPER_STEP_PIN);
            apply_period_from_sps(STEPPER_START_SPS);
            DL_TimerG_stopCounter(STEP_TIM_INST);
            return;
        }

        /* Keep DIR consistent if target flipped mid-move. */
        {
            int8_t nd = (s_target > s_pos) ? 1 : -1;
            if (nd != s_move_dir) {
                /* Reverse only after reaching the low-speed floor. */
                if (s_cur_sps > STEPPER_START_SPS) {
                    uint32_t dec = s_accel / (s_cur_sps > 0u ? s_cur_sps : 1u);
                    if (dec < 1u)
                        dec = 1u;
                    if (s_cur_sps > STEPPER_START_SPS + dec)
                        apply_period_from_sps(s_cur_sps - dec);
                    else
                        apply_period_from_sps(STEPPER_START_SPS);
                    s_phase = STEP_PHASE_HIGH;
                    return;
                }
                s_move_dir = nd;
                apply_dir_pin(s_move_dir);
            }
        }

        update_speed_profile();
        s_phase = STEP_PHASE_HIGH;
    }
}

void STEP_TIM_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(STEP_TIM_INST)) {
    case DL_TIMERG_IIDX_ZERO:
        Stepper_OnTimerTick();
        break;
    default:
        break;
    }
}