/**
 * @file ball_ctrl_cfg.h
 * @brief 视觉球位 → 曲柄倾角 停球参数（1/16 微步 · 中心稳定迭代）
 */
#ifndef BALL_CTRL_CFG_H
#define BALL_CTRL_CFG_H

#ifndef BALL_CTRL_DEFAULT_TARGET_MM_X100
#define BALL_CTRL_DEFAULT_TARGET_MM_X100 (0)
#endif

#ifndef BALL_CTRL_DEFAULT_FRAME_DT_MS
#define BALL_CTRL_DEFAULT_FRAME_DT_MS (40u)
#endif
#ifndef BALL_CTRL_PREDICT_MS
#define BALL_CTRL_PREDICT_MS (45u)
#endif
#ifndef BALL_CTRL_POS_ALPHA
#define BALL_CTRL_POS_ALPHA (0.45f)
#endif
#ifndef BALL_CTRL_VEL_ALPHA
#define BALL_CTRL_VEL_ALPHA (0.40f)
#endif
#ifndef BALL_CTRL_VEL_LIMIT_MM_S
#define BALL_CTRL_VEL_LIMIT_MM_S (220.0f)
#endif

/*
 * 均值已近 0，本轮专压 ±80mm 振荡：
 *  降 Kp、升 Kd、更早更强制动、bias 仅低速学习
 */
#ifndef BALL_CTRL_KP_POS
#define BALL_CTRL_KP_POS (0.014f)
#endif
#ifndef BALL_CTRL_KP_NEAR_POS
#define BALL_CTRL_KP_NEAR_POS (0.032f)
#endif
#ifndef BALL_CTRL_NEAR_ERR_MM
#define BALL_CTRL_NEAR_ERR_MM (48.0f)
#endif
#ifndef BALL_CTRL_KD_POS
#define BALL_CTRL_KD_POS (0.060f)
#endif
#ifndef BALL_CTRL_KD_NEAR_POS
#define BALL_CTRL_KD_NEAR_POS (0.300f)
#endif

/* 更早、更强的速度制动 */
#ifndef BALL_CTRL_BRAKE_ERR_MM
#define BALL_CTRL_BRAKE_ERR_MM (65.0f)
#endif
#ifndef BALL_CTRL_BRAKE_VEL_MM_S
#define BALL_CTRL_BRAKE_VEL_MM_S (15.0f)
#endif
#ifndef BALL_CTRL_BRAKE_ROD_MM_X100
#define BALL_CTRL_BRAKE_ROD_MM_X100 (90)
#endif

/* kick 更保守，只在真正卡住时用 */
#ifndef BALL_CTRL_KICK_ERR_MM
#define BALL_CTRL_KICK_ERR_MM (70.0f)
#endif
#ifndef BALL_CTRL_KICK_VEL_MM_S
#define BALL_CTRL_KICK_VEL_MM_S (8.0f)
#endif
#ifndef BALL_CTRL_KICK_ROD_MM_X100
#define BALL_CTRL_KICK_ROD_MM_X100 (70)
#endif

#ifndef BALL_CTRL_STICK_ERR_MM
#define BALL_CTRL_STICK_ERR_MM (3.0f)
#endif
#ifndef BALL_CTRL_STICK_VEL_MM_S
#define BALL_CTRL_STICK_VEL_MM_S (5.0f)
#endif
#ifndef BALL_CTRL_STICK_ROD_MM_X100
#define BALL_CTRL_STICK_ROD_MM_X100 (20)
#endif

/* bias：只在真正慢下来后学，防振荡中积分发散 */
#ifndef BALL_CTRL_BIAS_KI
#define BALL_CTRL_BIAS_KI (0.015f)
#endif
#ifndef BALL_CTRL_BIAS_DEADBAND_MM
#define BALL_CTRL_BIAS_DEADBAND_MM (2.0f)
#endif
#ifndef BALL_CTRL_BIAS_ERR_MAX_MM
#define BALL_CTRL_BIAS_ERR_MAX_MM (30.0f)
#endif
#ifndef BALL_CTRL_BIAS_VEL_MAX_MM_S
#define BALL_CTRL_BIAS_VEL_MAX_MM_S (10.0f)
#endif
#ifndef BALL_CTRL_BIAS_MAX_MM_X100
#define BALL_CTRL_BIAS_MAX_MM_X100 (40)
#endif

#ifndef BALL_CTRL_CONTROL_DEAD_MM
#define BALL_CTRL_CONTROL_DEAD_MM (1.5f)
#endif
#ifndef BALL_CTRL_SETTLED_ERR_MM
#define BALL_CTRL_SETTLED_ERR_MM (2.5f)
#endif
#ifndef BALL_CTRL_SETTLED_VEL_MM_S
#define BALL_CTRL_SETTLED_VEL_MM_S (4.0f)
#endif

/* 近点限幅收紧，减少过冲能量 */
#ifndef BALL_CTRL_NEAR_ROD_MAX_MM_X100
#define BALL_CTRL_NEAR_ROD_MAX_MM_X100 (45)
#endif
#ifndef BALL_CTRL_FAR_ROD_MAX_MM_X100
#define BALL_CTRL_FAR_ROD_MAX_MM_X100 (85)
#endif
#ifndef BALL_CTRL_ROD_POS_MAX_MM_X100
#define BALL_CTRL_ROD_POS_MAX_MM_X100 (90)
#endif
#ifndef BALL_CTRL_ROD_NEG_MAX_MM_X100
#define BALL_CTRL_ROD_NEG_MAX_MM_X100 (75)
#endif
#ifndef BALL_CTRL_ROD_MAX_MM_X100
#define BALL_CTRL_ROD_MAX_MM_X100 (BALL_CTRL_ROD_POS_MAX_MM_X100)
#endif
#ifndef BALL_CTRL_ROD_SLEW_MM_X100
#define BALL_CTRL_ROD_SLEW_MM_X100 (22)
#endif
#ifndef BALL_CTRL_ROD_SLEW_BRAKE_MM_X100
#define BALL_CTRL_ROD_SLEW_BRAKE_MM_X100 (40)
#endif
#ifndef BALL_CTRL_ROD_EPS_MM_X100
#define BALL_CTRL_ROD_EPS_MM_X100 (1)
#endif

#ifndef BALL_CTRL_TARGET_MIN_MM_X100
#define BALL_CTRL_TARGET_MIN_MM_X100 (-10000)
#endif
#ifndef BALL_CTRL_TARGET_MAX_MM_X100
#define BALL_CTRL_TARGET_MAX_MM_X100 (10000)
#endif

#ifndef BALL_CTRL_HOLD_LEVEL_ON_LOSS
#define BALL_CTRL_HOLD_LEVEL_ON_LOSS (1)
#endif
#ifndef BALL_CTRL_LOST_FRAME_GRACE
#define BALL_CTRL_LOST_FRAME_GRACE (2u)
#endif
#ifndef BALL_CTRL_LOST_DISABLE_MS
#define BALL_CTRL_LOST_DISABLE_MS (500u)
#endif
#ifndef BALL_CTRL_LINK_TIMEOUT_MS
#define BALL_CTRL_LINK_TIMEOUT_MS (200u)
#endif

#ifndef BALL_CTRL_STEPPER_SPS
#define BALL_CTRL_STEPPER_SPS (6000u)
#endif
#ifndef BALL_CTRL_STEPPER_ACCEL
#define BALL_CTRL_STEPPER_ACCEL (50000u)
#endif

#ifndef BALL_CTRL_SIGN
#define BALL_CTRL_SIGN (1.0f)
#endif

#endif /* BALL_CTRL_CFG_H */