#include "line_track.h"
#include "motor.h"
#include "encoder.h"
#include "grayscale_sensor.h"
#include "pid.h"

/* ---- 直道速度 ---- */
#define BASE_SPEED_DEFAULT  22
#define MIN_SPEED          -30
#define MAX_SPEED           30

/* ---- 大弯/直角 ---- */
#define CORNER_ERR          1500
#define CORNER_BASE         12
#define LOST_TURN_PWM       22

#define MASK_LEFT_OUT       0x03u
#define MASK_RIGHT_OUT      0xC0u

/* ---- 循迹 PID ---- */
#define TRACK_KP            0.012f
#define TRACK_KI            0.0000f
#define TRACK_KD            0.018f
#define TRACK_OUT_MAX       35.0f

static PID_t s_track_pid;
static int8_t s_last_side;
static int16_t s_base_speed;
static LineTrack_Status_t s_st;

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
{
    if (v > hi) return hi;
    if (v < lo) return lo;
    return v;
}

static void drive_signed(int16_t left, int16_t right)
{
    left  = clamp_i16(left,  MIN_SPEED, MAX_SPEED);
    right = clamp_i16(right, MIN_SPEED, MAX_SPEED);
    MotorB_SetSpeed(left);
    MotorA_SetSpeed(right);
    s_st.left_cmd  = left;
    s_st.right_cmd = right;
}

void LineTrack_Init(void)
{
    PID_Init(&s_track_pid, TRACK_KP, TRACK_KI, TRACK_KD, TRACK_OUT_MAX, 0.0f);
    s_last_side  = 0;
    s_base_speed = BASE_SPEED_DEFAULT;
    s_st.left_cmd = s_st.right_cmd = 0;
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

const LineTrack_Status_t *LineTrack_GetStatus(void)
{
    return &s_st;
}

void LineTrack_Step(LineTrack_Status_t *st)
{
    uint8_t  sensors;
    int16_t  err;
    float    pid_out = 0.0f;
    int16_t  left_cmd, right_cmd;
    int16_t  base;
    uint8_t  lost;
    uint8_t  hard_corner = 0;

    Encoder_UpdateSpeed();

    sensors = Read_Sensors();
    err     = Get_Tracking_Error(sensors);
    lost    = Is_Tracking_Lost(sensors);

    if (!lost) {
        uint8_t only_left  = (sensors & MASK_LEFT_OUT)  && !(sensors & ~MASK_LEFT_OUT);
        uint8_t only_right = (sensors & MASK_RIGHT_OUT) && !(sensors & ~MASK_RIGHT_OUT);
        if (only_left) {
            err = -3500;
            hard_corner = 1;
        } else if (only_right) {
            err = 3500;
            hard_corner = 1;
        }
    }

    if (!lost && err != 0)
        s_last_side = (err > 0) ? 1 : -1;

    if (lost) {
        PID_Reset(&s_track_pid);
        if (s_last_side > 0) {
            left_cmd  =  LOST_TURN_PWM;
            right_cmd = -LOST_TURN_PWM;
        } else if (s_last_side < 0) {
            left_cmd  = -LOST_TURN_PWM;
            right_cmd =  LOST_TURN_PWM;
        } else {
            left_cmd  = s_base_speed / 2;
            right_cmd = s_base_speed / 2;
        }
    } else {
        base = (hard_corner || (err > CORNER_ERR) || (err < -CORNER_ERR))
               ? CORNER_BASE : s_base_speed;

        pid_out = PID_CalcLinear(&s_track_pid, (float)err);
        left_cmd  = (int16_t)(base - (int16_t)pid_out);
        right_cmd = (int16_t)(base + (int16_t)pid_out);

        if (hard_corner) {
            if (err < 0) {
                left_cmd  = -LOST_TURN_PWM;
                right_cmd =  LOST_TURN_PWM;
            } else {
                left_cmd  =  LOST_TURN_PWM;
                right_cmd = -LOST_TURN_PWM;
            }
        }
    }

    drive_signed(left_cmd, right_cmd);

    s_st.sensors    = sensors;
    s_st.error      = err;
    s_st.lost       = lost;
    s_st.hard_corner = hard_corner;
    s_st.last_side  = s_last_side;

    if (st)
        *st = s_st;
}
