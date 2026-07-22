#include "pid.h"

float normalize_angle_error(float target, float current)
{
    float err = target - current;
    if (err >  180.0f) err -= 360.0f;
    if (err < -180.0f) err += 360.0f;
    return err;
}

void PID_Init(PID_t *pid, float kp, float ki, float kd, float out_max, float target)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->target = target;
    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->integral = 0.0f;
    pid->output = 0.0f;
    pid->out_max = out_max;
}

float PID_Calc(PID_t *pid, float feedback)
{
    pid->error = normalize_angle_error(pid->target, feedback);

    pid->integral += pid->error;
    if (pid->integral >  pid->out_max / pid->ki) pid->integral =  pid->out_max / pid->ki;
    if (pid->integral < -pid->out_max / pid->ki) pid->integral = -pid->out_max / pid->ki;

    float derivative = pid->error - pid->last_error;
    pid->output = pid->kp * pid->error + pid->ki * pid->integral + pid->kd * derivative;

    if (pid->output >  pid->out_max) pid->output =  pid->out_max;
    if (pid->output < -pid->out_max) pid->output = -pid->out_max;

    pid->last_error = pid->error;
    return pid->output;
}