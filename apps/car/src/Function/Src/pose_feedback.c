#include "pose_feedback.h"
#include "chassis.h"
#include "robot_config.h"

/* 侧向符号：若推车前进时 s 为负，改为 -1 */
#define POSE_LEFT_SIGN   (1.0f)
#define POSE_RIGHT_SIGN  (1.0f)

static Pose_State_t s_st;
static int32_t s_base_left;
static int32_t s_base_right;

static float side_pulses_to_m(int32_t side_sum_delta)
{
    /* 左右各两轮之和 → 平均单轮脉冲再换算米 */
    return (0.5f * (float)side_sum_delta) * ROBOT_M_PER_PULSE;
}

void Pose_Init(void)
{
    s_st.s_m = 0.0f;
    s_st.theta_rad = 0.0f;
    s_st.dL_m = 0.0f;
    s_st.dR_m = 0.0f;
    s_st.heading_src = HEADING_SRC_ENCODER;
    s_base_left = 0;
    s_base_right = 0;
}

void Pose_Reset(void)
{
    s_base_left = Chassis_GetLeftCount();
    s_base_right = Chassis_GetRightCount();
    s_st.s_m = 0.0f;
    s_st.theta_rad = 0.0f;
    s_st.dL_m = 0.0f;
    s_st.dR_m = 0.0f;
}

void Pose_SetHeadingSource(HeadingSource_t src)
{
    if (src == HEADING_SRC_IMU) {
        /* 未接 IMU：保持编码器，避免误用 */
        s_st.heading_src = HEADING_SRC_ENCODER;
        return;
    }
    s_st.heading_src = src;
}

void Pose_Update(float dt_s)
{
    int32_t d_left_cnt;
    int32_t d_right_cnt;
    float dL;
    float dR;

    (void)dt_s;

    d_left_cnt = Chassis_GetLeftCount() - s_base_left;
    d_right_cnt = Chassis_GetRightCount() - s_base_right;

    dL = POSE_LEFT_SIGN * side_pulses_to_m(d_left_cnt);
    dR = POSE_RIGHT_SIGN * side_pulses_to_m(d_right_cnt);

    s_st.dL_m = dL;
    s_st.dR_m = dR;
    s_st.s_m = 0.5f * (dL + dR);

    /* 编码器航向；IMU 分支预留 */
    if (s_st.heading_src == HEADING_SRC_ENCODER) {
        s_st.theta_rad = (dR - dL) / ROBOT_TRACK_M;
    }
}

float Pose_GetDistance_m(void)
{
    return s_st.s_m;
}

float Pose_GetTheta_rad(void)
{
    return s_st.theta_rad;
}

float Pose_GetTheta_deg(void)
{
    return s_st.theta_rad * (180.0f / ROBOT_PI);
}

const Pose_State_t *Pose_GetState(void)
{
    return &s_st;
}
