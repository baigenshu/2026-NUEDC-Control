/**
 * @file chassis.h
 * @brief 四轮差速底盘
 *
 * 符号：线速度 +前 -后；转向 +左 -右
 * 轮位：A 左后 · B 左前 · C 右前 · D 右后；左=A+B · 右=C+D
 */
#ifndef CHASSIS_H
#define CHASSIS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CHASSIS_STOP_COAST = 0,
    CHASSIS_STOP_BRAKE = 1,
} chassis_stop_mode_t;

typedef struct {
    int32_t a, b, c, d;
    int32_t left, right;
    float   dist_cm;
    float   heading_deg;
} chassis_odom_t;

void Chassis_Init(void);
void Chassis_Enable(bool on);

/** 每控制周期调用一次：刷新 odom，并持续输出当前差速指令 */
void Chassis_Update(uint32_t dt_ms);

void Chassis_Stop(chassis_stop_mode_t mode);
void Chassis_SetLR(int16_t left_pct, int16_t right_pct);
/** throttle=前后，turn=+左 -右（单位均为速度 %） */
void Chassis_Arcade(int16_t throttle, int16_t turn);

void Chassis_ResetOdom(void);
void Chassis_GetOdom(chassis_odom_t *o);
float Chassis_GetDistCm(void);

#endif /* CHASSIS_H */
