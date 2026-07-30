# 球位闭环停球（balance）

## 架构

```text
MaixCAM  pos_mm
    │
    ▼
    位置 PID  ──►  rod（丝杆倾角代理）
    │
stepper 开环跟 rod
```

- 反馈：球相对 O 的 `pos_mm`（整 mm）和滤波后球速
- 算法：**单环位置式 PID**；D 项对测量球速，不对目标做微分
- 积分分离 / 限幅抗饱和；到位关 EN

## 改定点

| 方式 | 示例 |
|------|------|
| 固件编译期 | `BallCtrl_SetTargetMm_x100(5000);` → +50.00 mm |
| 运行时 API | 同上，单位 **0.01 mm** |
| 串口 type=`0x12` | `target_mm` 单位 **1 mm**（见 vision_proto.md） |

```c
BallCtrl_SetTargetMm_x100(0);      /* O */
BallCtrl_SetTargetMm_x100(-2500);  /* -25.00 mm */
BallCtrl_Enable(true);
```

## 调参（单环 PID）

| 宏 | 环 | 默认 | 调法 |
|----|----|------|------|
| `KP_POS` | 位置比例 | 0.045 rod mm / ball mm | 远处反应弱 → 升；摆动 → 降 |
| `KI_POS` | 位置积分 | 0.004 | 稳态偏一侧 → 略升；慢摆 → 降 |
| `KD_POS` | 位置微分/速度阻尼 | 0.045 | 接近时冲过目标 → 升；响应迟钝 → 降 |
| `ROD_MAX` | 倾角权威 | ±7 mm | |
| `ROD_SLEW` | 丝杆目标变化 | 0.07 mm/15 ms | 响应太慢 → 略升；摆动 → 降 |
| `STEPPER_SPS` | 步进巡航 | 10000 step/s | 丢步/发热 → 降 |
| `STEPPER_ACCEL` | 步进加速度 | 30000 step/s² | 换向冲击 → 降 |
| `SIGN` | 方向 | +1 | 球越控越飞 → **-1** |

42 电机：IRUN≈0.8 A；到位关 EN。

## 接线

视觉 TX → **PA31**，GND 共地；可选 PA28 TX。

## 安全

- 丢球或 >100 ms 无帧 → 倾角回 0（`HOLD_LEVEL_ON_LOSS`）
- 目标与倾角均有软限位
