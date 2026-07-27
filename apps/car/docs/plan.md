# car 底盘驱动 · 架构

> 完整底盘软件栈；主函数只调少量高层 API  
> 轮位：A 右后 · B 右前 · C 左前 · D 左后（左=C+D，右=B+A）  
> API：[api.md](api.md) · 引脚：[pins.md](pins.md)  
> 更新：2026-07-28

---

## 1. 原则

1. **分层**：Hardware → Chassis → LineTrack → `main`
2. **主函数只见车**，不见引脚宏
3. **少接口**
   - 直行 `Chassis_Go(speed, opt)`（`speed` 正负=前后）
   - 转向 `Chassis_Turn(speed, angle, opt)`（`angle` 正负=左右）
   - 停车 `Chassis_Stop(mode)`
   - 即时差速 `SetLR` / `Arcade`
4. **周期统一**：`Chassis_Update(dt)` = 测速 +（可选）速度环 + MOTION 推进 + odom
5. **符号**：线速度 `+`前 `-`后；转向 `+`左 `-`右
6. **配置** 唯一入口 `chassis_cfg.h`（宏/表）；**对外类型**在 `chassis.h`
7. **外设只经 Chassis 动电机**（`Motor_Set` 仅调试/自检可直调）
8. **库内轻量状态机** 仲裁 Hold / Motion，避免 main 与回调互相覆盖语义不清

---

## 2. 目录

```text
apps/car/src/
├── main.c
├── Hardware/
│   ├── Inc/   motor.h  encoder.h  gray.h  imu.h  /*预留*/
│   └── Src/   motor.c  encoder.c  gray.c  imu.c
└── Function/
    ├── Inc/   chassis_cfg.h  chassis.h  line_track.h
    └── Src/   chassis.c  line_track.c
               （可选 chassis_motion.c 仅拆实现，仍只导出 chassis.h）
```

---

## 3. 数据流与仲裁

```text
ISR / 串口回调
    └─ 只写命令邮箱（estop / remote / throttle,turn / flags）
              │
main 周期
    ├─ 取邮箱
    ├─ 急停 → Abort + Enable(false)
    ├─ 遥控 → LineTrack 关 + Arcade/SetLR
    ├─ else if Busy → 不发新运动（Update 推进 MOTION）
    ├─ else if 巡线使能 → LineTrack_Update → Arcade(owner=LINE)
    └─ else → Stop（仅当 mode==IDLE 且需要静默停）
              │
        Chassis_Update(dt)
          测速 · odom · 速度环(P4) · MOTION 推进
              │
         motor ×4  ◄── encoder（ENC_SIGN 与 POL 分离）
              ▲
             imu（P5 预留）
```

| 层 | 职责 |
|----|------|
| motor | 单轮占空、STBY、死区、COAST/BRAKE |
| encoder | 正交计数（ISR 短）；测速在 Update 差分 |
| imu（预留） | 航向 yaw；未接时空实现 |
| gray | mask / 加权位置（权重以 cfg 为准） |
| chassis | 状态机、差速、POL/TRIM、Go/Turn、odom |
| line_track | 巡线 PID → `Arcade`（owner=LINE） |
| main | 任务优先级 + 邮箱；**不**直写 PWM |

### 3.1 库内状态（必实现）

```text
typedef enum {
    CHASSIS_STATE_IDLE = 0,   /* 无输出保持；可被 Stop 置入 */
    CHASSIS_STATE_HOLD,       /* SetLR / Arcade / 持续 Go|Turn：Busy=false */
    CHASSIS_STATE_MOTION,     /* 定距 Go / 定角 Turn：Busy=true */
} chassis_state_t;
```

| 事件 | 下一状态 | Busy | 说明 |
|------|----------|------|------|
| `SetLR` / `Arcade` | HOLD | false | **中止**任何 MOTION；保持差速直到改指令 |
| `Go` 且 `distance_cm==0` 或 `opt==NULL` 等价持续 | HOLD | false | 持续直行，**非** Busy |
| `Go` 且 `distance_cm>0` | MOTION | true | 到位/超时 → 默认 BRAKE → IDLE |
| `Turn` 且 `angle_deg==0` | HOLD | false | 持续自旋 |
| `Turn` 且 `angle_deg!=0` | MOTION | true | 定角；到位/超时 → 默认 BRAKE → IDLE |
| `Stop(mode)` | IDLE | false | 立即停车 |
| `Abort()` | IDLE | false | 取消目标 + `Stop(DEFAULT)` |
| MOTION 到位/超时 | IDLE | false | 使用 `CHASSIS_MOTION_DONE_STOP_MODE`（默认 BRAKE） |

**`Chassis_Busy()`** ⇔ `state == MOTION`。持续运动**不是** Busy。

命令 owner（实现可用枚举，防巡线盖遥控）：

| owner | 来源 |
|-------|------|
| NONE | IDLE |
| MANUAL | `SetLR` / `Arcade` / 持续 Go·Turn（策略/遥控） |
| MOTION | 定距/定角 |
| LINE | `LineTrack_Update` 内部 `Arcade` |

规则：

1. `LineTrack_Update` **仅当** `!Busy()` 且（IDLE 或 owner==LINE 的 HOLD）才应调用；库内若 `Arcade` 来自巡线而当前为 MOTION，则 **忽略** 写入。
2. 任意 `SetLR`/`Arcade`（MANUAL）**Abort MOTION** 后进入 HOLD。
3. 新 `Go`/`Turn` 目标运动 **替换** 当前 HOLD/MOTION。
4. **优先级（main）**：急停 > 遥控(MANUAL) > Busy(MOTION) > 巡线(LINE) > 空闲 Stop。

### 3.2 调用边界

| 上下文 | 允许 |
|--------|------|
| 编码器 GROUP1 ISR | 只读相、查表、累加计数；**禁止**浮点/PID/`Motor_Set` |
| UART/遥控 ISR | **只写邮箱**；禁止 `Chassis_*` / `Motor_*`（除明文约定的极短 flag） |
| 主循环 | `Chassis_*`、`LineTrack_*`、取邮箱 |
| 调试 | `Motor_Set` 单轮点动（可 `#ifdef`） |

### 3.3 `dt_ms` 契约

- 推荐定周期 **5–10 ms** 调 `Chassis_Update(dt_ms)`；允许抖动，**禁止**用 TIMG0 做节拍（TIMG0=PWMA）。
- 超时、积分、航向差分一律 **累加传入的 `dt_ms`**，不假设恒为 10。
- 速度环（P4）按累加时间工作；开环对 `dt` 不敏感。

---

## 4. Hardware

| 模块 | 要点 |
|------|------|
| motor | A/B/C/D ↔ 方向脚 + PWMA C0/C1、PWMB C0/C1（见 pins）；实现 COAST/BRAKE |
| encoder | 与电机同名；GROUP1 四倍频；`ENC_SIGN_*` 标定计数方向 |
| gray | `ReadMask` / `GetPosition`；权重宏在 cfg |
| **imu（预留）** | 见 §4.2；默认未接 |

### 4.1 停车（H 桥语义）

面向 TB6612 类（IN1/IN2 + PWM + STBY）：

| 模式 | 行为 |
|------|------|
| `COAST` | IN1=IN2=0，PWM=0；STBY 仍可保持使能 |
| `BRAKE` | IN1=IN2=1（短刹）或等价短路制动，PWM=0 |
| `DEFAULT` | 映射 `CHASSIS_DEFAULT_STOP_MODE`（建议 BRAKE） |

| 场景 | 默认停法 |
|------|----------|
| `Chassis_Stop(DEFAULT)` / `Abort` | `CHASSIS_DEFAULT_STOP_MODE` |
| MOTION 到位或超时 | `CHASSIS_MOTION_DONE_STOP_MODE`（建议 BRAKE） |
| 巡线丢线且 policy=STOP | `CHASSIS_DEFAULT_STOP_MODE` |

### 4.2 IMU / 陀螺仪预留（P5）

**目的**：航向保持、定角更稳、打滑时航向仍可用；不绑死芯片型号。

| 项 | 约定 |
|----|------|
| 文件 | Hardware/Inc/imu.h · imu.c |
| 关闭时 | `IMU_ENABLED=0`：Init/Update 空，`Imu_DataReady()==false` |
| 符号 | `yaw_deg` **左转增加**；`IMU_YAW_SIGN` 可翻 |
| 周期 | `Imu_Update(dt)` 在 `Chassis_Update` 内或之前 |
| 参考 | `apps/odometry` 的 IMU601（UART 姿态）作**后端**；chassis 只依赖抽象 `imu.h` |

**接口骨架：**

```c
typedef struct {
    float yaw_deg;        /* 相对零点，可多圈 unwrap */
    float yaw_rate_dps;   /* 可选 */
    float pitch_deg;      /* 可选 */
    float roll_deg;       /* 可选 */
} imu_state_t;

void  Imu_Init(void);
void  Imu_Calibrate(void);
void  Imu_Update(uint32_t dt_ms);
bool  Imu_DataReady(void);
void  Imu_Get(imu_state_t *out);
float Imu_GetYawDeg(void);
void  Imu_ResetYaw(void);
```

**总线候选（未定脚）：**

| 方案 | 注意 |
|------|------|
| UART 姿态模块 | car：UART0=DEBUG、UART1=TRANS、UART2=OUT2；接 IMU 须改线/让出口并改 SysConfig/pins |
| I2C 六轴 | 选定空闲脚后写入 pins |
| SPI | 避开 OLED SPI1 |

**航向源（实现阶段）：**

| `CHASSIS_HEADING_SOURCE` | 含义 | 阶段 |
|--------------------------|------|------|
| 0 ENC | 仅编码器差动积分 | P1 默认 |
| 1 IMU | 仅 `Imu_GetYawDeg`（需 `IMU_ENABLED` 且 ready） | P5 |
| 2 预留 | **未定义算法，禁止当作已实现融合** | 不做，直至另开设计 |

打滑辅助（可选，非融合）：`|ω_enc - ω_imu|` 大时置 flag；默认仍 ENC heading。

---

## 5. Chassis

### 5.1 差速与配平

- 命令：`left → C,D` · `right → B,A`
- 再 × `LEFT/RIGHT_TRIM`（/100）与各轮 `POL_*` → `Motor_Set`
- Arcade：`left = throttle + turn_eff`，`right = throttle - turn_eff`，再限幅到 `surface.speed_limit`

```text
turn_eff   = (turn + TURN_BIAS) * surface.turn_scale
left/right = mix(throttle, turn_eff) → clamp(±speed_limit)
每轮 out   = side * TRIM/100 * POL → 死区 → PWM
```

**`POL_*`（电机）与 `ENC_SIGN_*`（计数）必须分开标定**：前进时车体向前由 POL 保证；前进时**脉冲增加**由 ENC_SIGN 保证。禁止用同一宏同时表示两者。

### 5.2 四轮里程 / 测速（写死）

每周期 `Update`：

```text
/* 原始计数已含 ENC_SIGN（Get 时或差分后统一乘一次） */
dA,dB,dC,dD   = Δcount 本周期
pulse_L       = (dC + dD) / 2
pulse_R       = (dB + dA) / 2
dL_mm         = pulse_L * MM_PER_PULSE
dR_mm         = pulse_R * MM_PER_PULSE
ds_mm         = 0.5f * (dL_mm + dR_mm)
dθ_rad        = (dR_mm - dL_mm) / (float)WHEELBASE_MM   /* +左 */
dist_cm      += ds_mm / 10.f
heading_enc  += dθ_rad * (180/π)                        /* 可多圈 */
v_left/right  = (pulse_* / dt_s)                        /* counts/s 或换算 */
```

- `chassis_odom_t.left/right`：累计侧向脉冲（同侧两轮平均累计）
- `a,b,c,d`：四轮累计脉冲
- 同侧两轮差过大：可置 `slip` 调试位；**heading 仍用上式**（P1），除非 P5 切 IMU
- 首次 `Update`：只锁存 prev count，**不积分**（避免上电跳变）

### 5.3 Go / Turn

- `opt==NULL` → 默认（持续或 cfg timeout）
- **Go**：`distance_cm==0`（含 NULL）→ HOLD 持续；`>0` → MOTION，路程恒正，方向= `speed` 符号
- **Turn**：`angle==0` → HOLD 持续旋（方向=speed 符号）；否则 MOTION 定角（方向=angle 符号，speed 取绝对值）
- 推进仅在 `Update`；超时用累加 `dt`
- **straighten**（定距）：用航向误差 × `MOTION_STRAIGHT_KP`（P1 用 ENC heading）微调 turn
- **末端**（建议 P1 最小实现）：剩余距离 < `MOTION_SLOWDOWN_CM` 时降低速度幅值，减超调

### 5.4 地面档

- `turn_scale`：Arcade / 差速转向幅度
- `spin_scale`：**只缩放自旋速度命令**，不改用户目标角
- `angle_gain`：默认 **1.0，不改 θ_target**；仅当系统性欠转/过转且确认几何已准后，才允许 `θ_target_eff = angle * angle_gain`（调参备注写清）。**推荐标定优先改 spin_scale / 轮距，而不是 angle_gain**
- `speed_limit`：该地面最大 |速度|%

`Turn` 定角：`cmd = |speed| * spin_scale`，再限 `speed_limit`；到位判 `|θ_meas - θ_target| < TOL`（θ_target 默认=用户角）。

### 5.5 复位语义

| API | 四轮/侧向积分 | dist_cm | heading_enc | IMU yaw0 |
|-----|---------------|---------|-------------|----------|
| `Chassis_ResetOdom` | 清零积分基准（prev 重锁；硬件 Reset 编码器与否由实现统一） | 0 | 0（enc） | 若 `IMU_ENABLED` 则 `Imu_ResetYaw` |
| `Chassis_ResetHeading` | 不动 dist | 不动 | 0 | 对齐 `Imu_ResetYaw` |
| `Imu_ResetYaw` | 不动 | 不动 | 不动 enc | 当前姿态作 0 |

细节见 [api.md](api.md)。

---

## 6. LineTrack

- `Update` → `Chassis_Arcade(base, pid_turn)` 且 owner=LINE
- 与 MOTION **互斥**（见 §3.1）
- 误差：`err = Gray_GetPosition()`（加权和，量纲与 `GRAY_WEIGHT_*` 一致）；`turn = KP*err + …`，再限幅
- **丢线**：
  - `mask==0` 连续 ≥ `LT_LOST_DEBOUNCE` 拍才触发 policy
  - 0=STOP → `Stop(DEFAULT)`
  - 1=HOLD last turn
  - 2=SEARCH：`Arcade(0, ±LT_SEARCH_TURN)`，最长 `LT_SEARCH_TIMEOUT_MS`，超时则 STOP
- 权重以 **cfg** 为准；pins 表仅对照硬件

---

## 7. 配置宏 `chassis_cfg.h`（必有）

**唯一配置入口**：`Function/Inc/chassis_cfg.h`。  
**类型与 API**（`chassis_surface_t`、`chassis_odom_t`、枚举等）放在 **`chassis.h`**，cfg 只放宏与常量表，避免类型藏在 cfg。

### 7.1 原则

| 做法 | 说明 |
|------|------|
| 编译期宏 | 出厂/赛前默认 |
| 运行时 API | `SetSurface` / `SetTrim` / 可选 `SetTurnBias` |
| 不用裸 μ | 用 turn/spin scale、speed_limit |
| TRIM vs BIAS | TRIM=左右电机不平衡；BIAS=转向零偏 |
| POL vs ENC_SIGN | 电机方向 vs 计数方向 |

### 7.2 完整模板（实现时按实测填数）

```c
#ifndef CHASSIS_CFG_H
#define CHASSIS_CFG_H

#include <stdint.h>
/* surface 表类型在 chassis.h；本文件以宏为主 */

/* ========== 几何 / 编码器 ========== */
#define WHEEL_DIAMETER_MM              (65)      /* 实测 */
#define WHEELBASE_MM                   (160)     /* 左右轮距 */
#define ENCODER_PPR                    (1040)    /* 四倍频后 脉冲/转 */
#define MM_PER_PULSE                   (3.1415926f * (float)WHEEL_DIAMETER_MM / (float)ENCODER_PPR)

/* 计数方向：车体前进时该轮脉冲应增加；与 POL 独立 */
#define ENC_SIGN_A                     (+1)
#define ENC_SIGN_B                     (+1)
#define ENC_SIGN_C                     (+1)
#define ENC_SIGN_D                     (+1)

/* ========== 电机 / PWM ========== */
#define PWM_PERIOD                     (4000)    /* 与 SysConfig timerCount 一致 */
#define PWM_MAX                        (3800)
#define PWM_DEADZONE                   (80)
#define POL_A                          (-1)      /* 右后，点动后改 */
#define POL_B                          (+1)      /* 右前 */
#define POL_C                          (+1)      /* 左前 */
#define POL_D                          (-1)      /* 左后 */
#define LEFT_TRIM                      (92)      /* /100 */
#define RIGHT_TRIM                     (100)

/* ========== 默认速度（百分比）========== */
#define CHASSIS_SPEED_DEFAULT          (35)
#define CHASSIS_SPEED_SLOW             (20)
#define CHASSIS_SPEED_FAST             (55)
#define CHASSIS_SPEED_MAX              (70)
#define CHASSIS_TURN_SPEED_DEFAULT     (30)

/* ========== 运动到位 / 超时 ========== */
#define MOTION_DIST_TOL_CM             (1.0f)
#define MOTION_ANGLE_TOL_DEG           (3.0f)
#define MOTION_TIMEOUT_MS_DEFAULT      (8000)
#define MOTION_STRAIGHT_KP             (0.15f)
#define MOTION_SLOWDOWN_CM             (8.0f)    /* 末端降速区；0=关闭 */

/* ========== 速度环（P4 可选，默认开环）========== */
#define CHASSIS_DEFAULT_SPEED_MODE     (0)       /* 0=OPENLOOP */
#define SPEED_LOOP_DT_MS               (10)      /* 仅参考；实际用累加 dt */
#define SPEED_KP                       (1.2f)
#define SPEED_KI                       (0.15f)
#define SPEED_I_LIMIT                  (1500.f)  /* 抗饱和 */
#define SPEED_OUT_LIMIT                (PWM_MAX)
#define SPEED_PCT_TO_COUNTS_PER_SEC    (50.0f)   /* 100%↔侧向平均 counts/s */

/* ========== 停车 ========== */
/* 0=DEFAULT 1=COAST 2=BRAKE — 与 chassis_stop_mode_t 一致 */
#define CHASSIS_DEFAULT_STOP_MODE      (2)       /* BRAKE */
#define CHASSIS_MOTION_DONE_STOP_MODE  (2)       /* 到位/超时 */

/* ========== 地面档 ========== */
/* 类型 chassis_surface_t / chassis_surface_params_t 在 chassis.h */
#define SURF_NORMAL  { 1.00f, 1.00f, 1.00f, CHASSIS_SPEED_MAX }
#define SURF_LOW     { 0.85f, 0.80f, 1.00f, 45 }   /* angle_gain 默认 1 */
#define SURF_HIGH    { 1.10f, 1.05f, 1.00f, CHASSIS_SPEED_MAX }

#define CHASSIS_SURFACE_DEFAULT        (0)       /* NORMAL */
#define CHASSIS_TURN_BIAS              (0)

/* ========== IMU / 航向（P5，默认关）========== */
#define IMU_ENABLED                    (0)
#define CHASSIS_HEADING_SOURCE         (0)       /* 0=ENC 1=IMU；2=禁止默认开启 */
#define IMU_YAW_SIGN                   (+1)
#define IMU_YAW_RATE_SIGN              (+1)
#define IMU_UPDATE_HZ                   (100)
#define CHASSIS_IMU_STRAIGHT_KP        (1.2f)
#define CHASSIS_IMU_TURN_KP            (2.0f)
#define IMU_BUS_UART                   (0)
#define IMU_BUS_I2C                    (0)

/* ========== 巡线 ========== */
#define LT_BASE_SPEED_DEFAULT          (CHASSIS_SPEED_DEFAULT)
#define LT_KP                          (0.08f)
#define LT_KI                          (0.0f)
#define LT_KD                          (0.12f)
#define LT_TURN_LIMIT                  (40)      /* |pid turn| 限幅 % */
#define LT_LOST_LINE_POLICY            (0)       /* 0STOP 1HOLD 2SEARCH */
#define LT_LOST_DEBOUNCE               (5)       /* 连续丢线拍数 */
#define LT_SEARCH_TURN                 (25)      /* SEARCH 自旋 turn% */
#define LT_SEARCH_TIMEOUT_MS           (1500)
/* 权重以本 cfg 为准（与 pins G1..G8 对照） */
#define GRAY_WEIGHT_0                  (-3500)
#define GRAY_WEIGHT_1                  (-2500)
#define GRAY_WEIGHT_2                  (-1500)
#define GRAY_WEIGHT_3                  (-500)
#define GRAY_WEIGHT_4                  (500)
#define GRAY_WEIGHT_5                  (1500)
#define GRAY_WEIGHT_6                  (2500)
#define GRAY_WEIGHT_7                  (3500)

#endif /* CHASSIS_CFG_H */
```

### 7.3 运行时与宏

| 来源 | 行为 |
|------|------|
| 宏默认速度 | `Go(CHASSIS_SPEED_DEFAULT, …)` |
| `MOTION_*` | NULL opt 的 timeout；straighten Kp；末端降速 |
| surface 表 | Init 载入 DEFAULT；`SetSurface` 热切换 |
| TRIM / BIAS | 宏或 `SetTrim` / `SetTurnBias` |

### 7.4 标定顺序

1. `POL_*`：单轮点动，车体向前  
2. `ENC_SIGN_*`：前进时四轮脉冲增加  
3. `LEFT/RIGHT_TRIM`：开环直行  
4. `CHASSIS_TURN_BIAS`  
5. 几何 PPR / 轮径 / 轮距：定距定角  
6. （P4）速度环 KP/KI/I_LIMIT  
7. `SURF_*`（优先 scale，少动 angle_gain）  
8. 巡线 PID / 丢线参数  
9. （P5）Imu_Calibrate → YAW_SIGN → `HEADING_SOURCE=IMU`

### 7.5 不建议

- main 写死魔法数  
- 假物理 μ 替代 scale  
- 每地面整套 PID 复制  
- 配置分散多 `.c`  
- 用 TIMG0 当控制节拍  
- 默认打开未定义的「融合」航向  

---

## 8. 实现阶段

| 阶段 | 内容 | 验收 |
|------|------|------|
| **P0** | motor + encoder×4 + cfg + Enable/Stop/SetLR/Arcade + 状态机骨架 | 点动、POL/TRIM、遥控差速 |
| **P1** | Update 测速/odom 公式 + Go 定距 + Turn 定角 + timeout + Busy + 到位 BRAKE | 50 cm / ±90° |
| **P2** | gray + LineTrack + main 邮箱仲裁 | 巡线基本可跑 |
| **P3** | surface / turn_bias / straighten / 末端降速 | 换垫少改 PID |
| **P4** | 速度环（抗饱和；可选） | 跟速明显改善 |
| **P5** | imu 后端 + `HEADING_SOURCE` ENC/IMU | 定角更稳；**不做融合** |

API 中：`SetSpeedMode`、`*Block`、`SetSurface`、IMU 相关标为可选；P0–P1 为必达。

---

## 9. 主函数示例

```c
SYSCFG_DL_init();
Chassis_Init();
LineTrack_Init();
Chassis_Enable(true);

/* 定距：Busy=true */
Chassis_Go(CHASSIS_SPEED_DEFAULT,
           &(chassis_go_opt_t){ .distance_cm = 50.f, .straighten = true });
while (Chassis_Busy())
    Chassis_Update(10);

/* 定角 */
Chassis_Turn(CHASSIS_TURN_SPEED_DEFAULT, -90.f, NULL);
while (Chassis_Busy())
    Chassis_Update(10);

/* 持续直行：Busy=false，须自己 Stop，否则会被下面空闲逻辑清掉 */
Chassis_Go(CHASSIS_SPEED_DEFAULT, NULL);
for (int i = 0; i < 100; ++i)
    Chassis_Update(10);
Chassis_Stop(CHASSIS_STOP_DEFAULT);

LineTrack_SetEnable(true);
for (;;) {
    uint32_t dt = 10; /* 建议定周期 */
    /* 取邮箱：estop / remote_cmd … */
    Chassis_Update(dt);

    if (estop) {
        Chassis_Abort();
        Chassis_Enable(false);
        continue;
    }
    if (remote_active) {
        LineTrack_SetEnable(false);
        Chassis_Arcade(cmd.throttle, cmd.turn); /* HOLD, Busy=false */
    } else if (Chassis_Busy()) {
        ; /* MOTION 由 Update 推进 */
    } else if (LineTrack_IsEnabled()) {
        LineTrack_Update();
    } else if (Chassis_GetState() == CHASSIS_STATE_IDLE) {
        Chassis_Stop(CHASSIS_STOP_DEFAULT);
    }
    /* HOLD（持续 Go/遥控）不要每圈 Stop */
}
```

---

## 10. 约束

- 策略/串口 ISR **不**直写 PWM、原则上不调 `Chassis_*`  
- 巡线不调用 `Motor_Set`  
- 编码器 ISR 不做浮点 PID  
- 带目标运动必须有超时  
- 参数集中在 cfg；类型在 `chassis.h`  
- 控制节拍不用 TIMG0  

---

## 11. 文档

| 文件 | 内容 |
|------|------|
| [pins.md](pins.md) | 引脚 |
| **plan.md** | 架构 · 状态机 · odom · 阶段 |
| [api.md](api.md) | API 契约 |
