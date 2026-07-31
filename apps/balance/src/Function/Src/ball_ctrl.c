/**
 * @file ball_ctrl.c
 * @brief Low-rate visual ball position controller (crank-linkage boom).
 *
 * Feedback: vision pos_mm relative to O.
 * Actuation: abstract boom tilt rod_x100 → Stepper_SetTargetMm_x100().
 * Goal phase-1: hold ball at O or any setpoint while chassis is stationary.
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
static float             s_bias_mm;
static bool              s_have_ball;
static uint32_t          s_last_frame_ms;
static uint32_t          s_ms_total;
static uint32_t          s_lost_ms;
static uint8_t           s_bad_frames;
static bool              s_rod_applied;
static bool              s_coils_off;

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
    s_bias_mm = 0.0f;
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

static int32_t s_slew_limit_x100 = (int32_t)BALL_CTRL_ROD_SLEW_MM_X100;

static void apply_rod(int32_t rod_mm_x100)
{
    int32_t previous = s_rod_mm_x100;
    int32_t delta;
    int32_t slew = s_slew_limit_x100;

    if (slew < (int32_t)BALL_CTRL_ROD_SLEW_MM_X100)
        slew = (int32_t)BALL_CTRL_ROD_SLEW_MM_X100;

    /* 正/负不对称限幅 */
    rod_mm_x100 = clamp_i32(
        rod_mm_x100,
        -(int32_t)BALL_CTRL_ROD_NEG_MAX_MM_X100,
        (int32_t)BALL_CTRL_ROD_POS_MAX_MM_X100);

    if (s_rod_applied) {
        delta = rod_mm_x100 - previous;
        if (delta < (int32_t)BALL_CTRL_ROD_EPS_MM_X100 &&
            delta > -(int32_t)BALL_CTRL_ROD_EPS_MM_X100)
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
    /* 默认斜率；制动帧会临时抬高 */
    s_slew_limit_x100 = (int32_t)BALL_CTRL_ROD_SLEW_MM_X100;
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
    if (s_ball_f >= 0.0f)
        s_ball_mm_x100 = (int32_t)(s_ball_f * 100.0f + 0.5f);
    else
        s_ball_mm_x100 = (int32_t)(s_ball_f * 100.0f - 0.5f);
}

static void control_from_measurement(float dt_s)
{
    float predicted_ball_mm;
    float error_mm;
    float abs_error_mm;
    float kp;
    float near_weight;
    float output_limit;
    float bias_error;
    float correction_mm;
    float rod_mm;
    float minimum_rod_mm;
    float desired_sign;
    int32_t error_x100;

    predicted_ball_mm = s_ball_f + s_vel_mm_s *
        ((float)BALL_CTRL_PREDICT_MS / 1000.0f);
    error_mm = (float)s_target_mm_x100 / 100.0f - predicted_ball_mm;
    abs_error_mm = absf(error_mm);

    if (abs_error_mm <= BALL_CTRL_CONTROL_DEAD_MM)
        error_mm = 0.0f;
    abs_error_mm = absf(error_mm);

    near_weight = 1.0f - abs_error_mm / BALL_CTRL_NEAR_ERR_MM;
    near_weight = clampf(near_weight, 0.0f, 1.0f);
    kp = BALL_CTRL_KP_POS +
        (BALL_CTRL_KP_NEAR_POS - BALL_CTRL_KP_POS) * near_weight;
    {
        float kd = BALL_CTRL_KD_POS +
            (BALL_CTRL_KD_NEAR_POS - BALL_CTRL_KD_POS) * near_weight;
        output_limit = (float)BALL_CTRL_FAR_ROD_MAX_MM_X100 / 100.0f +
            ((float)BALL_CTRL_NEAR_ROD_MAX_MM_X100 / 100.0f -
             (float)BALL_CTRL_FAR_ROD_MAX_MM_X100 / 100.0f) * near_weight;

        bias_error = error_mm;
        if (bias_error > BALL_CTRL_BIAS_DEADBAND_MM)
            bias_error -= BALL_CTRL_BIAS_DEADBAND_MM;
        else if (bias_error < -BALL_CTRL_BIAS_DEADBAND_MM)
            bias_error += BALL_CTRL_BIAS_DEADBAND_MM;
        else
            bias_error = 0.0f;

        if (abs_error_mm <= BALL_CTRL_BIAS_ERR_MAX_MM &&
            absf(s_vel_mm_s) <= BALL_CTRL_BIAS_VEL_MAX_MM_S) {
            s_bias_mm += BALL_CTRL_SIGN * BALL_CTRL_BIAS_KI *
                bias_error * dt_s;
            s_bias_mm = clampf(
                s_bias_mm,
                -(float)BALL_CTRL_BIAS_MAX_MM_X100 / 100.0f,
                (float)BALL_CTRL_BIAS_MAX_MM_X100 / 100.0f);
        } else {
            s_bias_mm *= 0.995f;
        }

        correction_mm = BALL_CTRL_SIGN *
            (kp * error_mm - kd * s_vel_mm_s);
    }

    /*
     * 优先级：
     * 1) 冲向目标且速度大 → 反向制动（压过冲）
     * 2) 大误差且几乎卡住 → kick 拉回端点
     * 3) 中等误差低速 → stick 最小倾角
     * 否则走调度 PD
     */
    if (abs_error_mm <= BALL_CTRL_BRAKE_ERR_MM &&
        absf(s_vel_mm_s) >= BALL_CTRL_BRAKE_VEL_MM_S &&
        ((error_mm > 0.0f && s_vel_mm_s > 0.0f) ||
         (error_mm < 0.0f && s_vel_mm_s < 0.0f) ||
         /* 已越过目标仍高速：也要制动 */
         (error_mm > 0.0f && s_vel_mm_s < -BALL_CTRL_BRAKE_VEL_MM_S) ||
         (error_mm < 0.0f && s_vel_mm_s > BALL_CTRL_BRAKE_VEL_MM_S))) {
        float brake_scale;
        float spd = absf(s_vel_mm_s);
        /* 速度越大制动越满；近中心优先刹停 */
        brake_scale = spd / 45.0f;
        if (brake_scale > 1.0f)
            brake_scale = 1.0f;
        if (brake_scale < 0.55f)
            brake_scale = 0.55f;
        if (abs_error_mm < 40.0f && brake_scale < 0.85f)
            brake_scale = 0.85f;

        minimum_rod_mm =
            ((float)BALL_CTRL_BRAKE_ROD_MM_X100 / 100.0f) * brake_scale;
        /* 始终按速度反向制动（与误差符号无关，先消能量） */
        correction_mm = (s_vel_mm_s > 0.0f) ?
            -minimum_rod_mm : minimum_rod_mm;
        correction_mm *= BALL_CTRL_SIGN;
        /* 近中心几乎纯制动；远处保留少量位置项 */
        if (abs_error_mm < 35.0f)
            correction_mm = 0.90f * correction_mm +
                0.10f * BALL_CTRL_SIGN * (kp * error_mm);
        else
            correction_mm = 0.70f * correction_mm +
                0.30f * BALL_CTRL_SIGN * (kp * error_mm);
        if (output_limit < minimum_rod_mm)
            output_limit = minimum_rod_mm;
        s_slew_limit_x100 = (int32_t)BALL_CTRL_ROD_SLEW_BRAKE_MM_X100;
    } else if (abs_error_mm >= BALL_CTRL_KICK_ERR_MM &&
               absf(s_vel_mm_s) <= BALL_CTRL_KICK_VEL_MM_S) {
        minimum_rod_mm =
            (float)BALL_CTRL_KICK_ROD_MM_X100 / 100.0f;
        desired_sign = BALL_CTRL_SIGN * error_mm;
        correction_mm = desired_sign < 0.0f ?
            -minimum_rod_mm : minimum_rod_mm;
        if (output_limit < minimum_rod_mm)
            output_limit = minimum_rod_mm;
    } else if (abs_error_mm > BALL_CTRL_STICK_ERR_MM &&
               absf(s_vel_mm_s) <= BALL_CTRL_STICK_VEL_MM_S) {
        minimum_rod_mm =
            (float)BALL_CTRL_STICK_ROD_MM_X100 / 100.0f;
        desired_sign = BALL_CTRL_SIGN * error_mm;
        if (absf(correction_mm) < minimum_rod_mm)
            correction_mm = desired_sign < 0.0f ?
                -minimum_rod_mm : minimum_rod_mm;
    }

    /* 已接近且低速：保留 bias + 小纠偏，避免杆回 0 再滑走 */
    if (abs_error_mm <= BALL_CTRL_SETTLED_ERR_MM &&
        absf(s_vel_mm_s) <= BALL_CTRL_SETTLED_VEL_MM_S) {
        correction_mm *= 0.55f;
    }

    rod_mm = s_bias_mm + clampf(correction_mm, -output_limit, output_limit);
    rod_mm = clampf(
        rod_mm,
        -(float)BALL_CTRL_ROD_NEG_MAX_MM_X100 / 100.0f,
        (float)BALL_CTRL_ROD_POS_MAX_MM_X100 / 100.0f);

    if (abs_error_mm <= BALL_CTRL_SETTLED_ERR_MM &&
        absf(s_vel_mm_s) <= BALL_CTRL_SETTLED_VEL_MM_S)
        s_state = BALL_CTRL_STATE_SETTLED;
    else
        s_state = BALL_CTRL_STATE_RUN;

    error_x100 = (int32_t)(rod_mm * 100.0f);
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
        s_rod_applied = false;
        s_rod_mm_x100 = 0;
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
    s_target_mm_x100 = clamp_i32(
        mm_x100,
        (int32_t)BALL_CTRL_TARGET_MIN_MM_X100,
        (int32_t)BALL_CTRL_TARGET_MAX_MM_X100);
    s_bias_mm = 0.0f;
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
                s_rod_applied = false;
                s_rod_mm_x100 = 0;
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

    if (!s_have_ball)
        return;
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