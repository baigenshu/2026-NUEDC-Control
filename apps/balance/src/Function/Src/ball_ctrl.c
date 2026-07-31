/**
 * @file ball_ctrl.c
 * @brief Pure visual position PID controller for the crank-linkage boom.
 */
#include "ball_ctrl.h"
#include "ball_ctrl_cfg.h"
#include "vision_uart.h"
#include "stepper.h"

static bool              s_en;
static ball_ctrl_state_t s_state;
static int32_t           s_target_mm_x100;
static int32_t           s_ball_mm_x100;
static int32_t           s_rod_mm_x100;
static float             s_ball_f;
static float             s_vel_mm_s;
static float             s_integral;
static float             s_prev_error_mm;
static uint8_t            s_settled_frames;
static bool               s_have_ball;
static uint32_t          s_last_frame_ms;
static uint32_t          s_ms_total;
static uint32_t          s_lost_ms;
static uint8_t            s_bad_frames;
static bool               s_rod_applied;
static bool               s_coils_off;

static int32_t clamp_i32(int32_t value, int32_t lower, int32_t upper)
{
    if (value < lower)
        return lower;
    if (value > upper)
        return upper;
    return value;
}

static float clampf(float value, float lower, float upper)
{
    if (value < lower)
        return lower;
    if (value > upper)
        return upper;
    return value;
}

static float absf(float value)
{
    return value < 0.0f ? -value : value;
}

static void reset_dynamic_state(void)
{
    s_ball_f = 0.0f;
    s_vel_mm_s = 0.0f;
    s_integral = 0.0f;
    s_prev_error_mm = 0.0f;
    s_settled_frames = 0u;
    s_have_ball = false;
    s_last_frame_ms = 0u;
    s_bad_frames = 0u;
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

static void apply_rod(int32_t rod_mm_x100)
{
    int32_t previous = s_rod_mm_x100;
    int32_t delta;
    int32_t slew = (int32_t)BALL_CTRL_COMMAND_SLEW_MM_X100;

    rod_mm_x100 = clamp_i32(
        rod_mm_x100,
        -(int32_t)BALL_CTRL_OUTPUT_MAX_MM_X100,
        (int32_t)BALL_CTRL_OUTPUT_MAX_MM_X100);

    if (s_rod_applied) {
        delta = rod_mm_x100 - previous;
        if (delta < (int32_t)BALL_CTRL_COMMAND_EPS_MM_X100 &&
            delta > -(int32_t)BALL_CTRL_COMMAND_EPS_MM_X100)
            return;
        if (delta > slew)
            rod_mm_x100 = previous + slew;
        else if (delta < -slew)
            rod_mm_x100 = previous - slew;
        if (rod_mm_x100 == previous)
            return;
    }

    coils_on();
    s_rod_mm_x100 = rod_mm_x100;
    s_rod_applied = true;
    Stepper_SetTargetMm_x100(rod_mm_x100);
}

static void enter_lost(void)
{
    if (s_state != BALL_CTRL_STATE_LOST)
        s_lost_ms = 0u;
    s_state = BALL_CTRL_STATE_LOST;
    s_ball_mm_x100 = 0;
    reset_dynamic_state();
#if BALL_CTRL_HOLD_LEVEL_ON_LOSS
    s_rod_applied = false;
    apply_rod(0);
#endif
}

static void update_measurement(const ball_frame_t *frame, float *dt_s)
{
    float raw_mm;
    float previous_mm;
    float instant_velocity;
    uint32_t frame_dt_ms;

    raw_mm = (float)ball_pos_to_mm_x100(frame->pos_mm) / 100.0f;
    *dt_s = (float)BALL_CTRL_DEFAULT_FRAME_DT_MS / 1000.0f;

    if (!s_have_ball) {
        s_ball_f = raw_mm;
        s_vel_mm_s = 0.0f;
        s_have_ball = true;
    } else {
        frame_dt_ms = s_ms_total - s_last_frame_ms;
        if (frame_dt_ms < 30u)
            frame_dt_ms = 30u;
        if (frame_dt_ms > 250u)
            frame_dt_ms = 250u;
        *dt_s = (float)frame_dt_ms / 1000.0f;

        previous_mm = s_ball_f;
        s_ball_f += BALL_CTRL_POS_ALPHA * (raw_mm - s_ball_f);
        instant_velocity = (s_ball_f - previous_mm) / *dt_s;
        instant_velocity = clampf(
            instant_velocity,
            -BALL_CTRL_VEL_LIMIT_MM_S,
            BALL_CTRL_VEL_LIMIT_MM_S);
        s_vel_mm_s += BALL_CTRL_VEL_ALPHA *
            (instant_velocity - s_vel_mm_s);
    }

    s_last_frame_ms = s_ms_total;
    s_ball_mm_x100 = (int32_t)(s_ball_f >= 0.0f ?
        s_ball_f * 100.0f + 0.5f : s_ball_f * 100.0f - 0.5f);
}

static void update_settled_state(float measured_error_mm)
{
    if (absf(measured_error_mm) <= BALL_CTRL_SETTLED_ERR_MM &&
        absf(s_vel_mm_s) <= BALL_CTRL_SETTLED_VEL_MM_S) {
        if (s_settled_frames < 255u)
            s_settled_frames++;
        if (s_settled_frames >=
            (uint8_t)BALL_CTRL_SETTLED_CONFIRM_FRAMES)
            s_state = BALL_CTRL_STATE_SETTLED;
        return;
    }

    if (s_state == BALL_CTRL_STATE_SETTLED &&
        absf(measured_error_mm) <= BALL_CTRL_SETTLED_EXIT_ERR_MM &&
        absf(s_vel_mm_s) <= BALL_CTRL_SETTLED_EXIT_VEL_MM_S)
        return;

    s_settled_frames = 0u;
    s_state = BALL_CTRL_STATE_RUN;
}

static void control_from_measurement(float dt_s)
{
    float error_mm;
    float integral_candidate;
    float raw_command;
    float command;
    float kp;
    float ki;
    float near_weight;
    int32_t error_x100;

    error_mm = (float)s_target_mm_x100 / 100.0f - s_ball_f;
    if (absf(error_mm) <= BALL_CTRL_PID_DEADBAND_MM)
        error_mm = 0.0f;

    near_weight = (BALL_CTRL_PID_NEAR_ERR_MM - absf(error_mm)) /
        BALL_CTRL_PID_NEAR_ERR_MM;
    near_weight = clampf(near_weight, 0.0f, 1.0f);
    kp = BALL_CTRL_PID_KP;
    ki = BALL_CTRL_PID_KI +
        (BALL_CTRL_PID_NEAR_KI - BALL_CTRL_PID_KI) * near_weight;

    integral_candidate = s_integral;
    if (error_mm != 0.0f && s_prev_error_mm != 0.0f &&
        error_mm * s_prev_error_mm < 0.0f)
        integral_candidate *= 0.25f;

    if (absf(error_mm) <= BALL_CTRL_PID_INTEGRAL_MAX_ERR_MM &&
        absf(s_vel_mm_s) <= BALL_CTRL_PID_INTEGRAL_MAX_VEL_MM_S &&
        error_mm * s_vel_mm_s >= 0.0f) {
        integral_candidate += ki * error_mm * dt_s;
        integral_candidate = clampf(
            integral_candidate,
            -BALL_CTRL_PID_INTEGRAL_MAX,
            BALL_CTRL_PID_INTEGRAL_MAX);
    } else {
        integral_candidate *= 0.995f;
    }

    raw_command = BALL_CTRL_SIGN *
        (kp * error_mm +
         integral_candidate -
         BALL_CTRL_PID_KD * s_vel_mm_s);

    /* Conditional integration: do not wind up farther into output saturation. */
    if (!((raw_command >
               (float)BALL_CTRL_OUTPUT_MAX_MM_X100 / 100.0f &&
           error_mm > 0.0f) ||
          (raw_command <
               -(float)BALL_CTRL_OUTPUT_MAX_MM_X100 / 100.0f &&
           error_mm < 0.0f)))
        s_integral = integral_candidate;

    if (error_mm != 0.0f)
        s_prev_error_mm = error_mm;

    update_settled_state(error_mm);

    command = clampf(
        raw_command,
        -(float)BALL_CTRL_OUTPUT_MAX_MM_X100 / 100.0f,
        (float)BALL_CTRL_OUTPUT_MAX_MM_X100 / 100.0f);
    error_x100 = (int32_t)(command * 100.0f);
    apply_rod(error_x100);
}

void BallCtrl_Init(void)
{
    s_en = false;
    s_state = BALL_CTRL_STATE_IDLE;
    s_target_mm_x100 = BALL_CTRL_DEFAULT_TARGET_MM_X100;
    s_ball_mm_x100 = 0;
    s_rod_mm_x100 = 0;
    s_ms_total = 0u;
    s_lost_ms = 0u;
    s_rod_applied = false;
    s_coils_off = true;
    reset_dynamic_state();

    Stepper_SetSpeedSps(BALL_CTRL_STEPPER_SPS);
    Stepper_SetAccel(BALL_CTRL_STEPPER_ACCEL);
}

void BallCtrl_Enable(bool on)
{
    s_en = on;
    if (on) {
        coils_on();
        s_state = BALL_CTRL_STATE_RUN;
        s_lost_ms = 0u;
        s_rod_mm_x100 = 0;
        s_rod_applied = true;
        reset_dynamic_state();
    } else {
        s_state = BALL_CTRL_STATE_IDLE;
        s_rod_mm_x100 = 0;
        s_rod_applied = false;
        reset_dynamic_state();
        coils_off();
    }
}

bool BallCtrl_IsEnabled(void)
{
    return s_en;
}

void BallCtrl_SetTargetMm_x100(int32_t mm_x100)
{
    int32_t target_mm_x100 = clamp_i32(
        mm_x100,
        (int32_t)BALL_CTRL_TARGET_MIN_MM_X100,
        (int32_t)BALL_CTRL_TARGET_MAX_MM_X100);

    if (target_mm_x100 == s_target_mm_x100)
        return;

    s_target_mm_x100 = target_mm_x100;
    s_integral = 0.0f;
    s_settled_frames = 0u;
    s_state = s_en ? BALL_CTRL_STATE_RUN : BALL_CTRL_STATE_IDLE;
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
    s_ms_total++;
    if (s_state == BALL_CTRL_STATE_LOST && s_lost_ms < 1000000u)
        s_lost_ms++;
}

void BallCtrl_Update(void)
{
    ball_frame_t frame;
    ball_setpoint_cmd_t setpoint;
    float dt_s;

    if (VisionUart_TakeSetpoint(&setpoint) && setpoint.valid)
        BallCtrl_SetTargetMm(setpoint.target_mm);

    if (!s_en) {
        s_state = BALL_CTRL_STATE_IDLE;
        return;
    }

    if (s_state == BALL_CTRL_STATE_LOST &&
        s_lost_ms >= (uint32_t)BALL_CTRL_LOST_DISABLE_MS &&
        !s_coils_off)
        coils_off();

    if (VisionUart_TakeBallFrame(&frame)) {
        if (ball_frame_usable(&frame)) {
            s_bad_frames = 0u;
            update_measurement(&frame, &dt_s);
            if (s_state == BALL_CTRL_STATE_LOST) {
                s_state = BALL_CTRL_STATE_RUN;
                s_lost_ms = 0u;
                s_rod_mm_x100 = 0;
                s_rod_applied = true;
                reset_dynamic_state();
                update_measurement(&frame, &dt_s);
                coils_on();
            }
            control_from_measurement(dt_s);
        } else if (s_bad_frames < 255u) {
            s_bad_frames++;
            if (s_bad_frames > (uint8_t)BALL_CTRL_LOST_FRAME_GRACE)
                enter_lost();
        }
    }

    if (VisionUart_MsSinceBall() > (uint32_t)BALL_CTRL_LINK_TIMEOUT_MS) {
        if (s_state != BALL_CTRL_STATE_LOST)
            enter_lost();
        return;
    }
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