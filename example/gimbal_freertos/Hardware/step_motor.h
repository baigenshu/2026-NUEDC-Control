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
 * 第二路 (stepper_id = 2)  直驱（无齿轮）
 *   PA12 PWM/STEP  (TIMG0)
 *   PA13 DIR
 *   PA14 DCY
 *   PA15 SLP
 *   PA16 RST
 *
 * 电机本体：6400 pulse/rev → 0.05625°/pulse
 *
 * 角度/速度接口约定：
 *   传入的 angle / speed 一律按「负载侧 / 云台侧」物理量。
 *   电机1 内部自动 ×4（减速比），电机2 为 1:1。
 */

#include "ti_msp_dl_config.h"
#include <stdint.h>

/* 电机本体：每脉冲角度 */
#define STEP_DEG_PER_PULSE          0.05625f
#define STEP_PULSE_PER_REV          6400U

/* 电机1 Yaw 齿轮：小齿轮:大齿轮 = 1:4 → 负载转 θ，电机转 4θ */
#define STEP_MOTOR1_GEAR_RATIO      4.0f
#define STEP_MOTOR2_GEAR_RATIO      1.0f

void step_motor_init(void);
void step_motor_dir_set(uint8_t direction, uint8_t stepper_id);
void step_motor_start(uint8_t stepper_id);
void step_motor_stop(uint8_t stepper_id);

/* speed：云台/负载侧角速度，单位 °/s */
void step_set_speed(uint8_t speed, uint8_t stepper_id);

/* angle：云台/负载侧角度，单位 ° */
void step_motor_set_angle(uint8_t angle, uint8_t stepper_id);

#endif /* STEP_MOTOR_H */
