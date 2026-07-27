#ifndef __POSE_FEEDBACK_H
#define __POSE_FEEDBACK_H

#include <stdint.h>

/*
 * 相对位姿反馈（编码器优先，预留 IMU 航向源）
 * 依赖: chassis 左右聚合计数 + robot_config
 *
 * 约定:
 *   s > 0 前进，θ > 0 逆时针 (CCW)
 *   左右 count 为两侧双轮之和，距离用平均脉冲
 */

typedef enum {
    HEADING_SRC_ENCODER = 0,
    HEADING_SRC_IMU     = 1  /* 预留，未接 IMU 时勿选 */
} HeadingSource_t;

typedef struct {
    float s_m;       /* 中线位移 m */
    float theta_rad; /* 相对航向 rad */
    float dL_m;
    float dR_m;
    HeadingSource_t heading_src;
} Pose_State_t;

void Pose_Init(void);
void Pose_Reset(void);
void Pose_SetHeadingSource(HeadingSource_t src);

/* 用当前编码器相对 Reset 刷新 s、θ（无 IMU 时 dt 可忽略） */
void Pose_Update(float dt_s);

float Pose_GetDistance_m(void);
float Pose_GetTheta_rad(void);
float Pose_GetTheta_deg(void);
const Pose_State_t *Pose_GetState(void);

#endif
