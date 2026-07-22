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
float PID_Calc(PID_t *pid, float feedback);

#endif