/**
 * @file motor_cfg.h
 * @brief 四路电机 / 编码器参数
 *
 * 轮位：A 左后 · B 左前 · C 右前 · D 右后
 */
#ifndef MOTOR_CFG_H
#define MOTOR_CFG_H

#define PWM_PERIOD                     (4000)
#define PWM_MAX                        (3800)
#define PWM_DEADZONE                   (80)

/* 开环调速档位：KEY_SPD 循环 0→1000→2000→3000→0 */
#define PWM_GEAR_0                     (0)
#define PWM_GEAR_1                     (1000)
#define PWM_GEAR_2                     (2000)
#define PWM_GEAR_3                     (3000)
#define PWM_GEAR_COUNT                 (4)

/* 实车标定：A/D 与 B/C 极性相反 */
#define POL_A                          (-1)
#define POL_B                          (+1)
#define POL_C                          (+1)
#define POL_D                          (-1)

/* MG310 霍尔：CPR=13，减速比 30，四倍频 */
#define MG310_HALL_CPR                 (13)
#define MG310_GEAR_RATIO               (30)
#define ENCODER_PPR                    (MG310_HALL_CPR * 4 * MG310_GEAR_RATIO)

/* 车体前进时脉冲应增加 */
#define ENC_SIGN_A                     (+1)
#define ENC_SIGN_B                     (+1)
#define ENC_SIGN_C                     (+1)
#define ENC_SIGN_D                     (+1)

#endif /* MOTOR_CFG_H */
