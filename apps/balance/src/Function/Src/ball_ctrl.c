/**
 * @file ball_ctrl.c
 * @brief Cascade ball-beam: pos PI -> v_des; vel PD+FF -> rod
 */
#include "ball_ctrl.h"
#include "ball_ctrl_cfg.h"
#include "ball_proto.h"
#include "vision_uart.h"
#include "stepper.h"

#ifndef BALL_CTRL_LINK_TIMEOUT_MS
#define BALL_CTRL_LINK_TIMEOUT_MS   (150u)
#endif

static bool              s_en;
static ball_ctrl_state_t s_state;
static int32_t           s_target_mm_x100;
static int32_t           s_ball_mm_x100;
static int32_t           s_rod_mm_x100;
static float             s_ball_f;
static float             s_vel_mm_s;
static float             s_rod_f;
static float             s_i_pos;
static float             s_v_des;
static bool              s_have_ball;
static uint32_t          s_ms_accum;
static uint32_t          s_last_frame_ms;
static uint32_t          s_ms_total;
static bool              s_rod_applied;
static uint32_t          s_settle_ms;
static uint32_t          s_lost_ms;
static bool              s_coils_off;

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void coils_on(void)
{
    if (s_coils_off || !Stepper_IsEnabled()) {
        Stepper_SetEnable(true);
        s_coils_off = false;
    }
}

static void coils_off(void)
{
    Stepper_Stop();
    Stepper_SetEnable(false);
    s_coils_off = true;
}

static void reset_integrators(void)
{
    s_i_pos = 0.0f;
    s_v_des = 0.0f;
}

void BallCtrl_Init(void)
{
    s_en = false;
    s_state = BALL_CTRL_STATE_IDLE;
    s_target_mm_x100 = BALL_CTRL_DEFAULT_TARGET_MM_X100;
    s_ball_mm_x100 = 0;
    s_rod_mm_x100 = 0;
    s_ball_f = 0.0f;
    s_vel_mm_s = 0.0f;
    s_rod_f = 0.0f;
    reset_integrators();
    s_have_ball = false;
    s_ms_accum = 0;
    s_last_frame_ms = 0;
    s_ms_total = 0;
    s_rod_applied = false;
    s_settle_ms = 0;
    s_lost_ms = 0;
    s_coils_off = true;

    Stepper_SetSpeedSps(BALL_CTRL_STEPPER_SPS);
    Stepper_SetAccel(BALL_CTRL_STEPPER_ACCEL);
}

void BallCtrl_Enable(bool on)
{
    s_en = on;
    if (on) {
        coils_on();
        s_state = BALL_CTRL_STATE_RUN;
        s_ms_accum = 0;
        s_rod_applied = false;
        s_settle_ms = 0;
        s_lost_ms = 0;
        reset_integrators();
    } else {
        s_state = BALL_CTRL_STATE_IDLE;
        s_rod_f = 0.0f;
        s_rod_mm_x100 = 0;
        s_rod_applied = false;
        s_settle_ms = 0;
        reset_integrators();
        coils_off();
    }
}

bool BallCtrl_IsEnabled(void)
{
    return s_en;
}

void BallCtrl_SetTargetMm_x100(int32_t mm_x100)
{
    s_target_mm_x100 = clamp_i32(
        mm_x100,
        (int32_t)BALL_CTRL_TARGET_MIN_MM_X100,
        (int32_t)BALL_CTRL_TARGET_MAX_MM_X100);
    s_settle_ms = 0;
    reset_integrators();
    if (s_en)
        coils_on();
}

int32_t BallCtrl_GetTargetMm_x100(void)
{
    return s_target_mm_x100;
}

void BallCtrl_SetTargetMm(int16_t pos_mm)
{
    BallCtrl_SetTargetMm_x100(ball_pos_to_mm_x100(pos_mm));
}

void BallCtrl_OnMsTick(void)
{
    if (s_ms_accum < 100000u)
        s_ms_accum++;
    s_ms_total++;
    if (s_state == BALL_CTRL_STATE_SETTLED && s_settle_ms < 1000000u)
        s_settle_ms++;
    if (s_state == BALL_CTRL_STATE_LOST && s_lost_ms < 1000000u)
        s_lost_ms++;
}

static void apply_rod(int32_t rod_mm_x100)
{
    int32_t prev = s_rod_mm_x100;
    int32_t d;

    rod_mm_x100 = clamp_i32(
        rod_mm_x100,
        -(int32_t)BALL_CTRL_ROD_MAX_MM_X100,
        (int32_t)BALL_CTRL_ROD_MAX_MM_X100);

    if (s_rod_applied) {
        d = rod_mm_x100 - prev;
        if (d > (int32_t)BALL_CTRL_ROD_SLEW_MM_X100)
            rod_mm_x100 = prev + (int32_t)BALL_CTRL_ROD_SLEW_MM_X100;
        else if (d < -(int32_t)BALL_CTRL_ROD_SLEW_MM_X100)
            rod_mm_x100 = prev - (int32_t)BALL_CTRL_ROD_SLEW_MM_X100;
    }

    if (s_rod_applied) {
        d = rod_mm_x100 - prev;
        if (d < 0)
            d = -d;
        if (d < (int32_t)BALL_CTRL_ROD_EPS_MM_X100)
            return;
    }

    coils_on();
    s_rod_mm_x100 = rod_mm_x100;
    s_rod_applied = true;
    Stepper_SetTargetMm_x100(s_rod_mm_x100);
}

static void on_lost(void)
{
    if (s_state != BALL_CTRL_STATE_LOST)
        s_lost_ms = 0;
    s_state = BALL_CTRL_STATE_LOST;
    s_have_ball = false;
    s_vel_mm_s = 0.0f;
    s_settle_ms = 0;
    reset_integrators();
#if BALL_CTRL_HOLD_LEVEL_ON_LOSS
    s_rod_f = 0.0f;
    apply_rod(0);
#endif
}

static void control_step(float dt_s)
{
    float e_mm;
    float abs_e;
    float v_des;
    float ev;
    float rod_mm;
    float rod_unsat;
    float out_a = BALL_CTRL_OUT_ALPHA;
    int32_t err_x100;
    int32_t abs_err;
    float rod_max = (float)BALL_CTRL_ROD_MAX_MM_X100 / 100.0f;

    if (dt_s < 1e-4f)
        dt_s = 1e-4f;

    err_x100 = s_target_mm_x100 - s_ball_mm_x100;
    abs_err = err_x100 >= 0 ? err_x100 : -err_x100;
    e_mm = (float)err_x100 / 100.0f;
    abs_e = e_mm >= 0.0f ? e_mm : -e_mm;

    /* settled: near target and slow */
    if ((abs_err < (int32_t)BALL_CTRL_DEAD_MM_X100) &&
        (s_vel_mm_s < BALL_CTRL_VEL_DEAD_MM_S) &&
        (s_vel_mm_s > -BALL_CTRL_VEL_DEAD_MM_S)) {
        s_state = BALL_CTRL_STATE_SETTLED;
        reset_integrators();
        s_rod_f *= 0.65f;
        if (s_rod_f > -0.08f && s_rod_f < 0.08f)
            s_rod_f = 0.0f;
        apply_rod((int32_t)(s_rod_f * 100.0f));

        if (s_rod_mm_x100 > -(int32_t)BALL_CTRL_ROD_EPS_MM_X100 &&
            s_rod_mm_x100 < (int32_t)BALL_CTRL_ROD_EPS_MM_X100 &&
            s_settle_ms >= (uint32_t)BALL_CTRL_SETTLE_DISABLE_MS) {
            if (!s_coils_off)
                coils_off();
        }
        return;
    }

    s_state = BALL_CTRL_STATE_RUN;
    s_settle_ms = 0;
    coils_on();

    /* ===== outer: position PI -> v_des ===== */
    if (abs_e < BALL_CTRL_I_SEP_MM)
        s_i_pos += e_mm * dt_s;
    else
        s_i_pos *= 0.95f; /* bleed when far */

    s_i_pos = clampf(s_i_pos, -BALL_CTRL_I_LIM, BALL_CTRL_I_LIM);

    v_des = BALL_CTRL_KP_POS * e_mm + BALL_CTRL_KI_POS * s_i_pos;
    v_des = clampf(v_des, -BALL_CTRL_V_DES_MAX, BALL_CTRL_V_DES_MAX);
    s_v_des = v_des;

    /* ===== inner: velocity PD + FF -> rod ===== */
    ev = v_des - s_vel_mm_s;
    rod_unsat =
        BALL_CTRL_SIGN * (
            BALL_CTRL_KP_VEL * ev
          + BALL_CTRL_KFF_VEL * v_des
          + BALL_CTRL_KD_VEL * (-s_vel_mm_s)
        );

    rod_mm = clampf(rod_unsat, -rod_max, rod_max);

    /* anti-windup: if rod saturated and I pushing further, freeze I */
    if ((rod_mm >= rod_max - 1e-3f && e_mm > 0.0f) ||
        (rod_mm <= -rod_max + 1e-3f && e_mm < 0.0f)) {
        /* freeze: undo last integrate roughly */
        if (abs_e < BALL_CTRL_I_SEP_MM)
            s_i_pos -= e_mm * dt_s * 0.5f;
    }

    s_rod_f = s_rod_f + out_a * (rod_mm - s_rod_f);
    apply_rod((int32_t)(s_rod_f * 100.0f));
}

void BallCtrl_Update(void)
{
    ball_frame_t fr;
    ball_setpoint_cmd_t sp;
    float dt_s;
    float new_f;
    float inst_v;
    float prev_ball_f;
    uint32_t now;
    uint32_t dms;

    if (VisionUart_TakeSetpoint(&sp) && sp.valid)
        BallCtrl_SetTargetMm(sp.target_mm);

    if (!s_en) {
        s_state = BALL_CTRL_STATE_IDLE;
        return;
    }

    if (s_state == BALL_CTRL_STATE_LOST &&
        s_lost_ms >= (uint32_t)BALL_CTRL_LOST_DISABLE_MS &&
        !s_coils_off) {
        coils_off();
    }

    if (VisionUart_TakeBallFrame(&fr)) {
        if (ball_frame_usable(&fr)) {
            new_f = (float)ball_pos_to_mm_x100(fr.pos_mm);
            now = s_ms_total;
            if (s_have_ball) {
                dms = now - s_last_frame_ms;
                if (dms < 5u)
                    dms = 5u;
                if (dms > 200u)
                    dms = 200u;
                dt_s = (float)dms / 1000.0f;
                prev_ball_f = s_ball_f;
                s_ball_f = s_ball_f +
                    BALL_CTRL_POS_ALPHA * (new_f - s_ball_f);
                inst_v = ((s_ball_f - prev_ball_f) / 100.0f) / dt_s;
                inst_v = clampf(inst_v, -300.0f, 300.0f);
                s_vel_mm_s = s_vel_mm_s +
                    BALL_CTRL_VEL_ALPHA * (inst_v - s_vel_mm_s);
            } else {
                s_ball_f = new_f;
                s_vel_mm_s = 0.0f;
            }
            s_last_frame_ms = now;
            s_ball_mm_x100 = (int32_t)s_ball_f;
            s_have_ball = true;
            if (s_state == BALL_CTRL_STATE_LOST) {
                s_state = BALL_CTRL_STATE_RUN;
                s_lost_ms = 0;
                reset_integrators();
                coils_on();
            }
        } else {
            on_lost();
        }
    }

    if (!VisionUart_BallLinkOk() ||
        VisionUart_MsSinceBall() > (uint32_t)BALL_CTRL_LINK_TIMEOUT_MS) {
        if (s_have_ball || s_state == BALL_CTRL_STATE_RUN ||
            s_state == BALL_CTRL_STATE_SETTLED)
            on_lost();
        return;
    }

    if (!s_have_ball)
    {
        if (s_state == BALL_CTRL_STATE_LOST && !s_coils_off &&
            s_ms_accum >= BALL_CTRL_DT_MS) {
            s_ms_accum = 0;
            s_rod_f = 0.0f;
            apply_rod(0);
        }
        return;
    }

    if (s_ms_accum < BALL_CTRL_DT_MS)
        return;
    s_ms_accum = 0;
    control_step((float)BALL_CTRL_DT_MS / 1000.0f);
}

ball_ctrl_state_t BallCtrl_GetState(void)
{
    return s_state;
}

int32_t BallCtrl_GetBallMm_x100(void)
{
    return s_ball_mm_x100;
}

int32_t BallCtrl_GetRodMm_x100(void)
{
    return s_rod_mm_x100;
}

bool BallCtrl_IsSettled(void)
{
    return s_state == BALL_CTRL_STATE_SETTLED;
}