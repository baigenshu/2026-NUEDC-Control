#include "odom.h"
#include "encoder.h"
#include "imu601.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static OdomState_t g_odom;
static float s_yaw0_deg;
static float s_last_yaw_rad;
static uint8_t s_yaw_inited;
static int32_t s_prevA, s_prevB;
static uint8_t s_count_inited;

static float deg2rad(float d) { return d * (M_PI / 180.0f); }

static float wrap_pi(float a)
{
    while (a >  M_PI) a -= 2.0f * M_PI;
    while (a < -M_PI) a += 2.0f * M_PI;
    return a;
}

static float pulses_to_m(float pulses)
{
    return pulses * (2.0f * M_PI * ODOM_WHEEL_RADIUS_M) / ODOM_PULSES_PER_REV;
}

void Odom_Init(void)
{
    Odom_Reset();
}

void Odom_Reset(void)
{
    g_odom.x = 0.0f;
    g_odom.y = 0.0f;
    g_odom.theta = 0.0f;
    g_odom.v = 0.0f;
    g_odom.omega = 0.0f;
    g_odom.status = 0;
    s_yaw0_deg = IMU601_Attitude.yaw;
    s_last_yaw_rad = 0.0f;
    s_yaw_inited = 0;
    s_prevA = 0;
    s_prevB = 0;
    s_count_inited = 0;
    EncoderA_Reset();
    EncoderB_Reset();
}

const OdomState_t *Odom_GetState(void)
{
    return &g_odom;
}

void Odom_Update(float dt)
{
    float dL, dR, ds, dth_enc;
    float yaw_deg, yaw_rad, dth_imu;
    int32_t cA, cB;

    if (dt <= 0.0f || dt > 0.5f)
        return;

    /* Counts updated by Encoder_Sample() from QEI before this call */
    cA = EncoderA_Count;
    cB = EncoderB_Count;

    if (!s_count_inited) {
        s_prevA = cA;
        s_prevB = cB;
        s_count_inited = 1;
        s_yaw0_deg = IMU601_Attitude.yaw;
        s_last_yaw_rad = 0.0f;
        s_yaw_inited = 1;
        return;
    }

    dL = pulses_to_m((float)(cA - s_prevA) * ODOM_ENC_A_SIGN);
    dR = pulses_to_m((float)(cB - s_prevB) * ODOM_ENC_B_SIGN);
    s_prevA = cA;
    s_prevB = cB;

    ds = 0.5f * (dL + dR);
    dth_enc = (dR - dL) / ODOM_WHEEL_BASE_M;

    yaw_deg = IMU601_Attitude.yaw - s_yaw0_deg;
    /* unwrap near 0/360 boundary of module yaw */
    if (yaw_deg > 180.0f)  yaw_deg -= 360.0f;
    if (yaw_deg < -180.0f) yaw_deg += 360.0f;
    yaw_rad = deg2rad(yaw_deg);

    if (!s_yaw_inited) {
        s_last_yaw_rad = yaw_rad;
        s_yaw_inited = 1;
    }
    dth_imu = wrap_pi(yaw_rad - s_last_yaw_rad);
    s_last_yaw_rad = yaw_rad;

    /* Heading: trust IMU; keep encoder dth for slip residual */
    g_odom.theta = yaw_rad;
    g_odom.v = ds / dt;
    g_odom.omega = dth_imu / dt;

    /* Dead reckoning with mid-point heading */
    {
        float th_mid = g_odom.theta - 0.5f * dth_imu;
        g_odom.x += ds * cosf(th_mid);
        g_odom.y += ds * sinf(th_mid);
    }

    /* Simple slip flag: large mismatch enc vs imu omega */
    {
        float w_enc = dth_enc / dt;
        float err = w_enc - g_odom.omega;
        if (err < 0.0f) err = -err;
        if (err > 2.0f && (g_odom.v > 0.05f || g_odom.v < -0.05f))
            g_odom.status |= 0x01u;
        else
            g_odom.status &= ~0x01u;
    }

    (void)dth_enc;
}
