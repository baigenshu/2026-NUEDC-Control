#ifndef STEP_MOTOR_H
#define STEP_MOTOR_H

/*
 * 接线
 * 第一路 (stepper_id = 1)  Yaw，带 1:4 齿轮
 *   PA0  PWM/STEP  (TIMA0)
 *   PA1  DIR
 *   PA7  DCY
 *   PA8  SLP
 *   PA9  RST
 *
 * 第二路 (stepper_id = 2)  Pitch 直驱（无齿轮）
 *   PA12 PWM/STEP  (TIMG0)
 *   PA13 DIR
 *   PA14 DCY
 *   PA15 SLP
 *   PA16 RST
 *
 * 电机本体：6400 pulse/rev → 0.05625°/pulse
 *
 * 角度/速度接口约定：
 *   传入的 angle / speed 一律按"负载侧 / 云台侧"物理量。
 *   电机1 内部自动 ×4（减速比），电机2 为 1:1。
 *
 * 跟踪模式请用 step_set_velocity_f()：连续转，speed_dps 带符号 (float)。
 */

#include "ti_msp_dl_config.h"
#include <stdint.h>

/* 电机本体：每脉冲角度 */
#define STEP_DEG_PER_PULSE          0.05625f
#define STEP_PULSE_PER_REV          6400U

/* 电机1 Yaw 齿轮：小齿轮:大齿轮 = 1:4 → 负载转 θ，电机转 4θ */
#define STEP_MOTOR1_GEAR_RATIO      4.0f
#define STEP_MOTOR2_GEAR_RATIO      1.0f

/* 连续速度模式：remain 置此值表示不自动停 */
#define STEP_REMAIN_CONTINUOUS      0xFFFFFFFFu

void step_motor_init(void);
void step_motor_dir_set(uint8_t direction, uint8_t stepper_id);
void step_motor_start(uint8_t stepper_id);
void step_motor_stop(uint8_t stepper_id);

/* speed：云台/负载侧角速度，单位 °/s（无符号，配合 dir_set） */
void step_set_speed(uint16_t speed, uint8_t stepper_id);

/* angle：云台/负载侧相对角度，单位 °（走完脉冲后自动停） */
void step_motor_set_angle(uint16_t angle, uint8_t stepper_id);

/*
 * 连续速度（跟踪用）— float 版，推荐
 * speed_dps：负载侧 °/s，带符号
 *   >0 正转，<0 反转，0 停止
 */
void step_set_velocity_f(float speed_dps, uint8_t stepper_id);

/* 保留旧 int16 接口，内部转调 float 版 */
void step_motor_set_velocity(int16_t speed_dps, uint8_t stepper_id);

/* 是否还在跑脉冲（连续模式恒为 1，直到 stop） */
uint8_t step_motor_is_busy(uint8_t stepper_id);

#endif /* STEP_MOTOR_H */
