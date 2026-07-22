#include "ti_msp_dl_config.h"
#include "motor.h"
#include "OLED.h"
#include "bsp_systick.h"
#include "grayscale_sensor.h"
#include "pid.h"
#include "encoder.h"

/* ========== 循迹参数配置 ========== */
#define LEFT_BASE_SPEED   30    // 降速防过冲
#define RIGHT_BASE_SPEED  30
#define LEFT_MIN_SPEED    20    // 左轮死区（电机需要较高最低转速）
#define RIGHT_MIN_SPEED   10    // 右轮死区
#define MAX_SPEED         100

#define TRACK_KP          0.004f   // 降低比例系数, 减少摆动
#define TRACK_KI          0.0001f
#define TRACK_KD          0.005f   // 加大微分, 提前抑制过冲
#define TRACK_OUT_MAX     12       // 减小转向幅度

/* ========== 编码器速度闭环参数 ========== */
#define SPEED_KP          0.02f   // 速度环 P（降低）
#define SPEED_KI          0.005f  // 速度环 I（降低）
#define SPEED_KD          0.0f
#define SPEED_OUT_MAX     20      // 速度环最大PWM修正量（降低）

/* 编码器最大转速: PWM=100时对应脉冲/秒, 需实测校准 */
#define MAX_ENCODER_PPS   2000.0f

#define CONTROL_PERIOD    10      // 控制周期 ms

/* ========== 全局对象 ========== */
static PID_t track_pid;

/* 速度环状态（左右轮各一组, 不用 pid.c 的归一化） */
static float spd_integral_L = 0, spd_integral_R = 0;
static float spd_last_err_L = 0, spd_last_err_R = 0;

/* ========== 内部辅助: 限幅 ========== */
static inline float clampf(float v, float lo, float hi)
{
    if (v > hi) return hi;
    if (v < lo) return lo;
    return v;
}

/* ========== 速度PID单步（不做角度归一化） ========== */
static float speed_pid_step(float *integral, float *last_err,
                            float error, float kp, float ki, float kd)
{
    *integral += error;
    if (ki > 0.001f)
        *integral = clampf(*integral, -SPEED_OUT_MAX / ki, SPEED_OUT_MAX / ki);
    else
        *integral = clampf(*integral, -SPEED_OUT_MAX, SPEED_OUT_MAX);

    float deriv = error - *last_err;
    *last_err = error;
    return clampf(kp * error + ki * (*integral) + kd * deriv,
                  -SPEED_OUT_MAX, SPEED_OUT_MAX);
}

/* ========== 系统初始化 ========== */
static void System_Init(void)
{
    SYSCFG_DL_init();
    Motor_Init();
    Encoder_Init();
    OLED_Init();
    OLED_Clear();

    PID_Init(&track_pid, TRACK_KP, TRACK_KI, TRACK_KD, TRACK_OUT_MAX, 0.0f);

    OLED_ShowString(1, 1, "TRACK+ENC READY");
    delay_ms(1000);
    OLED_Clear();
}

/* ========== 循迹 + 编码器速度闭环控制 ========== */
static void Tracking_Loop(void)
{
    uint8_t  sensor_val;
    int16_t  track_error;
    float    pid_out;
    int16_t  left_cmd, right_cmd;
    float    target_pps_L, target_pps_R;
    float    actual_pps_L, actual_pps_R;
    float    err_L, err_R, corr_L, corr_R;
    int16_t  final_L, final_R;

    /* ---- 1. 更新编码器测速 ---- */
    Encoder_UpdateSpeed();

    /* ---- 2. 读取灰度传感器 + 计算循迹偏差 ---- */
    sensor_val = Read_Sensors();
    track_error = Get_Tracking_Error(sensor_val);

    /* ---- 3. 丢线保护 ---- */
    if (Is_Tracking_Lost(sensor_val)) {
        track_pid.integral = 0;
        pid_out = 0;
    } else {
        pid_out = PID_Calc(&track_pid, (float)track_error);
    }

    /* ---- 4. 双基准 + PID差速分配 = 指令速度 ---- */
    left_cmd  = LEFT_BASE_SPEED  - (int16_t)pid_out;
    right_cmd = RIGHT_BASE_SPEED + (int16_t)pid_out;

    if (left_cmd  > MAX_SPEED) left_cmd  = MAX_SPEED;
    if (left_cmd  < LEFT_MIN_SPEED)  left_cmd  = LEFT_MIN_SPEED;
    if (right_cmd > MAX_SPEED) right_cmd = MAX_SPEED;
    if (right_cmd < RIGHT_MIN_SPEED) right_cmd = RIGHT_MIN_SPEED;

    /* ---- 5. 指令速度 → 目标编码器转速 ---- */
    target_pps_L = (float)left_cmd  * MAX_ENCODER_PPS / 100.0f;
    target_pps_R = (float)right_cmd * MAX_ENCODER_PPS / 100.0f;

    /* ---- 6. 读取实际编码器转速（取绝对值） ---- */
    actual_pps_L = EncoderB_Speed; if (actual_pps_L < 0) actual_pps_L = -actual_pps_L;
    actual_pps_R = EncoderA_Speed; if (actual_pps_R < 0) actual_pps_R = -actual_pps_R;

    /* ---- 7. 左右轮各自速度PID ---- */
    err_L = target_pps_L - actual_pps_L;
    err_R = target_pps_R - actual_pps_R;

    corr_L = speed_pid_step(&spd_integral_L, &spd_last_err_L,
                            err_L, SPEED_KP, SPEED_KI, SPEED_KD);
    corr_R = speed_pid_step(&spd_integral_R, &spd_last_err_R,
                            err_R, SPEED_KP, SPEED_KI, SPEED_KD);

    /* ---- 8. 指令 PWM + PID补偿 = 最终输出 ---- */
    final_L = left_cmd  + (int16_t)corr_L;
    final_R = right_cmd + (int16_t)corr_R;

    if (final_L > MAX_SPEED) final_L = MAX_SPEED;
    if (final_L < LEFT_MIN_SPEED)  final_L = LEFT_MIN_SPEED;
    if (final_R > MAX_SPEED) final_R = MAX_SPEED;
    if (final_R < RIGHT_MIN_SPEED) final_R = RIGHT_MIN_SPEED;

    /* ---- 9. 驱动电机 ---- */
    MotorB_Forward((uint16_t)final_L);   // B = 左轮
    MotorA_Forward((uint16_t)final_R);   // A = 右轮

    /* ---- 10. OLED 调试: 编码器转速 ---- */
    OLED_ShowString(1, 1, "Er:");
    OLED_ShowNum(1, 4, track_error, 5);
    OLED_ShowString(2, 1, "L:");
    OLED_ShowNum(2, 3, (int16_t)actual_pps_L, 4);
    OLED_ShowString(2, 9, "R:");
    OLED_ShowNum(2, 11, (int16_t)actual_pps_R, 4);   // 两个转速值越接近 = 越直
    OLED_ShowString(3, 1, "cL:");
    OLED_ShowNum(3, 4, final_L, 3);
    OLED_ShowString(3, 9, "cR:");
    OLED_ShowNum(3, 12, final_R, 3);
}

/* ========== 主函数 ========== */
int main(void)
{
    System_Init();
    while (1) {
        Tracking_Loop();
        delay_ms(CONTROL_PERIOD);
    }
}
