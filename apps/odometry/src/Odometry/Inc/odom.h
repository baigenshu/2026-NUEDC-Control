#ifndef __ODOM_H
#define __ODOM_H

#include <stdint.h>

/*
 * Minimal differential-drive odometry
 *   - linear speed from dual encoders
 *   - heading from IMU601 yaw (deg)
 *   - pose integrate: x,y in meters, theta in rad
 */

typedef struct {
    float x;       /* m */
    float y;       /* m */
    float theta;   /* rad, CCW+ */
    float v;       /* m/s body linear */
    float omega;   /* rad/s body angular (from yaw delta) */
    uint32_t status; /* bit0: slip warn (reserved) */
} OdomState_t;

/* Robot geometry / encoder scale — calibrate on real vehicle */
#ifndef ODOM_WHEEL_RADIUS_M
#define ODOM_WHEEL_RADIUS_M     (0.0325f)   /* wheel radius (m) */
#endif
#ifndef ODOM_WHEEL_BASE_M
#define ODOM_WHEEL_BASE_M       (0.160f)    /* left-right distance (m) */
#endif
#ifndef ODOM_PULSES_PER_REV
#define ODOM_PULSES_PER_REV     (1040.0f)   /* encoder pulses per wheel rev (4x) */
#endif
/* Encoder sign: set -1 if wiring/mount is reversed */
#ifndef ODOM_ENC_A_SIGN
#define ODOM_ENC_A_SIGN         (1.0f)
#endif
#ifndef ODOM_ENC_B_SIGN
#define ODOM_ENC_B_SIGN         (1.0f)
#endif

void Odom_Init(void);
void Odom_Reset(void);

/* Call at ~100 Hz with measured dt (seconds). */
void Odom_Update(float dt);

const OdomState_t *Odom_GetState(void);

#endif
