/**
 * @file chassis.c
 * @brief 四轮差速底盘：状态机 · 差速 · odom · Go/Turn
 *
 * 命令 owner 防止巡线盖遥控；MOTION 仅在 Update 推进。
 */
#include "chassis.h"
#include "chassis_cfg.h"
#include "motor.h"
#include "encoder.h"
#include "imu.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ---------- 内部 owner ---------- */
typedef enum {
    CH_OWNER_NONE = 0,
    CH_OWNER_MANUAL,
    CH_OWNER_MOTION,
    CH_OWNER_LINE,
} ch_owner_t;

typedef enum {
    MOTION_NONE = 0,
    MOTION_GO,
    MOTION_TURN,
} motion_kind_t;

/* ---------- 运行时状态 ---------- */
static chassis_state_t       s_state;
static ch_owner_t            s_owner;
static chassis_speed_mode_t  s_speed_mode;
static chassis_surface_t     s_surface_id;
static chassis_surface_params_t s_surf;
static uint8_t               s_trim_l;   /* 0..100 */
static uint8_t               s_trim_r;
static int8_t                s_turn_bias;
static bool                  s_enabled;

/* 当前 HOLD 差速命令（百分比） */
static int16_t s_cmd_left;
static int16_t s_cmd_right;

/* odom */
static int32_t s_prev_a, s_prev_b, s_prev_c, s_prev_d;
static bool    s_odom_inited;
static int32_t s_acc_a, s_acc_b, s_acc_c, s_acc_d;
static int32_t s_acc_l, s_acc_r;
static float   s_dist_cm;
static float   s_heading_enc_deg;
static float   s_v_left, s_v_right;
static uint8_t s_slip;

/* MOTION */
static motion_kind_t s_motion_kind;
static float    s_motion_target;   /* Go: cm 路程；Turn: 目标角增量 deg */
static float    s_motion_progress; /* 已走路程 cm 或已转角 deg */
static float    s_motion_heading0; /* Go straighten / Turn 起点航向 */
static int16_t  s_motion_speed;    /* 有符号速度命令 % */
static bool     s_motion_straighten;
static uint32_t s_motion_timeout_ms;
static uint32_t s_motion_elapsed_ms;

/* 速度环积分（P4） */
static float s_speed_i_l, s_speed_i_r;

/* 地面档表 */
static const chassis_surface_params_t s_surf_table[] = {
    SURF_NORMAL, /* NORMAL */
    SURF_LOW,    /* LOW_GRIP */
    SURF_HIGH,   /* HIGH_GRIP */
};

/* LineTrack 通过弱符号可选标记 owner；默认 MANUAL */
/* 内部 API：供 line_track 指定 owner */
void Chassis_ArcadeOwned(int16_t throttle, int16_t turn, ch_owner_t owner);

/* ================================================================ */
/* 工具                                                              */
/* ================================================================ */
static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
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

static int16_t pct_to_duty(int16_t pct)
{
    /* -100..+100 → -PWM_MAX..+PWM_MAX */
    int32_t d;
    if (pct > 100)  pct = 100;
    if (pct < -100) pct = -100;
    d = ((int32_t)pct * (int32_t)PWM_MAX) / 100;
    return (int16_t)d;
}

static motor_stop_mode_t map_stop(chassis_stop_mode_t mode)
{
    int m = (int)mode;
    if (m == (int)CHASSIS_STOP_DEFAULT)
        m = CHASSIS_DEFAULT_STOP_MODE;
    if (m == 2 /* BRAKE */ || m == (int)CHASSIS_STOP_BRAKE)
        return MOTOR_STOP_BRAKE;
    return MOTOR_STOP_COAST;
}

static float heading_now(void)
{
#if IMU_ENABLED && (CHASSIS_HEADING_SOURCE == 1)
    if (Imu_DataReady())
        return Imu_GetYawDeg();
#endif
    return s_heading_enc_deg;
}

static void load_surface(chassis_surface_t s)
{
    if (s == CHASSIS_SURFACE_CUSTOM) {
        /* 保留当前 s_surf */
        s_surface_id = s;
        return;
    }
    if ((unsigned)s >= 3u)
        s = CHASSIS_SURFACE_NORMAL;
    s_surface_id = s;
    s_surf = s_surf_table[s];
}

/* 差速 → 四轮（TRIM / POL），开环或速度环 */
static void apply_lr(int16_t left_pct, int16_t right_pct)
{
    int16_t lim = s_surf.speed_limit;
    int16_t l, r;
    int16_t duty_l, duty_r;
    int16_t duty_a, duty_b, duty_c, duty_d;

    if (lim < 1)
        lim = 1;
    l = clamp_i16(left_pct,  (int16_t)(-lim), lim);
    r = clamp_i16(right_pct, (int16_t)(-lim), lim);

    if (s_speed_mode == CHASSIS_MODE_SPEED) {
        /* P4 速度环：pct → 目标 counts/s，PI 输出 duty */
        float tgt_l = (float)l * SPEED_PCT_TO_COUNTS_PER_SEC / 100.f;
        float tgt_r = (float)r * SPEED_PCT_TO_COUNTS_PER_SEC / 100.f;
        float el = tgt_l - s_v_left;
        float er = tgt_r - s_v_right;
        float out_l, out_r;

        s_speed_i_l = clampf(s_speed_i_l + el * SPEED_KI, -SPEED_I_LIMIT, SPEED_I_LIMIT);
        s_speed_i_r = clampf(s_speed_i_r + er * SPEED_KI, -SPEED_I_LIMIT, SPEED_I_LIMIT);
        out_l = el * SPEED_KP + s_speed_i_l;
        out_r = er * SPEED_KP + s_speed_i_r;
        out_l = clampf(out_l, -(float)SPEED_OUT_LIMIT, (float)SPEED_OUT_LIMIT);
        out_r = clampf(out_r, -(float)SPEED_OUT_LIMIT, (float)SPEED_OUT_LIMIT);
        duty_l = (int16_t)out_l;
        duty_r = (int16_t)out_r;
        /* 速度环输出为 duty，再乘 TRIM 保持左右配平 */
        duty_l = (int16_t)(((int32_t)duty_l * (int32_t)s_trim_l) / 100);
        duty_r = (int16_t)(((int32_t)duty_r * (int32_t)s_trim_r) / 100);
    } else {
        /* 开环：百分比 → duty，再 × TRIM */
        duty_l = pct_to_duty(l);
        duty_r = pct_to_duty(r);
        duty_l = (int16_t)(((int32_t)duty_l * (int32_t)s_trim_l) / 100);
        duty_r = (int16_t)(((int32_t)duty_r * (int32_t)s_trim_r) / 100);
    }

    /* 左 = C,D · 右 = B,A；再 × POL */
    duty_c = (int16_t)(duty_l * (int32_t)POL_C);
    duty_d = (int16_t)(duty_l * (int32_t)POL_D);
    duty_b = (int16_t)(duty_r * (int32_t)POL_B);
    duty_a = (int16_t)(duty_r * (int32_t)POL_A);

    Motor_Set(MOTOR_ID_A, duty_a);
    Motor_Set(MOTOR_ID_B, duty_b);
    Motor_Set(MOTOR_ID_C, duty_c);
    Motor_Set(MOTOR_ID_D, duty_d);
}

static void enter_idle_stop(chassis_stop_mode_t mode)
{
    s_state  = CHASSIS_STATE_IDLE;
    s_owner  = CH_OWNER_NONE;
    s_motion_kind = MOTION_NONE;
    s_cmd_left = s_cmd_right = 0;
    s_speed_i_l = s_speed_i_r = 0.f;
    Motor_StopAll(map_stop(mode));
}

static void enter_hold(int16_t left, int16_t right, ch_owner_t owner)
{
    s_state  = CHASSIS_STATE_HOLD;
    s_owner  = owner;
    s_motion_kind = MOTION_NONE;
    s_cmd_left  = left;
    s_cmd_right = right;
    apply_lr(left, right);
}

/* ================================================================ */
/* 生命周期                                                          */
/* ================================================================ */
void Chassis_Init(void)
{
    Motor_Init();
    Encoder_Init();
    Imu_Init();

    s_state      = CHASSIS_STATE_IDLE;
    s_owner      = CH_OWNER_NONE;
    s_speed_mode = (chassis_speed_mode_t)CHASSIS_DEFAULT_SPEED_MODE;
    s_trim_l     = (uint8_t)LEFT_TRIM;
    s_trim_r     = (uint8_t)RIGHT_TRIM;
    s_turn_bias  = (int8_t)CHASSIS_TURN_BIAS;
    s_enabled    = false;
    s_cmd_left = s_cmd_right = 0;
    s_motion_kind = MOTION_NONE;
    s_speed_i_l = s_speed_i_r = 0.f;

    load_surface((chassis_surface_t)CHASSIS_SURFACE_DEFAULT);
    Chassis_ResetOdom();
    Motor_StopAll(map_stop(CHASSIS_STOP_DEFAULT));
}

void Chassis_Enable(bool on)
{
    s_enabled = on;
    Motor_SetEnable(on);
    if (!on)
        enter_idle_stop(CHASSIS_STOP_DEFAULT);
}

chassis_state_t Chassis_GetState(void) { return s_state; }
bool            Chassis_Busy(void)     { return s_state == CHASSIS_STATE_MOTION; }

void Chassis_Abort(void)
{
    enter_idle_stop(CHASSIS_STOP_DEFAULT);
}

void Chassis_Stop(chassis_stop_mode_t mode)
{
    enter_idle_stop(mode);
}

/* ================================================================ */
/* 差速                                                              */
/* ================================================================ */
void Chassis_SetLR(int16_t left, int16_t right)
{
    enter_hold(left, right, CH_OWNER_MANUAL);
}

void Chassis_Arcade(int16_t throttle, int16_t turn)
{
    Chassis_ArcadeOwned(throttle, turn, CH_OWNER_MANUAL);
}

void Chassis_ArcadeOwned(int16_t throttle, int16_t turn, ch_owner_t owner)
{
    float turn_eff;
    int16_t left, right;
    int16_t lim;

    /* 巡线不得覆盖 MOTION */
    if (owner == CH_OWNER_LINE && s_state == CHASSIS_STATE_MOTION)
        return;

    lim = s_surf.speed_limit;
    turn_eff = ((float)turn + (float)s_turn_bias) * s_surf.turn_scale;
    left  = (int16_t)((float)throttle + turn_eff);
    right = (int16_t)((float)throttle - turn_eff);
    left  = clamp_i16(left,  (int16_t)(-lim), lim);
    right = clamp_i16(right, (int16_t)(-lim), lim);

    enter_hold(left, right, owner);
}

/* 供 line_track 使用的包装（同编译单元可见；头文件不导出 owner） */
void Chassis_ArcadeFromLine(int16_t throttle, int16_t turn)
{
    Chassis_ArcadeOwned(throttle, turn, CH_OWNER_LINE);
}

/* ================================================================ */
/* Go / Turn                                                         */
/* ================================================================ */
void Chassis_Go(int16_t speed_pct, const chassis_go_opt_t *opt)
{
    float dist = 0.f;
    bool  straighten = false;
    uint32_t tmo = MOTION_TIMEOUT_MS_DEFAULT;

    if (opt) {
        dist       = opt->distance_cm;
        straighten = opt->straighten;
        if (opt->timeout_ms)
            tmo = opt->timeout_ms;
    }

    if (dist <= 0.f) {
        /* 持续直行 HOLD */
        enter_hold(speed_pct, speed_pct, CH_OWNER_MANUAL);
        return;
    }

    /* 定距 MOTION：路程恒正，方向=speed 符号 */
    s_state             = CHASSIS_STATE_MOTION;
    s_owner             = CH_OWNER_MOTION;
    s_motion_kind       = MOTION_GO;
    s_motion_target     = dist;
    s_motion_progress   = 0.f;
    s_motion_heading0   = heading_now();
    s_motion_speed      = speed_pct;
    s_motion_straighten = straighten;
    s_motion_timeout_ms = tmo;
    s_motion_elapsed_ms = 0;
    s_cmd_left = s_cmd_right = speed_pct;
    apply_lr(speed_pct, speed_pct);
}

void Chassis_Turn(int16_t speed_pct, float angle_deg, const chassis_turn_opt_t *opt)
{
    uint32_t tmo = MOTION_TIMEOUT_MS_DEFAULT;
    int16_t  spin;
    float    target;

    if (opt && opt->timeout_ms)
        tmo = opt->timeout_ms;

    if (angle_deg == 0.f) {
        /* 持续自旋：speed 符号 = 转向方向 */
        spin = speed_pct;
        /* 左正：left 负 / right 正 → 逆时针 */
        enter_hold((int16_t)(-spin), spin, CH_OWNER_MANUAL);
        return;
    }

    /* 定角：方向=angle 符号，speed 取绝对值 × spin_scale */
    {
        int16_t abs_spd = (speed_pct >= 0) ? speed_pct : (int16_t)(-speed_pct);
        float cmd = (float)abs_spd * s_surf.spin_scale;
        if (cmd > (float)s_surf.speed_limit)
            cmd = (float)s_surf.speed_limit;
        spin = (int16_t)cmd;
        if (angle_deg < 0.f)
            spin = (int16_t)(-spin);
    }

    target = angle_deg * s_surf.angle_gain; /* 默认 gain=1 */

    s_state             = CHASSIS_STATE_MOTION;
    s_owner             = CH_OWNER_MOTION;
    s_motion_kind       = MOTION_TURN;
    s_motion_target     = target;
    s_motion_progress   = 0.f;
    s_motion_heading0   = heading_now();
    s_motion_speed      = spin; /* 有符号：+左 */
    s_motion_straighten = false;
    s_motion_timeout_ms = tmo;
    s_motion_elapsed_ms = 0;
    /* +左：left 负, right 正 */
    s_cmd_left  = (int16_t)(-spin);
    s_cmd_right = spin;
    apply_lr(s_cmd_left, s_cmd_right);
}

/* ================================================================ */
/* 模式 / 配平 / 地面 / 里程                                          */
/* ================================================================ */
void Chassis_SetSpeedMode(chassis_speed_mode_t m)
{
    s_speed_mode = m;
    s_speed_i_l = s_speed_i_r = 0.f;
}

void Chassis_SetTrim(uint8_t left, uint8_t right)
{
    if (left > 100)  left = 100;
    if (right > 100) right = 100;
    s_trim_l = left;
    s_trim_r = right;
}

void Chassis_SetSurface(chassis_surface_t s)
{
    load_surface(s);
}

void Chassis_SetTurnBias(int8_t bias)
{
    s_turn_bias = bias;
}

void Chassis_ResetOdom(void)
{
    Encoder_ResetAll();
    s_prev_a = s_prev_b = s_prev_c = s_prev_d = 0;
    s_odom_inited = false;
    s_acc_a = s_acc_b = s_acc_c = s_acc_d = 0;
    s_acc_l = s_acc_r = 0;
    s_dist_cm = 0.f;
    s_heading_enc_deg = 0.f;
    s_v_left = s_v_right = 0.f;
    s_slip = 0;
#if IMU_ENABLED
    Imu_ResetYaw();
#endif
}

void Chassis_GetOdom(chassis_odom_t *o)
{
    if (!o)
        return;
    o->a = s_acc_a;
    o->b = s_acc_b;
    o->c = s_acc_c;
    o->d = s_acc_d;
    o->left  = s_acc_l;
    o->right = s_acc_r;
    o->dist_cm     = s_dist_cm;
    o->heading_deg = heading_now();
    o->v_left  = s_v_left;
    o->v_right = s_v_right;
#if IMU_ENABLED
    o->yaw_imu_deg = Imu_GetYawDeg();
    o->imu_ready   = Imu_DataReady() ? 1u : 0u;
#else
    o->yaw_imu_deg = 0.f;
    o->imu_ready   = 0u;
#endif
    o->slip = s_slip;
}

float Chassis_GetHeadingDeg(void)
{
    return heading_now();
}

void Chassis_ResetHeading(void)
{
    s_heading_enc_deg = 0.f;
#if IMU_ENABLED
    Imu_ResetYaw();
#endif
}

/* ================================================================ */
/* Update：测速 · odom · MOTION 推进                                  */
/* ================================================================ */
void Chassis_Update(uint32_t dt_ms)
{
    int32_t ca, cb, cc, cd;
    int32_t dA, dB, dC, dD;
    float pulse_L, pulse_R;
    float dL_mm, dR_mm, ds_mm, dth_rad;
    float dt_s;

    if (dt_ms == 0u)
        dt_ms = 1u;
    dt_s = (float)dt_ms * 0.001f;

#if IMU_ENABLED
    Imu_Update(dt_ms);
#endif

    Encoder_GetAll(&ca, &cb, &cc, &cd);

    if (!s_odom_inited) {
        s_prev_a = ca; s_prev_b = cb; s_prev_c = cc; s_prev_d = cd;
        s_odom_inited = true;
        /* 首次不积分 */
        if (s_state == CHASSIS_STATE_HOLD || s_state == CHASSIS_STATE_MOTION)
            apply_lr(s_cmd_left, s_cmd_right);
        return;
    }

    dA = ca - s_prev_a;
    dB = cb - s_prev_b;
    dC = cc - s_prev_c;
    dD = cd - s_prev_d;
    s_prev_a = ca; s_prev_b = cb; s_prev_c = cc; s_prev_d = cd;

    /* 累计 */
    s_acc_a += dA; s_acc_b += dB; s_acc_c += dC; s_acc_d += dD;

    /* 侧向平均：左 C+D，右 B+A */
    pulse_L = 0.5f * (float)(dC + dD);
    pulse_R = 0.5f * (float)(dB + dA);
    s_acc_l += (int32_t)(pulse_L + (pulse_L >= 0.f ? 0.5f : -0.5f));
    s_acc_r += (int32_t)(pulse_R + (pulse_R >= 0.f ? 0.5f : -0.5f));

    dL_mm = pulse_L * MM_PER_PULSE;
    dR_mm = pulse_R * MM_PER_PULSE;
    ds_mm = 0.5f * (dL_mm + dR_mm);
    dth_rad = (dR_mm - dL_mm) / (float)WHEELBASE_MM; /* +左 */

    s_dist_cm += ds_mm / 10.f;
    s_heading_enc_deg += dth_rad * (180.f / M_PI);

    s_v_left  = pulse_L / dt_s;
    s_v_right = pulse_R / dt_s;

    /* 同侧两轮差过大 → slip 标志 */
    {
        int32_t diff_l = dC - dD;
        int32_t diff_r = dB - dA;
        if (diff_l < 0) diff_l = -diff_l;
        if (diff_r < 0) diff_r = -diff_r;
        s_slip = (diff_l > 20 || diff_r > 20) ? 1u : 0u;
    }

    /* ---- MOTION 推进 ---- */
    if (s_state == CHASSIS_STATE_MOTION) {
        s_motion_elapsed_ms += dt_ms;

        if (s_motion_kind == MOTION_GO) {
            float step = ds_mm / 10.f;
            if (s_motion_speed < 0)
                step = -step; /* 倒车时进度仍向目标累加 */
            if (step < 0.f)
                step = -step;
            s_motion_progress += step;

            {
                float remain = s_motion_target - s_motion_progress;
                int16_t spd = s_motion_speed;
                int16_t turn_corr = 0;
                float scale = 1.f;

                /* 末端降速 */
                if (MOTION_SLOWDOWN_CM > 0.f && remain < MOTION_SLOWDOWN_CM &&
                    s_motion_target > 0.f) {
                    scale = remain / MOTION_SLOWDOWN_CM;
                    if (scale < 0.25f)
                        scale = 0.25f;
                }

                spd = (int16_t)((float)spd * scale);

                /* 直线纠偏 */
                if (s_motion_straighten) {
                    float herr = heading_now() - s_motion_heading0;
                    float kp = MOTION_STRAIGHT_KP;
#if IMU_ENABLED && (CHASSIS_HEADING_SOURCE == 1)
                    kp = CHASSIS_IMU_STRAIGHT_KP;
#endif
                    turn_corr = (int16_t)(-herr * kp); /* 航向偏左则加右偏 */
                }

                s_cmd_left  = (int16_t)(spd + turn_corr);
                s_cmd_right = (int16_t)(spd - turn_corr);
                apply_lr(s_cmd_left, s_cmd_right);

                if (remain <= MOTION_DIST_TOL_CM ||
                    s_motion_elapsed_ms >= s_motion_timeout_ms) {
                    enter_idle_stop((chassis_stop_mode_t)CHASSIS_MOTION_DONE_STOP_MODE);
                }
            }
        } else if (s_motion_kind == MOTION_TURN) {
            float d_yaw = heading_now() - s_motion_heading0;
            float remain;
            int16_t spin = s_motion_speed;

            /* 目标角有符号；progress = 已转过的有符号角 */
            s_motion_progress = d_yaw;
            if (s_motion_target >= 0.f)
                remain = s_motion_target - s_motion_progress;
            else
                remain = s_motion_progress - s_motion_target; /* 两者均为负向时 remain>0 表示未到 */

            /* 统一：到方位 |heading - h0 - target| < TOL */
            {
                float err = heading_now() - s_motion_heading0 - s_motion_target;
                if (err < 0.f) err = -err;

                s_cmd_left  = (int16_t)(-spin);
                s_cmd_right = spin;
                apply_lr(s_cmd_left, s_cmd_right);

                if (err <= MOTION_ANGLE_TOL_DEG ||
                    s_motion_elapsed_ms >= s_motion_timeout_ms) {
                    enter_idle_stop((chassis_stop_mode_t)CHASSIS_MOTION_DONE_STOP_MODE);
                }
            }
            (void)remain;
        }
    } else if (s_state == CHASSIS_STATE_HOLD) {
        /* 保持差速；速度环需每拍刷新 */
        apply_lr(s_cmd_left, s_cmd_right);
    }
    /* IDLE：已停，不刷 PWM */
}

/* ================================================================ */
/* 阻塞调试                                                          */
/* ================================================================ */
bool Chassis_GoBlock(int16_t speed, const chassis_go_opt_t *opt, uint32_t poll_ms)
{
    if (poll_ms == 0u)
        poll_ms = 10u;
    Chassis_Go(speed, opt);
    if (!Chassis_Busy())
        return true; /* 持续运动，非阻塞语义下视为已接受 */
    while (Chassis_Busy())
        Chassis_Update(poll_ms);
    return true;
}

bool Chassis_TurnBlock(int16_t speed, float angle_deg,
                       const chassis_turn_opt_t *opt, uint32_t poll_ms)
{
    if (poll_ms == 0u)
        poll_ms = 10u;
    Chassis_Turn(speed, angle_deg, opt);
    if (!Chassis_Busy())
        return true;
    while (Chassis_Busy())
        Chassis_Update(poll_ms);
    return true;
}
