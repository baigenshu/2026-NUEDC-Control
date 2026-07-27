/**
 * @file chassis.h
 * @brief 四轮差速底盘对外 API 与类型
 *
 * 符号：线速度 +前 -后；转向 +左 -右
 * 架构见 docs/plan.md · 契约见 docs/api.md
 */
#ifndef CHASSIS_H
#define CHASSIS_H

#include <stdint.h>
#include <stdbool.h>

/* ========== 状态 ========== */
typedef enum {
    CHASSIS_STATE_IDLE = 0,   /* 无输出保持 */
    CHASSIS_STATE_HOLD,       /* SetLR / Arcade / 持续 Go|Turn：Busy=false */
    CHASSIS_STATE_MOTION,     /* 定距 Go / 定角 Turn：Busy=true */
} chassis_state_t;

typedef enum {
    CHASSIS_STOP_DEFAULT = 0, /* → CHASSIS_DEFAULT_STOP_MODE */
    CHASSIS_STOP_COAST,
    CHASSIS_STOP_BRAKE,
} chassis_stop_mode_t;

typedef enum {
    CHASSIS_MODE_OPENLOOP = 0,
    CHASSIS_MODE_SPEED,       /* P4 可选 */
} chassis_speed_mode_t;

typedef enum {
    CHASSIS_SURFACE_NORMAL = 0,
    CHASSIS_SURFACE_LOW_GRIP,
    CHASSIS_SURFACE_HIGH_GRIP,
    CHASSIS_SURFACE_CUSTOM,
} chassis_surface_t;

typedef struct {
    float   turn_scale;
    float   spin_scale;    /* 只缩转速，不改目标角 */
    float   angle_gain;    /* 默认 1.0；慎用 */
    int16_t speed_limit;
} chassis_surface_params_t;

typedef struct {
    float    distance_cm;  /* 0=持续 HOLD；>0 路程 cm（恒正）→ MOTION */
    bool     straighten;   /* 定距时航向纠偏 */
    uint32_t timeout_ms;   /* 0=cfg 默认；仅 MOTION */
} chassis_go_opt_t;

typedef struct {
    uint32_t timeout_ms;   /* 0=cfg 默认；仅 MOTION */
} chassis_turn_opt_t;

typedef struct {
    int32_t a, b, c, d;      /* 四轮累计脉冲（含 ENC_SIGN） */
    int32_t left, right;     /* 侧向平均累计 */
    float   dist_cm;
    float   heading_deg;     /* 当前航向源 */
    float   v_left, v_right; /* 侧向速度 counts/s */
    float   yaw_imu_deg;     /* P5；未接为 0 */
    uint8_t imu_ready;       /* 0/1 */
    uint8_t slip;            /* 可选打滑标志 */
} chassis_odom_t;

/* ========== 生命周期 ========== */
void            Chassis_Init(void);
void            Chassis_Enable(bool on);
void            Chassis_Update(uint32_t dt_ms);
chassis_state_t Chassis_GetState(void);
bool            Chassis_Busy(void);
void            Chassis_Abort(void);

/* ========== 停车 / 差速 ========== */
void Chassis_Stop(chassis_stop_mode_t mode);
void Chassis_SetLR(int16_t left, int16_t right);
void Chassis_Arcade(int16_t throttle, int16_t turn);

/* ========== Go / Turn ========== */
void Chassis_Go(int16_t speed_pct, const chassis_go_opt_t *opt);
void Chassis_Turn(int16_t speed_pct, float angle_deg, const chassis_turn_opt_t *opt);

/* ========== 模式 · 配平 · 地面 · 里程 ========== */
void Chassis_SetSpeedMode(chassis_speed_mode_t m);
void Chassis_SetTrim(uint8_t left, uint8_t right);
void Chassis_SetSurface(chassis_surface_t s);
void Chassis_SetTurnBias(int8_t bias);
void Chassis_ResetOdom(void);
void Chassis_GetOdom(chassis_odom_t *o);

float Chassis_GetHeadingDeg(void);
void  Chassis_ResetHeading(void);

/* ========== 调试阻塞（可选）========== */
bool Chassis_GoBlock(int16_t speed, const chassis_go_opt_t *opt, uint32_t poll_ms);
bool Chassis_TurnBlock(int16_t speed, float angle_deg,
                       const chassis_turn_opt_t *opt, uint32_t poll_ms);

#endif /* CHASSIS_H */
