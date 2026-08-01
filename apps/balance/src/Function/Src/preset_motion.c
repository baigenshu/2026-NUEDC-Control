#include "preset_motion.h"
#include "preset_motion_cfg.h"
#include "ball_ctrl.h"
#include "stepper.h"

static preset_motion_state_t s_state;
static uint32_t s_phase_start_ms;
static uint32_t s_profile_start_ms;
static bool s_positive_waypoint_ok;
static bool s_profile_stepper_dynamics_active;
static uint32_t s_saved_stepper_sps;
static uint32_t s_saved_stepper_accel;

static float absf(float value)
{
    return value < 0.0f ? -value : value;
}

static float clampf(float value, float lower, float upper)
{
    if (value < lower)
        return lower;
    if (value > upper)
        return upper;
    return value;
}

static int32_t round_to_i32(float value)
{
    return (int32_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static void enable_profile_stepper_dynamics(void)
{
    if (!s_profile_stepper_dynamics_active) {
        s_saved_stepper_sps = Stepper_GetSpeedSps();
        s_saved_stepper_accel = Stepper_GetAccel();
        s_profile_stepper_dynamics_active = true;
    }

    Stepper_SetSpeedSps(PRESET_MOTION_STEPPER_SPS);
    Stepper_SetAccel(PRESET_MOTION_STEPPER_ACCEL);
}

static void restore_stepper_dynamics(void)
{
    if (!s_profile_stepper_dynamics_active)
        return;

    Stepper_SetSpeedSps(s_saved_stepper_sps);
    Stepper_SetAccel(s_saved_stepper_accel);
    s_profile_stepper_dynamics_active = false;
}

static void start_override_phase(preset_motion_state_t state, uint32_t now_ms,
                                 int32_t rod_mm_x100)
{
    s_phase_start_ms = now_ms;
    s_state = state;
    BallCtrl_SetCommandOverrideMm_x100(rod_mm_x100);
}

static void start_final_settle(uint32_t now_ms)
{
    BallCtrl_SetTargetMm_x100(PRESET_MOTION_NEGATIVE_TARGET_MM_X100);
    BallCtrl_ClearCommandOverride();
    restore_stepper_dynamics();
    s_phase_start_ms = now_ms;
    s_state = PRESET_MOTION_FINAL_SETTLE;
}

static void update_negative_approach_command(void)
{
    float remaining_mm = ((float)BallCtrl_GetBallMm_x100() -
        (float)PRESET_MOTION_FINAL_PID_ENTRY_MM_X100) / 100.0f;
    float forward_speed_mm_s = -BallCtrl_GetVelocityMm_s();
    float desired_speed_mm_s;
    float drive_mm_x100;

    if (remaining_mm <= 0.0f)
        desired_speed_mm_s = 0.0f;
    else
        desired_speed_mm_s = clampf(
            remaining_mm / PRESET_MOTION_NEGATIVE_APPROACH_TIME_S,
            0.0f,
            PRESET_MOTION_NEGATIVE_APPROACH_MAX_SPEED_MM_S);

    drive_mm_x100 = PRESET_MOTION_NEGATIVE_SPEED_FEEDFORWARD_ROD_MM_X100 +
        PRESET_MOTION_NEGATIVE_SPEED_KP_ROD_PER_MM_S *
            (desired_speed_mm_s - forward_speed_mm_s);
    drive_mm_x100 = clampf(
        drive_mm_x100,
        -(float)PRESET_MOTION_NEGATIVE_BRAKE_ROD_MM_X100,
        -(float)PRESET_MOTION_NEGATIVE_PUSH_ROD_MM_X100);
    BallCtrl_SetCommandOverrideMm_x100(-round_to_i32(drive_mm_x100));
}

static int32_t predicted_position_mm_x100(void)
{
    float predicted_mm = (float)BallCtrl_GetBallMm_x100() / 100.0f +
        BallCtrl_GetVelocityMm_s() *
            (float)PRESET_MOTION_PREDICT_BRAKE_TIME_MS / 1000.0f;

    return (int32_t)(predicted_mm >= 0.0f ?
        predicted_mm * 100.0f + 0.5f : predicted_mm * 100.0f - 0.5f);
}

static void start_pid_profile(uint32_t now_ms)
{
    s_profile_start_ms = now_ms;
    s_positive_waypoint_ok = false;
    enable_profile_stepper_dynamics();
    start_override_phase(
        PRESET_MOTION_POSITIVE_PUSH,
        now_ms,
        PRESET_MOTION_POSITIVE_PUSH_ROD_MM_X100);
}

void PresetMotion_Init(void)
{
    s_state = PRESET_MOTION_IDLE;
    s_phase_start_ms = 0u;
    s_profile_start_ms = 0u;
    s_positive_waypoint_ok = false;
    s_profile_stepper_dynamics_active = false;
    s_saved_stepper_sps = 0u;
    s_saved_stepper_accel = 0u;
}

void PresetMotion_Start(uint32_t now_ms)
{
    restore_stepper_dynamics();
    BallCtrl_ClearHoldBias();
    BallCtrl_ClearCommandOverride();
    BallCtrl_SetTargetMm_x100(0);
    if (!BallCtrl_IsEnabled())
        BallCtrl_ZeroArmAndStart();
    /* BallCtrl_Enable() resets mode flags, so enable preset mode afterwards. */
    BallCtrl_SetPresetControl(true);
    s_phase_start_ms = now_ms;
    s_profile_start_ms = 0u;
    s_positive_waypoint_ok = false;
    s_state = PRESET_MOTION_ARMING_ZERO;
}

void PresetMotion_Cancel(void)
{
    BallCtrl_SetPresetControl(false);
    BallCtrl_ClearHoldBias();
    BallCtrl_ClearCommandOverride();
    restore_stepper_dynamics();
    s_state = PRESET_MOTION_IDLE;
    s_profile_start_ms = 0u;
    s_positive_waypoint_ok = false;
}

void PresetMotion_Update(uint32_t now_ms)
{
    if (s_state == PRESET_MOTION_IDLE ||
        s_state == PRESET_MOTION_COMPLETE ||
        s_state == PRESET_MOTION_TIMEOUT)
        return;

    if (!BallCtrl_IsEnabled()) {
        PresetMotion_Cancel();
        return;
    }

    if (s_state == PRESET_MOTION_ARMING_ZERO) {
        if (now_ms - s_phase_start_ms >=
            PRESET_MOTION_ARMING_ZERO_MAX_MS)
            start_pid_profile(now_ms);
        return;
    }

    if (s_state == PRESET_MOTION_POSITIVE_PUSH) {
        if (predicted_position_mm_x100() >=
            PRESET_MOTION_POSITIVE_TURN_MM_X100)
            start_override_phase(
                PRESET_MOTION_POSITIVE_BRAKE,
                now_ms,
                PRESET_MOTION_POSITIVE_BRAKE_ROD_MM_X100);
        return;
    }

    if (s_state == PRESET_MOTION_POSITIVE_BRAKE) {
        if (BallCtrl_GetBallMm_x100() >=
            PRESET_MOTION_POSITIVE_TURN_MM_X100) {
            s_positive_waypoint_ok = true;
            start_override_phase(
                PRESET_MOTION_NEGATIVE_PUSH,
                now_ms,
                PRESET_MOTION_NEGATIVE_PUSH_ROD_MM_X100);
        } else if (BallCtrl_GetVelocityMm_s() <= 0.0f) {
            start_override_phase(
                PRESET_MOTION_POSITIVE_PUSH,
                now_ms,
                PRESET_MOTION_POSITIVE_PUSH_ROD_MM_X100);
        }
        return;
    }

    if (s_state == PRESET_MOTION_NEGATIVE_PUSH) {
        if (BallCtrl_GetBallMm_x100() <=
            PRESET_MOTION_NEGATIVE_APPROACH_START_MM_X100)
            start_override_phase(
                PRESET_MOTION_NEGATIVE_BRAKE,
                now_ms,
                0);
        return;
    }

    if (s_state == PRESET_MOTION_NEGATIVE_BRAKE) {
        update_negative_approach_command();
        if (BallCtrl_GetBallMm_x100() <=
                PRESET_MOTION_FINAL_PID_ENTRY_MM_X100 &&
            absf(BallCtrl_GetVelocityMm_s()) <=
                PRESET_MOTION_FINAL_PID_ENTRY_SPEED_MM_S)
            start_final_settle(now_ms);
        return;
    }

    if (s_state == PRESET_MOTION_FINAL_SETTLE) {
        if (BallCtrl_IsSettled()) {
            s_state = s_positive_waypoint_ok ? PRESET_MOTION_COMPLETE :
                PRESET_MOTION_TIMEOUT;
            restore_stepper_dynamics();
        }
        return;
    }
}

preset_motion_state_t PresetMotion_GetState(void)
{
    return s_state;
}

uint32_t PresetMotion_GetElapsedMs(uint32_t now_ms)
{
    if (s_profile_start_ms == 0u)
        return 0u;
    return now_ms - s_profile_start_ms;
}

bool PresetMotion_IsActive(void)
{
    return s_state == PRESET_MOTION_ARMING_ZERO ||
        s_state == PRESET_MOTION_POSITIVE_PUSH ||
        s_state == PRESET_MOTION_POSITIVE_BRAKE ||
        s_state == PRESET_MOTION_NEGATIVE_PUSH ||
        s_state == PRESET_MOTION_NEGATIVE_BRAKE ||
        s_state == PRESET_MOTION_FINAL_SETTLE;
}

bool PresetMotion_IsFinished(void)
{
    return s_state == PRESET_MOTION_COMPLETE ||
        s_state == PRESET_MOTION_TIMEOUT;
}