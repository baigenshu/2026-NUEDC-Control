#ifndef __ODOM_H
#define __ODOM_H

#include <stdint.h>

/*
 * 最小差速底盘里程计
 *   - 由双编码器计算线速度
 *   - 由 IMU601 航向角（deg）计算朝向
 *   - 位姿积分：x、y 单位为米，theta 单位为弧度
 */

typedef struct {
    float x;       /* 米 */
    float y;       /* 米 */
    float theta;   /* 弧度，逆时针为正 */
    float v;       /* 车体线速度，米/秒 */
    float omega;   /* 车体角速度，弧度/秒（由航向角差计算） */
    uint32_t status; /* bit0：打滑告警（保留） */
} OdomState_t;

/* 机器人几何参数 / 编码器比例（实车可再标定） */
#ifndef ODOM_WHEEL_RADIUS_M
#define ODOM_WHEEL_RADIUS_M     (0.0230f)   /* 车轮半径（米），直径约 46mm */
#endif
#ifndef ODOM_WHEEL_BASE_M
#define ODOM_WHEEL_BASE_M       (0.135f)    /* 左右轮中心距（米） */
#endif
#ifndef ODOM_PULSES_PER_REV
#define ODOM_PULSES_PER_REV     (1040.0f)   /* 每圈脉冲数（4 倍频） */
#endif
#ifndef ODOM_ENC_A_SIGN
#define ODOM_ENC_A_SIGN         (1.0f)
#endif
#ifndef ODOM_ENC_B_SIGN
#define ODOM_ENC_B_SIGN         (1.0f)
#endif

void Odom_Init(void);
void Odom_Reset(void);

/* 以约 100 Hz 调用，dt 为实测时间间隔（秒）。 */
void Odom_Update(float dt);

const OdomState_t *Odom_GetState(void);

#endif
