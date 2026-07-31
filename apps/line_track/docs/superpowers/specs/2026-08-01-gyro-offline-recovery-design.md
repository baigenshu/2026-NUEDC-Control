# 陀螺仪辅助丢线恢复（方案 A）设计

## Context
红外循迹直线段稳定，但高速过弯时 IR 短暂丢线（m=00）后，当前"抱住最后 IR 误差"的策略估值不准，车回不来、跑飞。已接入 IMU601 陀螺仪（UART2/PA23 TX/PA24 RX，DMA 接收，输出航向 yaw 角）。本设计用 yaw 在丢线期间维持转弯，穿过盲区找回线。

**目标**：仅干预丢线段，在线循迹完全不变。

## 数据流（每 10ms 控制周期，`LineFollow_Update`）
1. 读 `IMU601_Attitude.yaw`（°），算 `yaw_rate`（带 0↔360 跨越处理；符号用 `LF_YAW_SIGN` 校准成"右转为正"，与现有 `pid>0=右转` 一致）。
2. **在线**（`s_active>0`）：IR 循迹完全不变；同时 `rate_ref` 低通跟踪 `yaw_rate`（`rate_ref += ALPHA·(yaw_rate−rate_ref)`），记录当前弯的转弯速率。
3. **丢线**（`s_active==0`）：绕过 IR 误差/PID，改用 `pid = LF_K_YAW·(rate_ref − yaw_rate)`；左右轮 `left=base+pid, right=base−pid`（同现有差速公式与限幅）。车按丢线前速率继续转。
4. 丢线持续 >`LF_LOSS_TIMEOUT_MS`（默认 500ms）未找回 → 停车（`stop_all`，防跑飞）。
5. 重新找到线 → 立即切回 IR 循迹，`rate_ref` 继续更新。
6. IMU 未就绪（`IMU601_FrameCount==0`）时丢线 → 直接停车（不瞎转）。

## 控制律与参数（`line_follow_cfg.h` 新增）
- `LF_K_YAW`：角速度环增益（(脉冲/10ms)/(°/s)），初值小（如 0.3），上机调。
- `LF_RATE_ALPHA`：`rate_ref` 低通系数（如 0.2）。
- `LF_LOSS_TIMEOUT_MS`：丢线超时（默认 500）。
- `LF_YAW_SIGN`：±1，校准 IMU 安装方向（右转 yaw 增加为 +1，否则 −1），上机定。

## 边界
- yaw 跨越：`delta = yaw_now − yaw_prev`；`delta > 180` 减 360，`delta < −180` 加 360。
- `yaw_rate` 用控制周期 dt=10ms 归一化（°/s 或 °/10ms，与 `LF_K_YAW` 单位匹配，统一即可）。
- IMU 异步更新（DMA）：yaw 在控制周期内可能未刷新；rate 用最新 yaw 与上次控制周期 yaw 比较，未刷新时 rate 趋 0（可接受，`rate_ref` 已平滑）。
- 在线更新 `rate_ref`，丢线冻结。

## 代码位置
- `src/Function/Src/line_follow.c`：`#include "imu601.h"`；新增 `s_yaw_prev`、`s_rate_ref`、`s_loss_ms` 静态状态；`LineFollow_Update` 中加 yaw_rate 计算 + 丢线分支（角速度环）；`LineFollow_Reset` 清这些状态。
- `src/Function/Inc/line_follow_cfg.h`：新增上面 4 个参数。
- 不动 `speed_ctrl` / `imu601` / `main.c`（main.c 的 100ms yaw 透传保留）。

## 验证
1. `make clean && make` 编译通过。
2. 烧录，直线段循迹不变（m=06 居中为主）。
3. 故意让车在弧上短暂丢线（或跑急弯）：丢线时车应按原转弯速率继续转、找回线，不再跑飞；串口 `m=00` 持续时间短（<500ms）。
4. 长时间丢线（>500ms）→ 停车。
5. 调 `LF_YAW_SIGN`：若丢线时车转向反了，翻转符号。
6. 调 `LF_K_YAW`：丢线时转弯不够→加大；振荡→减小。
