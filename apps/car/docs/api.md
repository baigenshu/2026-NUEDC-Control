# car API（重写）

> 更新：2026-07-30  
> 符号：线速度 +前 -后；转向 +左 -右

## Chassis

| API | 说明 |
|-----|------|
| `Chassis_Init / Enable / Update` | 生命周期；`Update(dt_ms)` 每拍必调 |
| `Stop / SetLR / Arcade` | 停车 / 左右差速 / 油门+转向 |
| `ResetOdom / GetOdom / GetDistCm` | 里程 |

## LineTrack

| API | 说明 |
|-----|------|
| `Init / SetEnable / Reset` | 开关与状态清零 |
| `SetBaseSpeed / Update` | 基速；使能后每拍 Update |
| `GetError / GetMask` | 调试 / 显示 |

## LapTask

| API | 说明 |
|-----|------|
| `Init / Start / Abort / Update` | 一圈任务 |
| `GetState / IsActive` | WAIT / RUN / DONE / TIMEOUT / ABORTED |
| `GetElapsedMs` | 运行中实时；完成时冻结 |
| `GetMask / GetError` | OLED 用 |

## 关键参数（`chassis_cfg.h`）

| 参数 | 作用 |
|------|------|
| `POL_A..D` | 电机极性（实车 A/D=-1，B/C=+1） |
| `LT_KP / LT_KD` | 巡线 PD |
| `LT_TURN_SIGN` | 误差→转向符号 |
| `LAP_TRACK_SPEED` | 巡航基速 |
| `LAP_MIN_DISTANCE_CM` | 允许认停车线前的最小里程 |
| `LAP_MARKER_MIN_ACTIVE` | 停车线：同时为黑的通道数 |
| `GRAY_WEIGHT_0..7` | G1..G8 权重 |

## 主循环

1. `Chassis_Update(dt)`
2. B21 → `LapTask_Start` / `Abort`
3. `LapTask_Update(now)`（内部调 `LineTrack_Update`）
4. ~100 ms 刷 OLED
