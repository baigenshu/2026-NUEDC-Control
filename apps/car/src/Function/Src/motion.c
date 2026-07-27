#include "motion.h"
#include "pose_feedback.h"
#include "go_straight.h"
#include "chassis.h"
#include "encoder.h"
#include "robot_config.h"
#include "bsp_systick.h"

#define PULSE_RATIO        0.98f
#define TURN_TIMEOUT_MS    15000u
#define DIST_TIMEOUT_MS    30000u
#define PWM_MIN_MOVE       8

static Motion_Status_t s_st;
static int16_t s_cmd_pwm;
static int8_t s_dir;

static int32_t s_baseA;
static int32_t s_baseB;
static int32_t s_baseC;
static int32_t s_baseD;
static int32_t s_target_pulses;
static uint32_t s_elapsed_ms;
static uint32_t s_timeout_ms;

static int32_t iabs32(int32_t v)
{
    return (v < 0) ? -v : v;
}

static int16_t clamp_pwm_abs(int16_t v)
{
    if (v < 0) {
        v = (int16_t)(-v);
    }
    if (v > 100) {
        v = 100;
    }
    return v;
}

static void capture_enc_base(void)
{
    s_baseA = EncoderA_Count;
    s_baseB = EncoderB_Count;
    s_baseC = EncoderC_Count;
    s_baseD = EncoderD_Count;
}

static void read_abs_pulses(int32_t v[4])
{
    v[0] = iabs32(EncoderA_Count - s_baseA);
    v[1] = iabs32(EncoderB_Count - s_baseB);
    v[2] = iabs32(EncoderC_Count - s_baseC);
    v[3] = iabs32(EncoderD_Count - s_baseD);
}

/* 转角：四轮平均（与已标定 ROBOT_TURN_SCALE 配套） */
static int32_t avg_abs_pulses(void)
{
    int32_t v[4];

    read_abs_pulses(v);
    return (v[0] + v[1] + v[2] + v[3]) / 4;
}

/*
 * 定距：有效轮平均（忽略几乎不动的轮，避免 0 稀释导致冲过约 2~4 倍）
 */
static int32_t dist_progress_pulses(void)
{
    int32_t v[4];
    int32_t sum;
    int32_t mx;
    int n;
    int i;

    read_abs_pulses(v);

    sum = 0;
    n = 0;
    mx = v[0];
    for (i = 0; i < 4; i++) {
        if (v[i] > mx) {
            mx = v[i];
        }
        if (v[i] >= 10) {
            sum += v[i];
            n++;
        }
    }

    if (n > 0) {
        return sum / n;
    }
    return mx;
}

static float pulses_to_deg(int32_t avg_pulses)
{
    float arc_m = (float)avg_pulses * ROBOT_M_PER_PULSE;
    float rad = arc_m / (ROBOT_TRACK_M * 0.5f);
    float deg = rad * (180.0f / ROBOT_PI);

    return deg / ROBOT_TURN_SCALE;
}

static float pulses_to_m(int32_t avg_pulses)
{
    return ((float)avg_pulses * ROBOT_M_PER_PULSE) / ROBOT_DIST_SCALE;
}

static void status_from_chassis(void)
{
    s_st.left_cmd = Chassis_GetStatus()->left_cmd;
    s_st.right_cmd = Chassis_GetStatus()->right_cmd;
}

static void enter_done(void)
{
    GoStraight_Stop();
    Chassis_Stop();
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
    if (s_st.state == MOTION_GO_DIST) {
        s_st.pulse_now = dist_progress_pulses();
    } else {
        s_st.pulse_now = avg_abs_pulses();
    }
    s_st.state = MOTION_DONE;
    s_st.done = 1;
}

static void apply_spin(void)
{
    if (s_dir > 0) {
        Chassis_SpinLeft(s_cmd_pwm);
    } else {
        Chassis_SpinRight(s_cmd_pwm);
    }
    status_from_chassis();
}

static void apply_go(void)
{
    GoStraight_Step();
    {
        const GoStraight_Status_t *gs = GoStraight_GetStatus();
        s_st.left_cmd = gs->left_cmd;
        s_st.right_cmd = gs->right_cmd;
    }
}

void Motion_Init(void)
{
    Pose_Init();
    GoStraight_Init();

    s_st.state = MOTION_IDLE;
    s_st.target_m = 0.0f;
    s_st.feedback_m = 0.0f;
    s_st.target_deg = 0.0f;
    s_st.feedback_deg = 0.0f;
    s_st.cmd_pwm = 0;
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
    s_st.pulse_now = 0;
    s_st.pulse_target = 0;
    s_st.done = 1;

    s_cmd_pwm = 0;
    s_dir = 1;
    s_baseA = s_baseB = s_baseC = s_baseD = 0;
    s_target_pulses = 0;
    s_elapsed_ms = 0;
    s_timeout_ms = TURN_TIMEOUT_MS;
}

void Motion_Abort(void)
{
    GoStraight_Stop();
    Chassis_Stop();
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
    s_st.state = MOTION_ABORTED;
    s_st.done = 1;
}

uint8_t Motion_IsDone(void)
{
    return s_st.done;
}

const Motion_Status_t *Motion_GetStatus(void)
{
    return &s_st;
}

void Motion_GoDistance(float meters, int16_t base_pwm)
{
    float target_abs;
    int16_t signed_pwm;

    if (meters > -0.005f && meters < 0.005f) {
        s_st.target_m = 0.0f;
        s_st.feedback_m = 0.0f;
        s_st.target_deg = 0.0f;
        s_st.feedback_deg = 0.0f;
        s_st.pulse_now = 0;
        s_st.pulse_target = 0;
        enter_done();
        return;
    }

    s_cmd_pwm = clamp_pwm_abs(base_pwm);
    if (s_cmd_pwm < PWM_MIN_MOVE) {
        s_cmd_pwm = PWM_MIN_MOVE;
    }

    s_dir = (meters >= 0.0f) ? 1 : -1;
    signed_pwm = (s_dir > 0) ? s_cmd_pwm : (int16_t)(-s_cmd_pwm);

    s_st.target_m = meters;
    s_st.feedback_m = 0.0f;
    s_st.target_deg = 0.0f;
    s_st.feedback_deg = 0.0f;
    s_st.cmd_pwm = s_cmd_pwm;

    target_abs = (meters >= 0.0f) ? meters : -meters;
    s_target_pulses = (int32_t)(target_abs / ROBOT_M_PER_PULSE * ROBOT_DIST_SCALE + 0.5f);
    if (s_target_pulses < 20) {
        s_target_pulses = 20;
    }
    s_st.pulse_target = s_target_pulses;

    capture_enc_base();
    s_elapsed_ms = 0;
    s_timeout_ms = DIST_TIMEOUT_MS;

    Pose_Reset();
    GoStraight_Start(signed_pwm);

    s_st.pulse_now = 0;
    s_st.state = MOTION_GO_DIST;
    s_st.done = 0;

    apply_go();
}

void Motion_TurnAngle(float deg, int16_t spin_pwm)
{
    float target_abs;
    float arc_m;

    if (deg > -0.5f && deg < 0.5f) {
        s_st.target_m = 0.0f;
        s_st.feedback_m = 0.0f;
        s_st.target_deg = 0.0f;
        s_st.feedback_deg = 0.0f;
        s_st.pulse_now = 0;
        s_st.pulse_target = 0;
        enter_done();
        return;
    }

    GoStraight_Stop();

    s_cmd_pwm = clamp_pwm_abs(spin_pwm);
    if (s_cmd_pwm < PWM_MIN_MOVE) {
        s_cmd_pwm = PWM_MIN_MOVE;
    }

    s_dir = (deg >= 0.0f) ? 1 : -1;
    s_st.target_deg = deg;
    s_st.feedback_deg = 0.0f;
    s_st.target_m = 0.0f;
    s_st.feedback_m = 0.0f;
    s_st.cmd_pwm = s_cmd_pwm;

    target_abs = (deg >= 0.0f) ? deg : -deg;
    arc_m = (target_abs * (ROBOT_PI / 180.0f)) * (ROBOT_TRACK_M * 0.5f);
    s_target_pulses = (int32_t)(arc_m / ROBOT_M_PER_PULSE * ROBOT_TURN_SCALE + 0.5f);
    if (s_target_pulses < 20) {
        s_target_pulses = 20;
    }
    s_st.pulse_target = s_target_pulses;

    capture_enc_base();
    s_elapsed_ms = 0;
    s_timeout_ms = TURN_TIMEOUT_MS;

    Pose_Reset();

    s_st.pulse_now = 0;
    s_st.state = MOTION_TURN;
    s_st.done = 0;

    apply_spin();
}

void Motion_Step(uint16_t period_ms)
{
    int32_t avg;
    int32_t need;

    if (s_st.state != MOTION_TURN && s_st.state != MOTION_GO_DIST) {
        return;
    }

    if (period_ms == 0u) {
        period_ms = 10u;
    }
    s_elapsed_ms += period_ms;

    need = (int32_t)((float)s_target_pulses * PULSE_RATIO);
    if (need < 1) {
        need = 1;
    }

    if (s_st.state == MOTION_GO_DIST) {
        avg = dist_progress_pulses();
        s_st.pulse_now = avg;
        s_st.feedback_m = pulses_to_m(avg);
        if (s_dir < 0) {
            s_st.feedback_m = -s_st.feedback_m;
        }

        if (avg >= need || s_elapsed_ms >= s_timeout_ms) {
            enter_done();
            return;
        }
        apply_go();
        return;
    }

    /* MOTION_TURN */
    avg = avg_abs_pulses();
    s_st.pulse_now = avg;
    s_st.feedback_deg = pulses_to_deg(avg);
    if (s_dir < 0) {
        s_st.feedback_deg = -s_st.feedback_deg;
    }

    if (avg >= need || s_elapsed_ms >= s_timeout_ms) {
        enter_done();
        return;
    }
    apply_spin();
}

void Motion_GoDistance_Wait(float meters, int16_t base_pwm, uint16_t period_ms)
{
    if (period_ms == 0u) {
        period_ms = 10u;
    }

    Motion_GoDistance(meters, base_pwm);
    while (!Motion_IsDone()) {
        Motion_Step(period_ms);
        delay_ms(period_ms);
    }
}

void Motion_TurnAngle_Wait(float deg, int16_t spin_pwm, uint16_t period_ms)
{
    if (period_ms == 0u) {
        period_ms = 10u;
    }

    Motion_TurnAngle(deg, spin_pwm);
    while (!Motion_IsDone()) {
        Motion_Step(period_ms);
        delay_ms(period_ms);
    }
}
