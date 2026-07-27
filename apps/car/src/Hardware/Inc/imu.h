/**
 * @file imu.h
 * @brief IMU / 航向抽象（P5 预留）
 *
 * IMU_ENABLED=0 时为空实现：Init/Update 空，DataReady=false。
 * 符号：yaw_deg 左转增加；可用 IMU_YAW_SIGN 翻转。
 */
#ifndef IMU_H
#define IMU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float yaw_deg;       /* 相对零点，可多圈 unwrap */
    float yaw_rate_dps;  /* 可选 */
    float pitch_deg;     /* 可选 */
    float roll_deg;      /* 可选 */
} imu_state_t;

void  Imu_Init(void);
void  Imu_Calibrate(void);
void  Imu_Update(uint32_t dt_ms);
bool  Imu_DataReady(void);
void  Imu_Get(imu_state_t *out);
float Imu_GetYawDeg(void);
void  Imu_ResetYaw(void);

#endif /* IMU_H */
