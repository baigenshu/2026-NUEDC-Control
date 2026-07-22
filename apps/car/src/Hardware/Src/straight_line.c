/**
 * ============================================================
 *  straight_line.c — 小车走直线模块
 *
 *  功能:
 *   模式0: 开环直行 — 两轮固定相同PWM
 *   模式1: 编码器闭环 — 左右轮速度PID同步
 *   模式2: 编码器+IMU闭环 — 速度同步 + 航向角修正
 *   附加:  定距停车 — 编码器脉冲计数达到目标后自动刹车
 *
 *  调用方式 (在 main 循环中):
 *   Encoder_UpdateSpeed();  // 先更新编码器速度
 *   StraightLine_Run();     // 再执行走直线控制
 *   delay_ms(10);
 * ============================================================
 */

#include "straight_line.h"
#include "motor.h"
#include "encoder.h"
#include "imu601.h"

/* ========== 全局控制器实例 ========== */
static StraightLine_t g_sl;

/* ========== 内部辅助: 浮点限幅 ========== */
static inline float clampf(float val, float min, float max)
{
    if (val > max) return max;
    if (val < min) return min;
    return val;
}

/* ========== 内部辅助: 速度PID单步计算（不包含角度归一化） ========== */
static float speed_pid_step(float *integral, float *last_error,
                            float error, float kp, float ki, float kd,
                            float out_max)
{
    float derivative;

    *integral += error;
    /* 积分限幅: 防止 windup */
    if (ki > 0.001f) {
        *integral = clampf(*integral, -out_max / ki, out_max / ki);
    } else {
        *integral = clampf(*integral, -out_max, out_max);
    }

    derivative = error - *last_error;
    *last_error = error;

    return clampf(kp * error + ki * (*integral) + kd * derivative,
                  -out_max, out_max);
}

/* ========== 初始化 ========== */
void StraightLine_Init(int16_t target_speed, uint8_t mode)
{
    StraightLine_t *sl = &g_sl;

    /* 速度环参数 */
    sl->speed_kp = SL_SPEED_KP;
    sl->speed_ki = SL_SPEED_KI;
    sl->speed_kd = SL_SPEED_KD;

    /* 航向环参数 */
    sl->yaw_kp = SL_YAW_KP;
    sl->yaw_ki = SL_YAW_KI;
    sl->yaw_kd = SL_YAW_KD;

    /* 目标 */
    sl->target_speed = (target_speed > 100) ? 100 :
                       (target_speed < 0)   ? 0   : target_speed;
    sl->target_encoder_pps = sl->target_speed * SL_MAX_ENCODER_PPS / 100.0f;
    sl->target_yaw = 0.0f;
    sl->mode = mode;

    /* 速度环状态清零 */
    sl->speed_error[0]      = 0.0f;
    sl->speed_error[1]      = 0.0f;
    sl->speed_integral[0]   = 0.0f;
    sl->speed_integral[1]   = 0.0f;
    sl->speed_last_error[0] = 0.0f;
    sl->speed_last_error[1] = 0.0f;
    sl->speed_output[0]     = 0.0f;
    sl->speed_output[1]     = 0.0f;

    /* 航向环状态清零 */
    sl->yaw_error      = 0.0f;
    sl->yaw_integral   = 0.0f;
    sl->yaw_last_error = 0.0f;
    sl->yaw_output     = 0.0f;

    /* 距离控制 */
    sl->use_distance   = 0;
    sl->target_pulses  = 0;
    sl->start_encoder_avg = 0;
    sl->reached        = 0;
}

/* ========== 启动（记录初始状态） ========== */
void StraightLine_Start(void)
{
    StraightLine_t *sl = &g_sl;

    /* 记录当前航向角作为目标（IMU模式用） */
    sl->target_yaw = IMU601_Attitude.yaw;

    /* 记录起始编码器位置（定距用） */
    sl->start_encoder_avg = (EncoderB_Count + EncoderA_Count) / 2;

    /* 清零所有积分/误差状态，避免冷启动突变 */
    sl->speed_integral[0]   = 0.0f;
    sl->speed_integral[1]   = 0.0f;
    sl->speed_last_error[0] = 0.0f;
    sl->speed_last_error[1] = 0.0f;
    sl->speed_output[0]     = 0.0f;
    sl->speed_output[1]     = 0.0f;

    sl->yaw_integral   = 0.0f;
    sl->yaw_last_error = 0.0f;
    sl->yaw_output     = 0.0f;

    sl->reached = 0;
}

/* ========== 设置定距停车 ========== */
void StraightLine_SetDistance(float distance_mm)
{
    StraightLine_t *sl = &g_sl;

    if (distance_mm <= 0.0f) {
        sl->use_distance = 0;
        sl->target_pulses = 0;
        return;
    }

    /* 行驶距离 → 轮子转动圈数 → 编码器脉冲数 */
    float revolutions = distance_mm / SL_WHEEL_CIRCUMFERENCE;
    sl->target_pulses = (int32_t)(revolutions * SL_PULSES_PER_REV);
    sl->use_distance  = 1;

    /* 启动时再记录起始位置 */
    sl->start_encoder_avg = 0;
}

/* ========== 检查是否到达目标距离 ========== */
uint8_t StraightLine_Reached(void)
{
    return g_sl.reached;
}

/* ========== 在线改速度 ========== */
void StraightLine_SetSpeed(int16_t speed)
{
    StraightLine_t *sl = &g_sl;

    sl->target_speed = (speed > 100) ? 100 :
                       (speed < 0)   ? 0   : speed;
    sl->target_encoder_pps = sl->target_speed * SL_MAX_ENCODER_PPS / 100.0f;
}

/* ========== 获取控制器引用（OLED调试用） ========== */
StraightLine_t* StraightLine_GetCtrl(void)
{
    return &g_sl;
}

/* ========== 停止 ========== */
void StraightLine_Stop(void)
{
    MotorA_Brake();
    MotorB_Brake();

    g_sl.speed_integral[0] = 0.0f;
    g_sl.speed_integral[1] = 0.0f;
    g_sl.yaw_integral      = 0.0f;
}

/* ========== 核心: 走直线周期调用 ========== */
void StraightLine_Run(void)
{
    StraightLine_t *sl = &g_sl;

    int16_t base_speed;      /* 基准PWM */
    float  diff_correction;  /* 编码器速度差 → PWM修正 */
    float  yaw_correction;   /* IMU航向修正量 */
    int16_t left_pwm, right_pwm;

    /* ---- 1. 定距停车检查 ---- */
    if (sl->use_distance && !sl->reached) {
        int32_t cur_avg = (EncoderB_Count + EncoderA_Count) / 2;
        int32_t traveled = cur_avg - sl->start_encoder_avg;

        /* 使用绝对值，兼容正反转 */
        if (traveled < 0) traveled = -traveled;

        if (traveled >= sl->target_pulses) {
            sl->reached = 1;
            MotorA_Brake();
            MotorB_Brake();
            return;
        }
    }

    if (sl->reached) {
        /* 到达后保持刹车 */
        MotorA_Brake();
        MotorB_Brake();
        return;
    }

    /* ---- 2. 基准速度 ---- */
    base_speed = sl->target_speed;
    if (base_speed == 0) {
        MotorA_Stop();
        MotorB_Stop();
        return;
    }

    /* ---- 3. 根据模式计算修正量 ---- */
    diff_correction = 0.0f;
    yaw_correction  = 0.0f;

    if (sl->mode == SL_MODE_OPEN_LOOP) {
        /* 模式0: 开环，不修正 */
        left_pwm  = base_speed;
        right_pwm = base_speed;
        goto APPLY_MOTOR;
    }

    /* ======== 模式1/2: 编码器速度闭环 ======== */
    {
        float left_actual, right_actual;
        float left_error, right_error;
        float avg_speed_error;

        /* 读取实际编码器转速 (脉冲/秒) */
        left_actual  = EncoderB_Speed;  /* 左轮编码器 */
        right_actual = EncoderA_Speed;  /* 右轮编码器 */

        /*
         * 方案: 分别对左右轮做速度PID，目标都是 target_encoder_pps。
         * 同时加入一个"同步修正"项 —— 两轮实际速度差也会反馈调节。
         *
         * left_error:  左轮实际 vs 目标（正值=左轮偏慢）
         * right_error: 右轮实际 vs 目标（正值=右轮偏慢）
         *
         * PID输出 = PWM补偿量，加到对应轮的基准PWM上。
         * 左右输出符号相反 → 差速修正。
         */

        /* 把编码器速度取绝对值（因为 Forward 时为正转速） */
        if (left_actual  < 0) left_actual  = -left_actual;
        if (right_actual < 0) right_actual = -right_actual;

        left_error  = sl->target_encoder_pps - left_actual;
        right_error = sl->target_encoder_pps - right_actual;

        /* 左右独立速度PID */
        sl->speed_output[0] = speed_pid_step(
            &sl->speed_integral[0], &sl->speed_last_error[0],
            left_error,
            sl->speed_kp, sl->speed_ki, sl->speed_kd,
            (float)SL_SPEED_OUTMAX);

        sl->speed_output[1] = speed_pid_step(
            &sl->speed_integral[1], &sl->speed_last_error[1],
            right_error,
            sl->speed_kp, sl->speed_ki, sl->speed_kd,
            (float)SL_SPEED_OUTMAX);

        /* 同步修正：两轮速度差驱动它们互相靠拢 */
        avg_speed_error = (left_actual - right_actual) * 0.5f;
        diff_correction = avg_speed_error * sl->speed_kp * 0.5f;

        /* 限幅 */
        diff_correction = clampf(diff_correction,
                                 -(float)SL_SPEED_OUTMAX,
                                  (float)SL_SPEED_OUTMAX);
    }

    /* ======== 模式2: IMU航向角修正 ======== */
    if (sl->mode == SL_MODE_IMU) {
        float current_yaw = IMU601_Attitude.yaw;
        float yaw_err = sl->target_yaw - current_yaw;

        /* 角度归一化到 [-180, 180] */
        if (yaw_err >  180.0f) yaw_err -= 360.0f;
        if (yaw_err < -180.0f) yaw_err += 360.0f;

        yaw_correction = speed_pid_step(
            &sl->yaw_integral, &sl->yaw_last_error,
            yaw_err,
            sl->yaw_kp, sl->yaw_ki, sl->yaw_kd,
            (float)SL_YAW_OUTMAX);

        sl->yaw_error  = yaw_err;
        sl->yaw_output = yaw_correction;
    }

    /* ---- 4. 合成最终PWM ---- */
    /*
     * 左轮 = 基准 + 左速度PID补偿 - 差速修正 - 航向修正
     * 右轮 = 基准 + 右速度PID补偿 + 差速修正 + 航向修正
     *
     * 航向逻辑: 车头偏右(yaw>target) → 修正量<0 → 左轮减速/右轮加速 → 车左转回正
     */
    left_pwm  = base_speed + (int16_t)sl->speed_output[0]
              - (int16_t)diff_correction - (int16_t)yaw_correction;
    right_pwm = base_speed + (int16_t)sl->speed_output[1]
              + (int16_t)diff_correction + (int16_t)yaw_correction;

    /* ---- 5. 输出限幅 ---- */
    if (left_pwm  > 100) left_pwm  = 100;
    if (left_pwm  < 0)   left_pwm  = 0;
    if (right_pwm > 100) right_pwm = 100;
    if (right_pwm < 0)   right_pwm = 0;

APPLY_MOTOR:
    /* ---- 6. 驱动电机 ---- */
    MotorB_Forward((uint16_t)left_pwm);   /* B = 左轮 */
    MotorA_Forward((uint16_t)right_pwm);  /* A = 右轮 */
}
