# 运动控制工作框架（apps/car）

> 目标原语：**走指定距离** · **转指定角度** · **走指定半径圆/弧**  
> 航向策略：**编码器推算优先，API 预留 IMU 切换**  
> 本文档为设计与任务拆分，**不含业务代码实现**。

---

## 1. 背景与现状

### 1.1 工程位置

| 项 | 值 |
|----|-----|
| 目标工程 | `apps/car` |
| MCU | TI MSPM0G3507 |
| 底盘 | 四轮差速（MG310 + TB6612） |
| 几何配置 | `src/Function/Inc/robot_config.h` |

### 1.2 已有能力

| 模块 | 路径 | 能力 |
|------|------|------|
| `Chassis_*` | `Function/chassis.*` | 开环 L/R PWM、原地转、左右编码器聚合 |
| `GoStraight_*` | `Function/go_straight.*` | 编码器 L−R 保直 PID（**无距离目标**） |
| `Kine_*` | `Function/kinematics.*` | 车体 `(vx, ω)` → 四轮 PWM（**main 未接入**） |
| `PID_*` | `Hardware/pid.*` | 线性 / 航向 wrap 两种计算 |
| `Encoder_*` | `Hardware/encoder.*` | 四路正交计数 + 速度估计 |
| `LineTrack_*` | `Function/line_track.*` | 灰度巡线（与运动原语互斥） |

### 1.3 缺失能力

| 需求 | 现状 |
|------|------|
| 走指定距离 | 仅定时开环 / 保直不停车 |
| 转指定角度 | 仅 `Chassis_Spin*` 定时转 |
| 指定半径圆/弧 | 无 |
| car 内位姿反馈 | 无（`apps/odometry` 有独立实现，几何常数不一致） |
| 速度闭环 | 编码器速度未用于控制 |

### 1.4 硬件约束（与接线表一致）

- **car 工程当前无 IMU**（IMU601 在 `diansai` / `odometry`）。
- 轮位映射：左前 C、右前 B、左后 D、右后 A。
- 侧聚合：左 = EncC+EncD，右 = EncB+EncA（`Chassis_GetLeftCount/RightCount`）。

---

## 2. 分层架构

```text
┌─────────────────────────────────────────┐
│  main / 任务序列（demo、赛题动作表）       │
└───────────────────┬─────────────────────┘
                    │ 非阻塞 Start + 周期 Step
┌───────────────────▼─────────────────────┐
│  Motion 原语层                            │
│  GoDistance / TurnAngle / DriveCircle     │
│  状态机：IDLE → RUN → DONE / ABORT        │
└───────────────────┬─────────────────────┘
                    │ 读 s、θ；写 vx/ω 或 L/R
┌───────────────────▼─────────────────────┐
│  PoseFeedback 反馈层                      │
│  脉冲 → 米；Δθ 编码器 / 预留 IMU          │
└───────────────────┬─────────────────────┘
                    │
        ┌───────────┼───────────┐
        ▼           ▼           ▼
   GoStraight    Kine_*      PID_*
   （保直）     （曲率）    （可选航向）
        │           │
        └─────┬─────┘
              ▼
        Chassis_Drive / Spin*
              │
        Motor_* + Encoder_*
```

### 2.1 建议新增模块（实现阶段）

| 模块 | 建议路径 | 职责 |
|------|----------|------|
| `pose_feedback` | `Function/Inc/pose_feedback.h` · `Function/Src/pose_feedback.c` | 统一距离/航向反馈；航向源可切换 |
| `motion` | `Function/Inc/motion.h` · `Function/Src/motion.c` | 三大原语状态机 + `Step` |

**不新建**：电机/编码器驱动；几何常数一律读 `robot_config.h`。

### 2.2 与现有模块关系

| 依赖 | 用法 |
|------|------|
| `Chassis_*` | 唯一执行出口：`Drive` / `Spin*` / `Stop` |
| `GoStraight_*` | GO_DIST 可复用其 L−R 平衡思路，或内聚到 motion 内同类 PID |
| `Kine_*` | CIRCLE：`ω = vx/R` → `Kine_GetPWM` → 映射到 L/R |
| `PID_*` | 保直、可选航向、可选接近减速 |
| `robot_config.h` | **唯一**几何与脉冲标定源 |

---

## 3. 几何与换算

### 3.1 权威常数（`robot_config.h`）

```c
ROBOT_WHEEL_DIAMETER_M  0.048f   /* 轮径 48 mm */
ROBOT_TRACK_M           0.160f   /* 左右轮距 16 cm */
ROBOT_WHEELBASE_M       0.200f   /* 前后轴距 20 cm（差速圆公式主用 track） */
ROBOT_PULSES_PER_REV    1040     /* 13 × 4 × 20 */
ROBOT_MOTOR_MAX_RPM     400
ROBOT_LEFT_TRIM / RIGHT_TRIM     /* 开环微调，影响定距/定角标定 */
```

**禁止**混用 `apps/odometry` / `diansai` 中的 `ODOM_WHEEL_RADIUS_M`、`ODOM_WHEEL_BASE_M`（数值不同）。

### 3.2 派生量（实现时建议写成宏或 inline）

| 符号 | 公式 | 说明 |
|------|------|------|
| 轮周长 | `C = π · D` | `D = ROBOT_WHEEL_DIAMETER_M` |
| 米/脉冲 | `m_pp = C / PPR` | `PPR = ROBOT_PULSES_PER_REV` |
| 侧位移 | `dL = Δcount_L · m_pp`，`dR` 同理 | 相对某次动作起点 |
| 中线位移 | `s = (dL + dR) / 2` | 前进为正（符号与编码器极性一致后标定） |
| 编码器航向 | `θ = (dR − dL) / L_track` | 单位 rad，CCW+（与 kine 一致） |
| 原地转轮程 | `s_wheel = \|θ\| · (L_track / 2)` | 用于估算或交叉校验 |
| 圆弧角速度 | `ω = v / R` | `v` 中线线速度，`R` 圆心到车体中心 |
| 差速 | `v_L = v − ω·(L/2)`，`v_R = v + ω·(L/2)` | 与 `kinematics.c` 一致 |
| 弧长 | `s_arc = R · \|φ\|` | `φ` 为转过圆心角（rad） |

### 3.3 数量级参考（便于 OLED 调试）

| 量 | 约值 |
|----|------|
| 周长 | ≈ 0.1508 m |
| 米/脉冲 | ≈ 0.145 mm/pulse |
| 1 m 直线 | ≈ 6900 脉冲（中线平均） |
| 原地 90° | 轮程 ≈ 0.1257 m ≈ 866 脉冲/侧（反向） |

*实际以标定为准；打滑会使编码器 θ 偏大/偏小。*

### 3.4 符号约定

| 量 | 正方向 |
|----|--------|
| 线速度 `vx` / 距离 `s` | 车体前进 |
| 角速度 `ω` / 航向 `θ` | 逆时针（CCW） |
| `Motion_TurnAngle(+deg)` | 左转（CCW） |
| `Motion_DriveCircle(+R, …)` | 绕车体左侧圆心（CCW 弧） |
| PWM | 经 `Chassis_*` 后正=车体前进 |

---

## 4. 航向源抽象

### 4.1 接口意图

```c
typedef enum {
    HEADING_SRC_ENCODER = 0,  /* 默认：θ = (dR - dL) / track */
    HEADING_SRC_IMU     = 1,  /* 预留：移植 IMU601 后启用 */
} HeadingSource_t;

void Pose_Init(void);
void Pose_Reset(void);                 /* 清零本动作/全局相对原点 */
void Pose_SetHeadingSource(HeadingSource_t src);
void Pose_Update(float dt_s);          /* 周期调用，与 Motion_Step 同频或更高 */

float Pose_GetDistance_m(void);        /* 相对 Reset 的中线 s */
float Pose_GetTheta_rad(void);         /* 相对 Reset 的 θ */
float Pose_GetTheta_deg(void);
```

### 4.2 编码器路径（P0 必做）

1. 动作开始：`Pose_Reset()`，记录左右 count 基线（或用相对累加）。
2. 每周期：读 `Chassis_GetLeftCount/RightCount`，算 `dL/dR` → `s`、`θ`。
3. TURN / CIRCLE 完成判据优先用 `θ`；GO_DIST 用 `s`。

### 4.3 IMU 预留（文档级，不实现）

| 项 | 说明 |
|----|------|
| 参考实现 | `apps/odometry`：`Odom_Update` + IMU601 航向 |
| 接入点 | `Pose_Update` 内 `if (src == HEADING_SRC_IMU)` 读 yaw |
| 几何 | 仍用 `robot_config.h`；**不要**照搬 odometry 的 wheel/track 宏 |
| 接线 | 以 car 最新接线表为准；当前 car 未接 IMU 时禁止默认选 IMU |
| 融合 | Phase 后期可做「编码器距离 + IMU 航向」；本框架不强制 |

---

## 5. Motion API 草案

### 5.1 生命周期

```c
void Motion_Init(void);
void Motion_Step(void);              /* 建议 10~20 ms 调用一次 */
uint8_t Motion_IsDone(void);         /* 1=完成或空闲可接下一条 */
void Motion_Abort(void);             /* 立即 Chassis_Stop，回 IDLE */
const Motion_Status_t *Motion_GetStatus(void);
```

### 5.2 原语

```c
/* 走指定距离：meters>0 前进，<0 后退；base_pwm 为 0~100 量级基准 */
void Motion_GoDistance(float meters, int16_t base_pwm);

/* 转指定角度：deg>0 左转 CCW；spin_pwm 为原地转 PWM 幅值 */
void Motion_TurnAngle(float deg, int16_t spin_pwm);

/*
 * 指定半径圆弧：
 *   radius_m > 0：圆心在车体左侧（CCW）
 *   radius_m < 0：圆心在车体右侧（CW），|R| 为半径
 *   arc_deg：沿圆弧转过的圆心角（度），符号可与半径配合；实现时建议 |arc_deg| 为幅值，方向由 R 符号决定
 *   base_pwm：中线等效开环速度档位
 */
void Motion_DriveCircle(float radius_m, float arc_deg, int16_t base_pwm);
```

### 5.3 状态与查询

```c
typedef enum {
    MOTION_IDLE = 0,
    MOTION_GO_DIST,
    MOTION_TURN,
    MOTION_CIRCLE,
    MOTION_DONE,
    MOTION_ABORT,
} Motion_State_t;

typedef struct {
    Motion_State_t state;
    float target;          /* m 或 deg，视 state */
    float feedback;        /* 当前 s 或 θ(deg) */
    float radius_m;        /* CIRCLE 用 */
    int16_t left_cmd;
    int16_t right_cmd;
    uint8_t done;
} Motion_Status_t;
```

### 5.4 调用约定

1. **非阻塞**：`Start` 类函数只置状态与目标，不长时间 `delay`。
2. **单飞行**：运行中再次 `Go/Turn/Circle` 的行为：文档约定为 **Abort 当前再启动新目标**（实现时二选一并写死）。
3. **完成**：`IsDone()==1` 且已 `Chassis_Stop`；下一动作前可再 `Pose_Reset` 或由原语内部 Reset。
4. **与巡线互斥**：`LineTrack` 与 `Motion` 不可同时 `Step`。

### 5.5 main 侧伪代码

```c
Motion_Init();
Pose_Init();
/* 可选: Kine_Init(ROBOT_WHEEL_DIAMETER_M, ROBOT_WHEELBASE_M,
                   ROBOT_TRACK_M, ROBOT_MOTOR_MAX_RPM); */

Motion_GoDistance(1.0f, 18);
while (!Motion_IsDone()) {
    Pose_Update(0.01f);
    Motion_Step();
    delay_ms(10);
}

Motion_TurnAngle(90.0f, 22);
while (!Motion_IsDone()) {
    Pose_Update(0.01f);
    Motion_Step();
    delay_ms(10);
}

Motion_DriveCircle(0.30f, 360.0f, 18);
while (!Motion_IsDone()) {
    Pose_Update(0.01f);
    Motion_Step();
    delay_ms(10);
}
```

---

## 6. 状态机

### 6.1 总图

```text
                    Start_*
         ┌──────────────────────────┐
         │                          │
         ▼                          │
      ┌──────┐   Start_*          ┌──────┐
      │ IDLE │ ─────────────────► │ RUN* │
      └──┬───┘                    └──┬───┘
         ▲                           │
         │         完成条件          │
         │         Chassis_Stop      ▼
         │                      ┌────────┐
         └──────────────────────│  DONE  │
                                └────────┘
         Abort 任意 RUN/DONE
              │
              ▼
         ┌────────┐
         │ ABORT  │ ──Stop──► IDLE
         └────────┘

* RUN ∈ { GO_DIST, TURN, CIRCLE }
```

### 6.2 各态控制律

#### GO_DIST — 走指定距离

| 项 | 内容 |
|----|------|
| 进入 | `Pose_Reset`；保存 `target_s`、`base_pwm`；方向 = sign(meters) |
| 控制 | 仿 `GoStraight`：目标 `left_cnt≈right_cnt`，输出 `base ± corr`；后退时 base 取负或左右同负 |
| 反馈 | `s = Pose_GetDistance_m()` |
| 完成 | `\|s\| ≥ \|target_s\|` → Stop → DONE |
| 可选 Phase2 | 剩余距离 < 阈值时降低 base（梯形速度） |

#### TURN — 转指定角度

| 项 | 内容 |
|----|------|
| 进入 | `Pose_Reset`；`target_θ`（rad）；`spin_pwm` |
| 控制 | `Chassis_SpinLeft/Right` 或 `Drive(-u, +u)`，方向由 `sign(deg)` 决定 |
| 反馈 | `θ = Pose_GetTheta_rad()`（编码器；IMU 就绪后可切换） |
| 完成 | `\|θ\| ≥ \|target_θ\|` → Stop → DONE |
| 注意 | 纯编码器对打滑敏感；低速 + 标定 track 可改善 |

#### CIRCLE — 指定半径弧

| 项 | 内容 |
|----|------|
| 进入 | `Pose_Reset`；`R`、`φ`（弧度）、`base_pwm` |
| 控制 | 取中线开环速度档 `v_cmd`（可由 base 映射或先固定 PWM 差速比）；`ω = v/R`；`Kine_GetPWM(vx, 0, ω)` → 映射 FL/FR/RL/RR → C/B/D/A → 或简化为 L/R 两路 `Chassis_Drive` |
| 简化实现 | 不强制真 m/s：用 `v_L:v_R = (R−L/2):(R+L/2)` 比例分配 `base_pwm` |
| 反馈完成 | **优先** `\|θ\| ≥ \|φ\|`；备选 `\|s\| ≥ \|R·φ\|` |
| 奇异 | `\|R\| < L_track/2` 时内侧轮反向，需允许负 PWM；`\|R\|→0` 退化为原地转，可内部转调 TURN |
| 整圆 | `arc_deg = ±360`；闭环建议用 θ 累计，避免弧长积分漂移叠加 |

### 6.3 Kine 与 Chassis 映射

`kinematics` 轮序：FL/FR/RL/RR  
本车：`FL=C, FR=B, RL=D, RR=A`

差速车同侧前后应同速，可：

```text
left_cmd  = clamp( (pwm_FL + pwm_RL) / 2 )
right_cmd = clamp( (pwm_FR + pwm_RR) / 2 )
Chassis_Drive(left_cmd, right_cmd);
```

或扩展 `Chassis_SetBodyVel`（实现阶段可选），内部完成 Kine + 映射。

---

## 7. 控制周期与调度

| 阶段 | 方式 | 周期 |
|------|------|------|
| 近期 | `main` 循环 `Motion_Step` + `delay_ms(10~20)` | 10–20 ms |
| 中期 | 定时器置标志，主循环消费（参考 `odometry` 100 Hz） | 10 ms |
| 同周期建议 | `Encoder_UpdateSpeed`（若速度环）→ `Pose_Update` → `Motion_Step` → OLED | |

**禁止**在 ISR 内直接调电机复杂逻辑过久；ISR 只置标志或更新计数（计数已在编码器边沿中断）。

---

## 8. 实现任务拆分

### 8.1 阶段表

| 阶段 | 任务 | 产出 | 依赖 | 验收建议 |
|------|------|------|------|----------|
| **P0** | `pose_feedback` | 脉冲→m、编码器 θ | chassis 计数 + robot_config | 手推车 OLED 显示 s、θ 合理 |
| **P1** | `Motion_GoDistance` | 定距 + 保直 | P0 + GoStraight 思路 | 0.5 m / 1.0 m，误差 &lt; 5 cm（硬地） |
| **P2** | `Motion_TurnAngle` | 定角原地转 | P0 | ±45°/±90°/180°，误差 &lt; 5° |
| **P3** | `Motion_DriveCircle` | 定 R 弧/圆 | P0 + Kine 或比例差速 | R=0.3 m，半圆/整圆轨迹可辨 |
| **P4** | 航向源切换骨架 | `HEADING_SRC_*` | P0 | 编码器默认；IMU 分支编译期或空实现 |
| **P5** | Demo 序列 | main 或独立 demo | P1–P3 | 1 m → 90° → R=0.3 整圆 可重复 |

### 8.2 建议文件改动清单（实现时）

```text
apps/car/src/Function/Inc/pose_feedback.h   [新]
apps/car/src/Function/Src/pose_feedback.c   [新]
apps/car/src/Function/Inc/motion.h          [新]
apps/car/src/Function/Src/motion.c          [新]
apps/car/src/main.c                         [demo 切换]
apps/car/src/Function/Inc/robot_config.h    [可选：增加 m_per_pulse 等派生宏]
```

Makefile 已递归编译 `src/**/*.c`，一般**无需**改构建脚本。

### 8.3 不在首期范围

- 速度 PID / 轨迹跟踪 MPC
- 完整 4 轮滑移检测
- 自动标定流程
- 与 `line_track` 的融合导航
- IMU 硬件移植与调参（仅预留接口）

---

## 9. 标定与风险

### 9.1 建议标定顺序

1. **轮径**：直线推 1 m，用中线脉冲反算 `m_pp` 或微调 `ROBOT_WHEEL_DIAMETER_M`。
2. **轮距**：原地转 360°，用 `θ_enc` 与真值比，微调 `ROBOT_TRACK_M`。
3. **TRIM**：先开环左右直线，再开保直 PID。
4. **最小 PWM**：过低一侧不转；GO_DIST/CIRCLE 保持 `\|cmd\| ≥ PWM_MIN_MOVE`。

### 9.2 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| 打滑 | 定距/定角/圆误差 | 降速；后续 IMU 航向 |
| TRIM 不对称 | 圆变椭圆 | 标定 + 保直/比例差速 |
| R 过小 | 内侧轮近 0 或反向 | 限制 `\|R\|_min`；允许负 PWM |
| 与 LineTrack 同时跑 | 抢占 Chassis | 互斥状态 |
| 几何常数多套 | 难排查 | **仅** robot_config |
| 阻塞 delay 过长 | 控制发散 | 统一 10–20 ms Step |

### 9.3 安全

- 任意异常路径调用 `Motion_Abort` → `Chassis_Stop`。
- 上电默认 IDLE，电机 Stop。
- 调试首跑使用低 `base_pwm`（如 15–20）。

---

## 10. 验收清单

### 功能

- [ ] `Motion_GoDistance(0.5f, …)` / `(1.0f, …)` 停车距离达标
- [ ] `Motion_TurnAngle(±45/±90/180, …)` 转角达标
- [ ] `Motion_DriveCircle(R, 90/180/360, …)` 轨迹曲率可辨、能停
- [ ] `Motion_Abort` 立即停车
- [ ] 连续多条指令（DONE 后再 Start）无卡死
- [ ] `Pose` 默认编码器；IMU 源可编译/切换且未接 IMU 时不误选

### 工程

- [ ] 几何仅来自 `robot_config.h`
- [ ] 控制周期稳定 10–20 ms
- [ ] 与 `LineTrack` 互斥有说明或保护
- [ ] OLED 或串口可观察 `state / target / feedback / L/R cmd`

### Demo 序列（P5）

- [ ] 前进 1.0 m → 左转 90° → R=0.30 m 整圆 → 停车

---

## 11. 参考索引

| 资源 | 路径 |
|------|------|
| 几何与脉冲 | `apps/car/src/Function/Inc/robot_config.h` |
| 底盘 API | `apps/car/src/Function/Inc/chassis.h` |
| 运动学 | `apps/car/src/Function/Inc/kinematics.h` |
| 保直 | `apps/car/src/Function/Inc/go_straight.h` |
| 当前开环 main | `apps/car/src/main.c` |
| 里程计参考（勿直接抄几何） | `apps/odometry/src/Odometry/` |
| 接线 | `apps/car/硬件接线表.html` |

---

## 12. 修订记录

| 日期 | 说明 |
|------|------|
| 2026-07-27 | 初版：三大原语工作框架，仅文档；航向编码器优先 + IMU 预留 |
