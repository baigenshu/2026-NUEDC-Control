/**
 * @file motor.h
 * @brief 四轮电机底层：TB6612 类 H 桥（IN1/IN2 + PWM + STBY）
 *
 * 轮位：A 右后 · B 右前 · C 左前 · D 左后
 * 策略层应通过 Chassis 访问；本接口仅调试/自检可直调。
 */
#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MOTOR_ID_A = 0, /* 右后 */
    MOTOR_ID_B = 1, /* 右前 */
    MOTOR_ID_C = 2, /* 左前 */
    MOTOR_ID_D = 3, /* 左后 */
    MOTOR_ID_COUNT
} motor_id_t;

typedef enum {
    MOTOR_STOP_COAST = 0, /* IN1=IN2=0, PWM=0 */
    MOTOR_STOP_BRAKE = 1, /* IN1=IN2=1, PWM=0 */
} motor_stop_mode_t;

void Motor_Init(void);

/** STBY 使能（true=运行，false=待机） */
void Motor_SetEnable(bool on);

/**
 * 单轮有符号占空：
 * - duty > 0：正转（IN1=0, IN2=1）
 * - duty < 0：反转（IN1=1, IN2=0）
 * - duty == 0：COAST（IN1=IN2=0）
 * 死区与 PWM_MAX 在本层处理；极性 POL_* 由 Chassis 在调用前乘入。
 */
void Motor_Set(motor_id_t id, int16_t duty);

/** 四轮同时停车 */
void Motor_StopAll(motor_stop_mode_t mode);

#endif /* MOTOR_H */
