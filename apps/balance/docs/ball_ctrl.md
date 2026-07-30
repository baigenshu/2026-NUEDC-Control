# 球位闭环停球（balance）

## 架构

```text
MaixCAM  pos_mm
    │
    ▼
外环 位置 PI  ──►  v_des（期望球速 mm/s）
    │
内环 速度 PD+FF ──►  rod（丝杆倾角代理）
    │
stepper 开环跟 rod
```

- 反馈：球相对 O 的 `pos_mm`（整 mm）+ 差分速度
- 算法：**级联**（位置 PI → 速度 PD + 速度前馈）
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

## 调参（级联）

| 宏 | 环 | 默认 | 调法 |
|----|----|------|------|
| `KP_POS` | 外：位置→速度 | 1.0 | 慢/不到 → 升；抖 → 降 |
| `KI_POS` | 外：消静差 | 0.10 | 稳态偏一侧 → 略升；超调 → 降 |
| `V_DES_MAX` | 外：限速 | 35 mm/s | 大摆过猛 → 降 |
| `KP_VEL` | 内：速度→倾角 | 0.035 | 跟速软 → 升 |
| `KD_VEL` | 内：阻尼 | 0.025 | 振荡 → 升 |
| `KFF_VEL` | 速度前馈 | 0.008 | 匀速段无力 → 升 |
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
