#ifndef TRACK_CONTROL_H
#define TRACK_CONTROL_H

#include <stdint.h>

/*
 * 跟踪控制模块 — P 控制 + 死区 + 超时 + 轴映射
 *
 * 主循环高频调用 track_control_update()。
 * 内部读取 track_proto 的最新帧，计算速度并驱动步进电机。
 */

/* P 控制参数 — 优先响应，允许丢步 */
#define TRACK_DEADZONE_PX   3       /* 死区像素 */
#define TRACK_KP_X          0.55f   /* °/s per pixel，Yaw */
#define TRACK_KP_Y          0.45f   /* °/s per pixel，Pitch */
#define TRACK_VMAX_YAW      90.0f   /* Yaw 负载侧速度上限 °/s */
#define TRACK_VMAX_PITCH    70.0f   /* Pitch 负载侧速度上限 °/s */

/* 丢目标超时 ms */
#define TRACK_TIMEOUT_MS    80U

/* 轴方向：接线反了改符号（±1）。俯仰已在 .c 对 err_y 取反，一般保持 1 */
#define YAW_DIR_SIGN        (1)
#define PITCH_DIR_SIGN      (1)   /* 偏上却低头则改 ±1；.c 里已对 err_y 取反 */

/* 软限位（可选 Phase 5），0 = 不启用，设为 450 则启用 ±45.0° 软限位 */
#define PITCH_LIMIT_DEG_X10  0

void track_control_init(void);
void track_control_update(void);

#endif /* TRACK_CONTROL_H */
