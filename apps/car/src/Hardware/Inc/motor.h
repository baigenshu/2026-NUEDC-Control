/**
 * @file motor.h
 * @brief 四轮 TB6612 类 H 桥
 *
 * A 左后 PA12 · B 左前 PA21 · C 右前 PA13 · D 右后 PA22 · STBY PB16
 */
#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MOTOR_ID_A = 0,
    MOTOR_ID_B = 1,
    MOTOR_ID_C = 2,
    MOTOR_ID_D = 3,
    MOTOR_ID_COUNT
} motor_id_t;

typedef enum {
    MOTOR_STOP_COAST = 0,
    MOTOR_STOP_BRAKE = 1,
} motor_stop_mode_t;

void Motor_Init(void);
void Motor_SetEnable(bool on);
/** duty: +正转 -反转 0=COAST；死区/限幅在本层；POL 由 Chassis 乘入 */
void Motor_Set(motor_id_t id, int16_t duty);
void Motor_StopAll(motor_stop_mode_t mode);

#endif /* MOTOR_H */
