#include "line_track.h"
#include "chassis.h"
#include "grayscale_sensor.h"
#include "pid.h"

/* ---- 直道速度 ---- */
#define BASE_SPEED_DEFAULT  15
#define MIN_SPEED          -30
#define MAX_SPEED           35

/* ---- 大弯/直角 ---- */
#define CORNER_ERR          1500
#define CORNER_BASE         10
#define LOST_TURN_PWM       18

#define MASK_LEFT_OUT       0x03u   /* S1 S2 */
#define MASK_RIGHT_OUT      0xC0u   /* S7 S8 */

/* ---- 循迹 PID（作用在灰度误差上） ---- */
#define TRACK_KP            0.010f
#define TRACK_KI            0.0000f
#define TRACK_KD            0.015f
#define TRACK_OUT_MAX       28.0f

static PID_t s_track_pid;
static int8_t s_last_side;
static int16_t s_base_speed;
static LineTrack_Status_t s_st;

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
{
    if (v > hi) {
        return hi;
    }
    if (v < lo) {
        return lo;
    }
    return v;
}

static void drive_lr(int16_t left, int16_t right)
{
    left = clamp_i16(left, MIN_SPEED, MAX_SPEED);
    right = clamp_i16(right, MIN_SPEED, MAX_SPEED);
    Chassis_Drive(left, right);
    s_st.left_cmd = left;
    s_st.right_cmd = right;
}

void LineTrack_Init(void)
{
    PID_Init(&s_track_pid, TRACK_KP, TRACK_KI, TRACK_KD, TRACK_OUT_MAX, 0.0f);
    s_last_side = 0;
    s_base_speed = BASE_SPEED_DEFAULT;
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
    s_st.error = 0;
    s_st.sensors = 0;
    s_st.lost = 0;
    s_st.hard_corner = 0;
    s_st.last_side = 0;
}

void LineTrack_SetBaseSpeed(int16_t base)
{
    s_base_speed = clamp_i16(base, 0, MAX_SPEED);
}

void LineTrack_Stop(void)
{
    Chassis_Stop();
    PID_Reset(&s_track_pid);
    s_st.left_cmd = 0;
    s_st.right_cmd = 0;
}

const LineTrack_Status_t *LineTrack_GetStatus(void)
{
    return &s_st;
}

void LineTrack_Step(LineTrack_Status_t *st)
{
    uint8_t sensors;
    int16_t err;
    float pid_out;
    int16_t left_cmd;
    int16_t right_cmd;
    int16_t base;
    uint8_t lost;
    uint8_t hard_corner = 0;

    sensors = Read_Sensors();
    err = Get_Tracking_Error(sensors);
    lost = Is_Tracking_Lost(sensors);

    /* 仅外侧触发：按直角弯处理 */
    if (!lost) {
        uint8_t only_left = (sensors & MASK_LEFT_OUT) && !(sensors & (uint8_t)~MASK_LEFT_OUT);
        uint8_t only_right = (sensors & MASK_RIGHT_OUT) && !(sensors & (uint8_t)~MASK_RIGHT_OUT);
        if (only_left) {
            err = -3500;
            hard_corner = 1;
        } else if (only_right) {
            err = 3500;
            hard_corner = 1;
        }
    }

    if (!lost && err != 0) {
        s_last_side = (err > 0) ? 1 : -1;
    }

    if (lost) {
        /* 丢线：按上次侧向原地找线 */
        PID_Reset(&s_track_pid);
        if (s_last_side > 0) {
            left_cmd = LOST_TURN_PWM;
            right_cmd = -LOST_TURN_PWM;
        } else if (s_last_side < 0) {
            left_cmd = -LOST_TURN_PWM;
            right_cmd = LOST_TURN_PWM;
        } else {
            left_cmd = (int16_t)(s_base_speed / 2);
            right_cmd = (int16_t)(s_base_speed / 2);
        }
    } else if (hard_corner) {
        /* 直角：原地转向 */
        PID_Reset(&s_track_pid);
        if (err < 0) {
            left_cmd = -LOST_TURN_PWM;
            right_cmd = LOST_TURN_PWM;
        } else {
            left_cmd = LOST_TURN_PWM;
            right_cmd = -LOST_TURN_PWM;
        }
    } else {
        /*
         * 正常循迹:
         * err>0 线在右 → 需右转 → 左快右慢
         * pid 目标 0，feedback=err → out = K*(0-err) = -K*err
         * left = base - out = base + K*err  (err>0 时左加速)
         * right = base + out = base - K*err
         */
        base = ((err > CORNER_ERR) || (err < -CORNER_ERR)) ? CORNER_BASE : s_base_speed;
        pid_out = PID_CalcLinear(&s_track_pid, (float)err);
        left_cmd = (int16_t)((float)base - pid_out);
        right_cmd = (int16_t)((float)base + pid_out);
    }

    drive_lr(left_cmd, right_cmd);

    s_st.sensors = sensors;
    s_st.error = err;
    s_st.lost = lost;
    s_st.hard_corner = hard_corner;
    s_st.last_side = s_last_side;

    if (st) {
        *st = s_st;
    }
}
