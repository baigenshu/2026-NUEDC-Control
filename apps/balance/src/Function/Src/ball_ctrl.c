/**
 * @file ball_ctrl.c
 * @brief Position PID for a camera-observed ball on a crank-driven boom.
 */
#include "ball_ctrl.h"
#include "ball_ctrl_cfg.h"
#include "vision_uart.h"
#include "stepper.h"

#define BALL_CTRL_TRACE_CAPACITY (1024u)

typedef struct {
    uint32_t ms;
    int16_t raw_mm;
    int16_t pos_mm_x10;
    int16_t vel_mm_s_x10;
    int16_t command_x100;
    int16_t step_pos;
    uint8_t state;
    uint8_t conf;
} ball_ctrl_trace_sample_t;

volatile ball_ctrl_trace_sample_t
    g_ball_ctrl_trace[BALL_CTRL_TRACE_CAPACITY];
volatile uint16_t g_ball_ctrl_trace_head;
volatile uint16_t g_ball_ctrl_trace_count;

static bool              s_en;
static ball_ctrl_state_t s_state;
static int32_t           s_target_mm_x100;
static int32_t           s_ball_mm_x100;
static int32_t           s_rod_mm_x100;
static float             s_pos_mm;
static float             s_vel_mm_s;
static float             s_integral_command;
static float             s_command;
static float             s_prev_error_mm;
static uint8_t            s_settled_frames;
static bool               s_have_ball;
static uint32_t           s_last_frame_ms;
static volatile uint32_t  s_ms_total;
static volatile uint32_t  s_lost_ms;
static uint8_t            s_bad_frames;
static bool               s_rod_applied;
static bool               s_coils_off;
static bool               s_calibration_pending;
static int16_t            s_calibration_samples[BALL_CTRL_CALIBRATION_FRAMES];
static uint8_t            s_calibration_head;
static uint8_t            s_calibration_count;
static float              s_position_offset_mm;

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

static int16_t float_to_i16_x10(float value)
{
    int32_t scaled = (int32_t)(value >= 0.0f ?
        value * 10.0f + 0.5f : value * 10.0f - 0.5f);

    return (int16_t)clamp_i32(scaled, -32768, 32767);
}

static void trace_sample(const ball_frame_t *frame)
{
    uint16_t index = g_ball_ctrl_trace_head;
    int32_t step_pos = Stepper_GetPositionSteps();

    g_ball_ctrl_trace[index].ms = s_ms_total;
    g_ball_ctrl_trace[index].raw_mm = frame->pos_mm;
    g_ball_ctrl_trace[index].pos_mm_x10 = float_to_i16_x10(s_pos_mm);
    g_ball_ctrl_trace[index].vel_mm_s_x10 = float_to_i16_x10(s_vel_mm_s);
    g_ball_ctrl_trace[index].command_x100 = (int16_t)s_rod_mm_x100;
    g_ball_ctrl_trace[index].step_pos = (int16_t)clamp_i32(
        step_pos, -32768, 32767);
    g_ball_ctrl_trace[index].state = (uint8_t)s_state;
    g_ball_ctrl_trace[index].conf = frame->conf;

    index++;
    if (index >= (uint16_t)BALL_CTRL_TRACE_CAPACITY)
        index = 0u;
    g_ball_ctrl_trace_head = index;
    if (g_ball_ctrl_trace_count < (uint16_t)BALL_CTRL_TRACE_CAPACITY)
        g_ball_ctrl_trace_count++;
}

static void reset_dynamic_state(void)
{
    s_pos_mm = 0.0f;
    s_vel_mm_s = 0.0f;
    s_integral_command = 0.0f;
    s_prev_error_mm = 0.0f;
    s_settled_frames = 0u;
    s_have_ball = false;
    s_last_frame_ms = 0u;
    s_bad_frames = 0u;
}

static void reset_calibration_samples(void)
{
    s_calibration_head = 0u;
    s_calibration_count = 0u;
}

static void record_calibration_sample(const ball_frame_t *frame)
{
    s_calibration_samples[s_calibration_head] = frame->pos_mm;
    s_calibration_head++;
    if (s_calibration_head >= (uint8_t)BALL_CTRL_CALIBRATION_FRAMES)
        s_calibration_head = 0u;
    if (s_calibration_count < (uint8_t)BALL_CTRL_CALIBRATION_FRAMES)
        s_calibration_count++;
}

static bool try_calibrated_start(void)
{
    int32_t sum = 0;
    int16_t minimum;
    int16_t maximum;
    uint8_t index;

    if (!s_calibration_pending ||
        s_calibration_count < (uint8_t)BALL_CTRL_CALIBRATION_FRAMES ||
        !VisionUart_BallLinkOk())
        return false;

    minimum = s_calibration_samples[0];
    maximum = minimum;
    for (index = 0u; index < (uint8_t)BALL_CTRL_CALIBRATION_FRAMES; index++) {
        int16_t sample = s_calibration_samples[index];
        sum += sample;
        if (sample < minimum)
            minimum = sample;
        if (sample > maximum)
            maximum = sample;
    }
    if ((int32_t)maximum - (int32_t)minimum >
        (int32_t)BALL_CTRL_CALIBRATION_MAX_SPREAD_MM)
        return false;

    s_position_offset_mm =
        (float)sum / (float)BALL_CTRL_CALIBRATION_FRAMES -
        (float)s_target_mm_x100 / 100.0f;
    s_calibration_pending = false;
    Stepper_SetZero();
    BallCtrl_Enable(true);
    return true;
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

static void apply_command(float command, float dt_s, bool immediate)
{
    float max_command = (float)BALL_CTRL_OUTPUT_MAX_MM_X100 / 100.0f;
    float max_delta;
    float delta;
    int32_t rod_mm_x100;

    command = clampf(command, -max_command, max_command);

    if (s_rod_applied && !immediate) {
        dt_s = clampf(
            dt_s,
            (float)BALL_CTRL_FRAME_DT_MIN_MS / 1000.0f,
            (float)BALL_CTRL_FRAME_DT_MAX_MS / 1000.0f);
        max_delta = BALL_CTRL_COMMAND_RATE_UNIT_S * dt_s;
        delta = command - s_command;
        if (delta > max_delta)
            command = s_command + max_delta;
        else if (delta < -max_delta)
            command = s_command - max_delta;
    }

    s_command = command;
    rod_mm_x100 = (int32_t)(command >= 0.0f ?
        command * 100.0f + 0.5f : command * 100.0f - 0.5f);
    rod_mm_x100 = clamp_i32(
        rod_mm_x100,
        -(int32_t)BALL_CTRL_OUTPUT_MAX_MM_X100,
        (int32_t)BALL_CTRL_OUTPUT_MAX_MM_X100);

    if (s_rod_applied &&
        rod_mm_x100 - s_rod_mm_x100 <
            (int32_t)BALL_CTRL_COMMAND_EPS_MM_X100 &&
        rod_mm_x100 - s_rod_mm_x100 >
            -(int32_t)BALL_CTRL_COMMAND_EPS_MM_X100)
        return;

    coils_on();
    s_rod_mm_x100 = rod_mm_x100;
    s_rod_applied = true;
    Stepper_SetTargetMm_x100(rod_mm_x100);
}

static void enter_lost(void)
{
    if (s_state == BALL_CTRL_STATE_LOST)
        return;

    s_state = BALL_CTRL_STATE_LOST;
    s_lost_ms = 0u;
    s_ball_mm_x100 = 0;
#if BALL_CTRL_HOLD_LEVEL_ON_LOSS
    apply_command(
        0.0f,
        (float)BALL_CTRL_DEFAULT_FRAME_DT_MS / 1000.0f,
        true);
#endif
    reset_dynamic_state();
}

static void update_measurement(const ball_frame_t *frame, float *dt_s)
{
    float raw_mm;
    float predicted_mm;
    float innovation_mm;
    uint32_t frame_dt_ms;

    raw_mm = (float)ball_pos_to_mm_x100(frame->pos_mm) / 100.0f -
        s_position_offset_mm;
    *dt_s = (float)BALL_CTRL_DEFAULT_FRAME_DT_MS / 1000.0f;

    if (!s_have_ball) {
        s_pos_mm = raw_mm;
        s_vel_mm_s = 0.0f;
        s_have_ball = true;
    } else {
        frame_dt_ms = s_ms_total - s_last_frame_ms;
        if (frame_dt_ms < (uint32_t)BALL_CTRL_FRAME_DT_MIN_MS)
            frame_dt_ms = (uint32_t)BALL_CTRL_FRAME_DT_MIN_MS;
        if (frame_dt_ms > (uint32_t)BALL_CTRL_FRAME_DT_MAX_MS)
            frame_dt_ms = (uint32_t)BALL_CTRL_FRAME_DT_MAX_MS;
        *dt_s = (float)frame_dt_ms / 1000.0f;

        predicted_mm = s_pos_mm + s_vel_mm_s * *dt_s;
        innovation_mm = clampf(
            raw_mm - predicted_mm,
            -BALL_CTRL_OBSERVER_INNOVATION_MAX_MM,
            BALL_CTRL_OBSERVER_INNOVATION_MAX_MM);
        s_pos_mm = predicted_mm +
            BALL_CTRL_OBSERVER_ALPHA * innovation_mm;
        s_vel_mm_s +=
            BALL_CTRL_OBSERVER_BETA * innovation_mm / *dt_s;
        s_vel_mm_s = clampf(
            s_vel_mm_s,
            -BALL_CTRL_VEL_LIMIT_MM_S,
            BALL_CTRL_VEL_LIMIT_MM_S);
    }

    s_last_frame_ms = s_ms_total;
    s_ball_mm_x100 = (int32_t)(s_pos_mm >= 0.0f ?
        s_pos_mm * 100.0f + 0.5f : s_pos_mm * 100.0f - 0.5f);
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
    float measured_error_mm;
    float control_error_mm;
    float integral_candidate;
    float base_command;
    float old_unsaturated;
    float new_unsaturated;
    float command;
    float kp;
    float kd;
    float near_weight;
    float leak;
    float max_command = (float)BALL_CTRL_OUTPUT_MAX_MM_X100 / 100.0f;

    measured_error_mm =
        (float)s_target_mm_x100 / 100.0f - s_pos_mm;
    control_error_mm = measured_error_mm;
    if (absf(control_error_mm) <= BALL_CTRL_PID_DEADBAND_MM &&
        absf(s_vel_mm_s) <= BALL_CTRL_PID_DEADBAND_VEL_MM_S)
        control_error_mm = 0.0f;

    near_weight =
        (BALL_CTRL_PID_NEAR_ZONE_MM - absf(measured_error_mm)) /
        BALL_CTRL_PID_NEAR_ZONE_MM;
    near_weight = clampf(near_weight, 0.0f, 1.0f);
    kp = BALL_CTRL_PID_FAR_KP +
        (BALL_CTRL_PID_NEAR_KP - BALL_CTRL_PID_FAR_KP) * near_weight;
    kd = BALL_CTRL_PID_FAR_KD +
        (BALL_CTRL_PID_NEAR_KD - BALL_CTRL_PID_FAR_KD) * near_weight;

    integral_candidate = s_integral_command;
    if (measured_error_mm != 0.0f && s_prev_error_mm != 0.0f &&
        measured_error_mm * s_prev_error_mm < 0.0f)
        integral_candidate *= BALL_CTRL_PID_CROSSING_I_RETAIN;

    if (absf(measured_error_mm) <= BALL_CTRL_PID_INTEGRAL_ZONE_MM &&
        absf(s_vel_mm_s) <= BALL_CTRL_PID_INTEGRAL_VEL_MAX_MM_S) {
        integral_candidate +=
            BALL_CTRL_PID_KI * control_error_mm * dt_s;
    } else {
        leak = 1.0f - BALL_CTRL_PID_INTEGRAL_LEAK_PER_S * dt_s;
        integral_candidate *= clampf(leak, 0.0f, 1.0f);
    }
    integral_candidate = clampf(
        integral_candidate,
        -BALL_CTRL_PID_INTEGRAL_OUTPUT_MAX,
        BALL_CTRL_PID_INTEGRAL_OUTPUT_MAX);

    base_command = kp * control_error_mm - kd * s_vel_mm_s;
    old_unsaturated = base_command + s_integral_command;
    new_unsaturated = base_command + integral_candidate;

    if (absf(new_unsaturated) <= max_command ||
        absf(new_unsaturated) < absf(old_unsaturated))
        s_integral_command = integral_candidate;

    s_prev_error_mm = measured_error_mm;
    update_settled_state(measured_error_mm);

    command = BALL_CTRL_SIGN * clampf(
        base_command + s_integral_command,
        -max_command,
        max_command);
    apply_command(command, dt_s, false);
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
    s_command = 0.0f;
    s_rod_applied = false;
    s_coils_off = true;
    s_calibration_pending = false;
    s_position_offset_mm = 0.0f;
    g_ball_ctrl_trace_head = 0u;
    g_ball_ctrl_trace_count = 0u;
    reset_dynamic_state();
    reset_calibration_samples();

    Stepper_SetSpeedSps(BALL_CTRL_STEPPER_SPS);
    Stepper_SetAccel(BALL_CTRL_STEPPER_ACCEL);
}

void BallCtrl_Enable(bool on)
{
    s_en = on;
    if (on) {
        s_state = BALL_CTRL_STATE_RUN;
        s_lost_ms = 0u;
        s_rod_mm_x100 = 0;
        s_command = 0.0f;
        s_rod_applied = false;
        reset_dynamic_state();
        apply_command(
            0.0f,
            (float)BALL_CTRL_DEFAULT_FRAME_DT_MS / 1000.0f,
            true);
    } else {
        s_state = BALL_CTRL_STATE_IDLE;
        s_rod_mm_x100 = 0;
        s_rod_applied = false;
        s_calibration_pending = false;
        reset_dynamic_state();
        reset_calibration_samples();
        coils_off();
    }
}

bool BallCtrl_IsEnabled(void)
{
    return s_en;
}

void BallCtrl_RequestCalibratedStart(void)
{
    if (s_en)
        return;

    reset_calibration_samples();
    s_calibration_pending = true;
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
    s_integral_command = 0.0f;
    s_prev_error_mm = 0.0f;
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
        if (VisionUart_TakeBallFrame(&frame) && ball_frame_usable(&frame)) {
            record_calibration_sample(&frame);
            (void)try_calibrated_start();
        }
        return;
    }

    if (s_state == BALL_CTRL_STATE_LOST &&
        s_lost_ms >= (uint32_t)BALL_CTRL_LOST_DISABLE_MS &&
        !s_coils_off)
        coils_off();

    if (VisionUart_TakeBallFrame(&frame)) {
        if (ball_frame_usable(&frame)) {
            s_bad_frames = 0u;
            if (s_state == BALL_CTRL_STATE_LOST) {
                s_state = BALL_CTRL_STATE_RUN;
                s_lost_ms = 0u;
                reset_dynamic_state();
                coils_on();
            }
            update_measurement(&frame, &dt_s);
            control_from_measurement(dt_s);
            trace_sample(&frame);
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

int32_t BallCtrl_GetPositionOffsetMm_x100(void)
{
    float offset_x100 = s_position_offset_mm * 100.0f;

    return (int32_t)(offset_x100 >= 0.0f ?
        offset_x100 + 0.5f : offset_x100 - 0.5f);
}

bool BallCtrl_IsSettled(void)
{
    return s_state == BALL_CTRL_STATE_SETTLED;
}