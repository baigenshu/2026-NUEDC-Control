#ifndef __PID_H
#define __PID_H

#include <stdint.h>

typedef struct {
    float kp;
    float ki;
    float kd;
    float target;
    float error;
    float last_error;
    float integral;
    float output;
    float out_max;
} PID_t;

float normalize_angle_error(float target, float current);
void PID_Init(PID_t *pid, float kp, float ki, float kd, float out_max, float target);

/* 角度环：误差做 ±180 归一化（IMU 航向） */
float PID_Calc(PID_t *pid, float feedback);

/* 线性环：循迹/速度等普通误差，不做角度折叠 */
float PID_CalcLinear(PID_t *pid, float feedback);

void PID_Reset(PID_t *pid);

#endif
