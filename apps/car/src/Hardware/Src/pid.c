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

void PID_Reset(PID_t *pid)
{
    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->integral = 0.0f;
    pid->output = 0.0f;
}

static float pid_step(PID_t *pid, float error)
{
    float i_lim;

    pid->error = error;
    pid->integral += error;

    if (pid->ki > 1e-6f) {
        i_lim = pid->out_max / pid->ki;
        if (pid->integral >  i_lim) pid->integral =  i_lim;
        if (pid->integral < -i_lim) pid->integral = -i_lim;
    } else {
        pid->integral = 0.0f;
    }

    {
        float derivative = pid->error - pid->last_error;
        pid->output = pid->kp * pid->error
                    + pid->ki * pid->integral
                    + pid->kd * derivative;
    }

    if (pid->output >  pid->out_max) pid->output =  pid->out_max;
    if (pid->output < -pid->out_max) pid->output = -pid->out_max;

    pid->last_error = pid->error;
    return pid->output;
}

float PID_Calc(PID_t *pid, float feedback)
{
    return pid_step(pid, normalize_angle_error(pid->target, feedback));
}

float PID_CalcLinear(PID_t *pid, float feedback)
{
    return pid_step(pid, pid->target - feedback);
}
