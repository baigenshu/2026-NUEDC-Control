/**
 * @file ball_ctrl_cfg.h
 * @brief cascade: pos PI -> v_des; vel PD -> rod
 */
#ifndef BALL_CTRL_CFG_H
#define BALL_CTRL_CFG_H

#ifndef BALL_CTRL_DT_MS
#define BALL_CTRL_DT_MS             (15u)
#endif
#ifndef BALL_CTRL_DEFAULT_TARGET_MM_X100
#define BALL_CTRL_DEFAULT_TARGET_MM_X100  (0)
#endif
#ifndef BALL_CTRL_KP_POS
#define BALL_CTRL_KP_POS            (1.0f)
#endif
#ifndef BALL_CTRL_KI_POS
#define BALL_CTRL_KI_POS            (0.10f)
#endif
#ifndef BALL_CTRL_V_DES_MAX
#define BALL_CTRL_V_DES_MAX         (35.0f)
#endif
#ifndef BALL_CTRL_I_SEP_MM
#define BALL_CTRL_I_SEP_MM          (25.0f)
#endif
#ifndef BALL_CTRL_I_LIM
#define BALL_CTRL_I_LIM             (40.0f)
#endif
#ifndef BALL_CTRL_KP_VEL
#define BALL_CTRL_KP_VEL            (0.035f)
#endif
#ifndef BALL_CTRL_KD_VEL
#define BALL_CTRL_KD_VEL            (0.025f)
#endif
#ifndef BALL_CTRL_KFF_VEL
#define BALL_CTRL_KFF_VEL           (0.008f)
#endif
#ifndef BALL_CTRL_DEAD_MM_X100
#define BALL_CTRL_DEAD_MM_X100      (100)
#endif
#ifndef BALL_CTRL_VEL_DEAD_MM_S
#define BALL_CTRL_VEL_DEAD_MM_S     (8.0f)
#endif
#ifndef BALL_CTRL_ROD_MAX_MM_X100
#define BALL_CTRL_ROD_MAX_MM_X100   (700)
#endif
#ifndef BALL_CTRL_ROD_SLEW_MM_X100
#define BALL_CTRL_ROD_SLEW_MM_X100  (7)
#endif
#ifndef BALL_CTRL_ROD_EPS_MM_X100
#define BALL_CTRL_ROD_EPS_MM_X100   (20)
#endif
#ifndef BALL_CTRL_TARGET_MIN_MM_X100
#define BALL_CTRL_TARGET_MIN_MM_X100  (-10000)
#endif
#ifndef BALL_CTRL_TARGET_MAX_MM_X100
#define BALL_CTRL_TARGET_MAX_MM_X100  ( 10000)
#endif
#ifndef BALL_CTRL_HOLD_LEVEL_ON_LOSS
#define BALL_CTRL_HOLD_LEVEL_ON_LOSS  (1)
#endif
#ifndef BALL_CTRL_SETTLE_DISABLE_MS
#define BALL_CTRL_SETTLE_DISABLE_MS (600u)
#endif
#ifndef BALL_CTRL_LOST_DISABLE_MS
#define BALL_CTRL_LOST_DISABLE_MS   (300u)
#endif
#ifndef BALL_CTRL_POS_ALPHA
#define BALL_CTRL_POS_ALPHA         (0.40f)
#endif
#ifndef BALL_CTRL_VEL_ALPHA
#define BALL_CTRL_VEL_ALPHA         (0.35f)
#endif
#ifndef BALL_CTRL_OUT_ALPHA
#define BALL_CTRL_OUT_ALPHA         (0.40f)
#endif
#ifndef BALL_CTRL_STEPPER_SPS
#define BALL_CTRL_STEPPER_SPS       (10000u)
#endif
#ifndef BALL_CTRL_STEPPER_ACCEL
#define BALL_CTRL_STEPPER_ACCEL     (30000u)
#endif
#ifndef BALL_CTRL_LINK_TIMEOUT_MS
#define BALL_CTRL_LINK_TIMEOUT_MS   (150u)
#endif
#ifndef BALL_CTRL_SIGN
#define BALL_CTRL_SIGN              (1.0f)
#endif

#endif