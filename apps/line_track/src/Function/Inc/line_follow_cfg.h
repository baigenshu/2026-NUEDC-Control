/**
 * @file line_follow_cfg.h
 * @brief 四路红外巡线参数 · 四轮速度 PID（编码器闭环）
 *
 * 传感器几何（左→右 p1..p4）：
 *   p1-p2 = 32.9mm  p2-p3 = 14.5mm  p3-p4 = 32.9mm
 *   线宽 ~18mm，白底黑线
 */
#ifndef LINE_FOLLOW_CFG_H
#define LINE_FOLLOW_CFG_H

/* ================================================================
 * 速度档位 — 编码器脉冲/控制周期（10ms）
 * ================================================================ */
#define SPD_GEAR_0                     (0)
#define SPD_GEAR_1                     (5)
#define SPD_GEAR_2                     (10)
#define SPD_GEAR_3                     (18)
#define SPD_GEAR_COUNT                 (4)

/* ================================================================
 * 传感器权重（左负右正，取自参考工程）
 *   外侧权重大（±8）、内侧小（±0.5），偏离时外侧主导
 * ================================================================ */
#define LF_W0                          (-8.0f)
#define LF_W1                          (-0.5f)
#define LF_W2                          (+0.5f)
#define LF_W3                          (+8.0f)

/* ================================================================
 * 位置 PID（线偏移 → 速度修正），位置式，积分限幅
 *   pid = LF_KP*error + LF_KI*integral + LF_KD*(error-last_error)
 *   增益取自参考工程，需上机精调
 * ================================================================ */
#define LF_KP                          (2.6f)
#define LF_KI                          (0.03f)
#define LF_KD                          (1.2f)
#define LF_I_MAX                       (15.0f)

/* ================================================================
 * 差速限幅（相对 base speed 的比例，有符号 → 允许内轮反转）
 *   left/right = base ∓ pid，pid 限幅 = base*LF_PID_OUT_FRAC
 *   左右轮速度限幅 = base*LF_MAX_SPD_FRAC
 * ================================================================ */
#define LF_PID_OUT_FRAC                (1.2f)
#define LF_MAX_SPD_FRAC                (1.25f)

/* ================================================================
 * 四轮速度 PID（编码器反馈 → PWM 占空比）
 *
 * 输入单位：encoder pulses / 10ms
 * 输出单位：PWM duty（直接送给 Motor_Set，含符号）
 *
 * 标定参考：PWM_MAX ≈ 满速 ≈ 60–80 pulses/10ms（MG310, 空载）
 *   Kp ≈ PWM_MAX / 80 * 0.5 ≈ 25
 * ================================================================ */
#define SPD_KP                         (25.0f)
#define SPD_KI                         (6.0f)
#define SPD_KD                         (3.0f)
#define SPD_I_MAX                      (800.0f)   /* 积分限幅 (PWM) */
#define SPD_OUT_MAX                    (3800.0f)  /* 输出限幅 (PWM)，对齐 motor_cfg.h PWM_MAX */

#endif /* LINE_FOLLOW_CFG_H */
