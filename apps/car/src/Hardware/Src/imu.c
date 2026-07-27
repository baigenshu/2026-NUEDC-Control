/**
 * @file imu.c
 * @brief IMU 抽象实现（P5 预留 stub）
 *
 * IMU_ENABLED=0：空实现，DataReady 恒 false。
 * 接入真实模块时在此对接 UART/I2C 后端，保持对外接口不变。
 */
#include "imu.h"
#include "chassis_cfg.h"

#if IMU_ENABLED

/* --- 真实后端占位：接入后填充 --- */
static imu_state_t s_state;
static float       s_yaw0;
static bool        s_ready;

void Imu_Init(void)
{
    s_state.yaw_deg      = 0.f;
    s_state.yaw_rate_dps = 0.f;
    s_state.pitch_deg    = 0.f;
    s_state.roll_deg     = 0.f;
    s_yaw0  = 0.f;
    s_ready = false;
    /* TODO: 初始化总线 / 模块 */
}

void Imu_Calibrate(void)
{
    /* TODO: 静止校准 */
}

void Imu_Update(uint32_t dt_ms)
{
    (void)dt_ms;
    /* TODO: 读姿态，应用 IMU_YAW_SIGN，unwrap 相对 s_yaw0 */
}

bool Imu_DataReady(void)
{
    return s_ready;
}

void Imu_Get(imu_state_t *out)
{
    if (out)
        *out = s_state;
}

float Imu_GetYawDeg(void)
{
    return s_state.yaw_deg;
}

void Imu_ResetYaw(void)
{
    /* 将当前姿态记为零点 */
    s_yaw0 = s_state.yaw_deg + s_yaw0; /* 简化：后端应使用原始 yaw */
    s_state.yaw_deg = 0.f;
}

#else /* !IMU_ENABLED */

void Imu_Init(void) {}
void Imu_Calibrate(void) {}
void Imu_Update(uint32_t dt_ms) { (void)dt_ms; }
bool Imu_DataReady(void) { return false; }

void Imu_Get(imu_state_t *out)
{
    if (out) {
        out->yaw_deg      = 0.f;
        out->yaw_rate_dps = 0.f;
        out->pitch_deg    = 0.f;
        out->roll_deg     = 0.f;
    }
}

float Imu_GetYawDeg(void) { return 0.f; }
void  Imu_ResetYaw(void) {}

#endif /* IMU_ENABLED */
