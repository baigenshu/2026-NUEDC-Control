#ifndef __STRAIGHT_LINE_H
#define __STRAIGHT_LINE_H

#include <stdint.h>

/* ========== 走直线模式选择 ========== */
#define SL_MODE_OPEN_LOOP   0   // 开环：两轮同PWM（简单但不精确）
#define SL_MODE_ENCODER     1   // 编码器速度闭环（推荐）
#define SL_MODE_IMU         2   // 编码器 + IMU航向闭环（最精确）

/* ========== 默认PID参数（需根据实际赛道和电池电压在线调试） ========== */
#define SL_SPEED_KP     0.03f   // 速度环P: 100PPS误差 → 3 PWM补偿
#define SL_SPEED_KI     0.01f   // 速度环I: 慢速消除静差（PPS→PWM）
#define SL_SPEED_KD     0.0f    // 速度环D（通常不需要，电机惯性即低通）
#define SL_SPEED_OUTMAX 30      // 速度环输出限幅（最大PWM补偿量，防止单轮飞转）

#define SL_YAW_KP       3.0f    // 航向环P: 1°偏航 → 3 PWM修正
#define SL_YAW_KI       0.02f   // 航向环I: 慢速修正IMU温漂
#define SL_YAW_KD       1.0f    // 航向环D: 抑制来回摆动
#define SL_YAW_OUTMAX   25      // 航向环输出限幅（避免大角度时剧烈转向）

/*
 * 编码器最大转速（PWM=100时的脉冲/秒）
 * 校准方法: 全速跑直线, 用OLED显示 EncoderA_Speed, 取左右均值
 */
#define SL_MAX_ENCODER_PPS  2000.0f

/* 编码器线数：轮胎一圈的脉冲数（用于距离计算） */
#define SL_PULSES_PER_REV   1560    // 需根据实际编码器+减速比校准

/* 轮胎周长（单位：mm） */
#define SL_WHEEL_CIRCUMFERENCE  210.0f  // 需实测校准

/* ========== 走直线控制器结构体 ========== */
typedef struct {
    float speed_kp, speed_ki, speed_kd;     // 速度环PID参数
    float yaw_kp, yaw_ki, yaw_kd;           // 航向环PID参数

    int16_t target_speed;       // 目标速度 (0-100, PWM%)
    float   target_encoder_pps; // 目标编码器转速 (脉冲/秒)
    float   target_yaw;         // 目标航向角（启动时自动记录）
    uint8_t mode;               // 模式: 0=开环, 1=编码器, 2=编码器+IMU

    /* 速度环状态（左右轮各一个） */
    float speed_error[2];       // [0]=左, [1]=右
    float speed_integral[2];
    float speed_last_error[2];
    float speed_output[2];      // PID输出，叠加到基准速度上

    /* 航向环状态 */
    float yaw_error;
    float yaw_integral;
    float yaw_last_error;
    float yaw_output;

    /* 距离控制 */
    uint8_t  use_distance;      // 是否启用定距停车
    int32_t  target_pulses;     // 目标脉冲数
    int32_t  start_encoder_avg; // 起始编码器平均值
    uint8_t  reached;           // 是否已到达目标距离
} StraightLine_t;

/* ========== API ========== */

/**
 * 初始化走直线控制器
 * target_speed: 目标速度 0-100（PWM占空比百分比）
 * mode: SL_MODE_OPEN_LOOP / SL_MODE_ENCODER / SL_MODE_IMU
 */
void StraightLine_Init(int16_t target_speed, uint8_t mode);

/**
 * 启动走直线（记录初始航向/编码器）
 */
void StraightLine_Start(void);

/**
 * 走直线控制循环 —— 每 10ms 调用一次
 * 调用前确保已调用 Encoder_UpdateSpeed() 更新编码器速度
 */
void StraightLine_Run(void);

/**
 * 停止走直线，电机刹车
 */
void StraightLine_Stop(void);

/**
 * 设置定距停车
 * distance_mm: 行驶距离（毫米），到达后自动停车
 */
void StraightLine_SetDistance(float distance_mm);

/**
 * 检查是否已到达目标距离
 * 返回: 1=已到达, 0=未到达/未启用定距
 */
uint8_t StraightLine_Reached(void);

/**
 * 在线修改目标速度
 */
void StraightLine_SetSpeed(int16_t speed);

/**
 * 获取当前控制器引用（用于在线调参/OLED显示）
 */
StraightLine_t* StraightLine_GetCtrl(void);

#endif /* __STRAIGHT_LINE_H */
