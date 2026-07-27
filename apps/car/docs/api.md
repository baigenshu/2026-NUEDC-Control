# car 底盘驱动 · API

> 头文件：`chassis.h` · 巡线 `line_track.h` · 配置 `chassis_cfg.h`  
> 轮位：A 右后 · B 右前 · C 左前 · D 左后 · 左=C+D · 右=B+A  
> **符号**：线速度/直行 `+`前 `-`后；转向 `+`左 `-`右  
> 风格：少函数；方向用有符号参数；距离/角度等用 `opt`（`NULL`=默认）  
> 架构 [plan.md](plan.md) · 引脚 [pins.md](pins.md)  
> 更新：2026-07-28

**必选（P0–P2）** vs **可选（P3–P5）** 见各节标注；状态机与 odom 公式以 [plan.md](plan.md) 为准。

---

## 1. 两档能力与状态

| 档 | 接口 | 状态 | `Busy` |
|----|------|------|--------|
| 即时差速 | `SetLR` / `Arcade` | HOLD | false |
| 持续直行/自旋 | `Go(v,NULL)` / `Turn(±ω,0,…)` | HOLD | false |
| 定距 / 定角 | `Go(…, distance>0)` / `Turn(…, angle≠0)` | MOTION | **true** |
| 停车 | `Stop` / `Abort` / 到位 | IDLE | false |

```c
typedef enum {
    CHASSIS_STATE_IDLE = 0,
    CHASSIS_STATE_HOLD,
    CHASSIS_STATE_MOTION,
} chassis_state_t;

chassis_state_t Chassis_GetState(void);
bool            Chassis_Busy(void);   /* 仅 MOTION */
```

| 调用 | 对 MOTION 的影响 |
|------|------------------|
| `SetLR` / `Arcade` | **中止** MOTION → HOLD |
| 新的 `Go` / `Turn` | **替换** 当前 HOLD/MOTION |
| `Stop` / `Abort` | → IDLE |
| `LineTrack_Update` | 不得在 Busy 时调用；库对 MOTION 下的巡线 Arcade **忽略** |

---

## 2. Chassis

### 2.1 生命周期（必选）

| 函数 | 作用 |
|------|------|
| `Chassis_Init()` | motor/encoder + cfg；状态 IDLE；默认停 |
| `Chassis_Enable(bool on)` | STBY |
| `Chassis_Update(uint32_t dt_ms)` | **必调**：测速、odom、速度环(可选)、MOTION 推进 |
| `Chassis_GetState()` / `Busy()` | 见上 |
| `Chassis_Abort(void)` | 取消目标 + `Stop(DEFAULT)` → IDLE |

`dt_ms`：建议 5–10 ms 定周期；超时/积分**累加** `dt_ms`。禁止用 TIMG0 作节拍（PWMA）。

### 2.2 停车（必选）

```c
typedef enum {
    CHASSIS_STOP_DEFAULT = 0, /* → CHASSIS_DEFAULT_STOP_MODE */
    CHASSIS_STOP_COAST,       /* IN1=IN2=0, PWM=0 */
    CHASSIS_STOP_BRAKE,       /* 短刹 IN1=IN2=1, PWM=0 */
} chassis_stop_mode_t;

void Chassis_Stop(chassis_stop_mode_t mode);
```

| 场景 | 模式 |
|------|------|
| 用户 `Stop(DEFAULT)` / `Abort` | `CHASSIS_DEFAULT_STOP_MODE`（cfg，建议 BRAKE） |
| MOTION 到位或超时 | `CHASSIS_MOTION_DONE_STOP_MODE`（建议 BRAKE） |

`Stop` → IDLE，Busy=false。HOLD 持续运动**不会**自动停，需 `Stop`/`Abort`/新指令。

### 2.3 即时差速（必选 P0）

```c
void Chassis_SetLR(int16_t left, int16_t right);     /* -100..+100 */
void Chassis_Arcade(int16_t throttle, int16_t turn); /* turn +左 -右 */
```

- 进入 HOLD；中止 MOTION  
- 经 TURN_BIAS、surface.turn_scale、speed_limit、TRIM、POL 后下发  
- 保持到再次改指令或 Stop  

### 2.4 直行（必选 P1 定距；持续 P0 可用 Go→等价 Set 差速）

```c
typedef struct {
    float    distance_cm; /* 0=持续 HOLD；>0 路程 cm（恒正）→ MOTION */
    bool     straighten;  /* 定距时航向纠偏（P3；P1 可先 false） */
    uint32_t timeout_ms;  /* 0=cfg 默认；仅 MOTION */
} chassis_go_opt_t;

void Chassis_Go(int16_t speed_pct, const chassis_go_opt_t *opt);
/* speed_pct: +前 -后；opt==NULL → 持续 HOLD */
```

```c
Chassis_Go(40, NULL);           /* HOLD 持续前 */
Chassis_Go(-30, NULL);          /* HOLD 持续后 */
Chassis_Go(35, &(chassis_go_opt_t){ .distance_cm = 50.f }); /* MOTION */
Chassis_Go(40, &(chassis_go_opt_t){
    .distance_cm = 80.f, .straighten = true, .timeout_ms = 5000 });
```

定距：积分 `dist`（plan §5.2）；`|剩余| < TOL` 或超时 → 到位停。可选剩余 < `MOTION_SLOWDOWN_CM` 降速。

### 2.5 转向（必选 P1 定角）

```c
typedef struct {
    uint32_t timeout_ms; /* 0=cfg 默认；仅 MOTION */
} chassis_turn_opt_t;

void Chassis_Turn(int16_t speed_pct, float angle_deg, const chassis_turn_opt_t *opt);
```

| 条件 | 状态 | 方向 |
|------|------|------|
| `angle_deg != 0` | MOTION | angle 符号（+左 -右）；speed 取绝对值 × spin_scale |
| `angle_deg == 0` | HOLD | 持续旋；speed 符号（+左 -右） |

```c
Chassis_Turn(40,  90.f, NULL);
Chassis_Turn(40, -90.f, NULL);
Chassis_Turn(35,   0.f, NULL);  /* HOLD */
Chassis_Turn(-35,  0.f, NULL);
```

定角：`θ_target` **默认等于用户角**（不乘 angle_gain，除非标定注明）；到位 `|heading - θ0 - θ_target| < TOL`。

### 2.6 模式 · 配平 · 地面 · 里程

```c
typedef enum {
    CHASSIS_MODE_OPENLOOP = 0,
    CHASSIS_MODE_SPEED,          /* P4 可选 */
} chassis_speed_mode_t;

typedef enum {
    CHASSIS_SURFACE_NORMAL = 0,
    CHASSIS_SURFACE_LOW_GRIP,
    CHASSIS_SURFACE_HIGH_GRIP,
    CHASSIS_SURFACE_CUSTOM,
} chassis_surface_t;

typedef struct {
    float   turn_scale;
    float   spin_scale;    /* 只缩转速，不改目标角 */
    float   angle_gain;    /* 默认 1.0；慎用 */
    int16_t speed_limit;
} chassis_surface_params_t;

void Chassis_SetSpeedMode(chassis_speed_mode_t m);   /* 可选 P4 */
void Chassis_SetTrim(uint8_t left, uint8_t right);   /* 0..100，/100 */
void Chassis_SetSurface(chassis_surface_t s);        /* 可选 P3 */
void Chassis_SetTurnBias(int8_t bias);               /* 可选 */
void Chassis_ResetOdom(void);
void Chassis_GetOdom(chassis_odom_t *o);

typedef struct {
    int32_t a, b, c, d;      /* 四轮累计脉冲（含 ENC_SIGN） */
    int32_t left, right;     /* 侧向平均累计 */
    float   dist_cm;
    float   heading_deg;     /* 当前航向源 */
    float   v_left, v_right; /* 侧向速度，counts/s 或文档约定单位 */
    float   yaw_imu_deg;     /* P5；未接为 0 */
    uint8_t imu_ready;       /* 0/1 */
    uint8_t slip;            /* 可选打滑标志 */
} chassis_odom_t;
```

**类型均在 `chassis.h`，不在 cfg。**

四轮合成公式（实现必须一致）：

```text
pulse_L = (ΔC + ΔD) / 2
pulse_R = (ΔB + ΔA) / 2
ds_mm   = 0.5 * (pulse_L + pulse_R) * MM_PER_PULSE
dθ_rad  = (pulse_R - pulse_L) * MM_PER_PULSE / WHEELBASE_MM   /* +左 */
```

`POL_*` 与 `ENC_SIGN_*` 分离，见 plan。

### 2.7 航向 / IMU（可选 P5）

未接陀螺：`heading_deg` 仅编码器差动，`imu_ready=0`。  
接入后（`IMU_ENABLED=1`）仍用 Go/Turn；`CHASSIS_HEADING_SOURCE`：`0=ENC`，`1=IMU`。  
**`2=融合` 未定义，不得默认开启。**

```c
void  Imu_Init(void);
void  Imu_Calibrate(void);
void  Imu_Update(uint32_t dt_ms);
bool  Imu_DataReady(void);
float Imu_GetYawDeg(void);
void  Imu_ResetYaw(void);

float Chassis_GetHeadingDeg(void);
void  Chassis_ResetHeading(void);
```

| API | dist_cm | heading_enc | IMU 零点 |
|-----|---------|-------------|----------|
| `ResetOdom` | 0 | 0 | 若启用 IMU 则 `Imu_ResetYaw` |
| `ResetHeading` | 不动 | 0 | `Imu_ResetYaw` |
| `Imu_ResetYaw` | 不动 | 不动 enc | 当前 yaw 作 0 |

定角 / straighten 按航向源用误差 × `MOTION_STRAIGHT_KP` 或 `CHASSIS_IMU_*_KP`。

### 2.8 调试阻塞（可选）

```c
bool Chassis_GoBlock(int16_t speed, const chassis_go_opt_t *opt, uint32_t poll_ms);
bool Chassis_TurnBlock(int16_t speed, float angle_deg,
                       const chassis_turn_opt_t *opt, uint32_t poll_ms);
```

仅调试；赛场主循环应用非阻塞 + `Busy`。

---

## 3. LineTrack（必选 P2）

| 函数 | 作用 |
|------|------|
| `LineTrack_Init()` | 初始化 |
| `LineTrack_SetEnable(bool)` / `IsEnabled()` | 开停 |
| `LineTrack_SetBaseSpeed(int16_t pct)` | 基速 |
| `LineTrack_Update()` | → `Arcade`（owner=LINE）；**要求 !Busy** |
| `LineTrack_GetError()` / `GetMask()` | 调试 |

- `error` 与 `GRAY_WEIGHT_*` 同量纲；`turn` 限幅 `LT_TURN_LIMIT`  
- 丢线：连续 `LT_LOST_DEBOUNCE` 拍 `mask==0` 后执行 policy（STOP/HOLD/SEARCH）  
- SEARCH：`LT_SEARCH_TURN` + `LT_SEARCH_TIMEOUT_MS`  
- 权重以 **cfg** 为准  

与 MOTION 互斥；main 在 Busy 时不调用 Update。

---

## 4. Hardware（校准 / 驱动）

| API | 说明 |
|-----|------|
| `Motor_Set(id, signed_duty)` | 单轮；含死区；调试点动 |
| `Motor_SetEnable(bool)` | STBY |
| `Motor_StopAll(mode)` | COAST/BRAKE（也可由 Chassis 调） |
| `Encoder_Init` / `Get*` / `Reset*` | 四轮；ISR 只计数 |
| `Gray_ReadMask` / `GetPosition` | mask bit0=G1…；position=加权和 |

---

## 5. 运动速查

| 目的 | 调用 | Busy |
|------|------|------|
| 持续前/后 | `Go(+v,NULL)` / `Go(-v,NULL)` | no |
| 定距 | `Go(±v, &opt{.distance_cm=L})` | yes |
| 定距+直线 | 同上 `.straighten=true` | yes |
| 持续旋 | `Turn(+ω,0,NULL)` / `Turn(-ω,0,NULL)` | no |
| 定角 | `Turn(ω, ±deg, NULL)` | yes |
| 弧线/遥控 | `Arcade` / `SetLR` | no |
| 停 / 取消 | `Stop(mode)` / `Abort()` | no |
| 速度闭环 | `SetSpeedMode(SPEED)`（P4） | — |
| 巡线 | `LineTrack_Update`（!Busy） | — |

```c
Chassis_Go(CHASSIS_SPEED_DEFAULT, &(chassis_go_opt_t){ .distance_cm = 50.f });
while (Chassis_Busy())
    Chassis_Update(10);
```

---

## 6. 配置宏 `chassis_cfg.h`

完整分组见 [plan.md §7](plan.md)。实现时**禁止**在 main 写死速度/超时字面量。

| 分组 | 示例 |
|------|------|
| 几何 | 轮径 · 轮距 · PPR · `MM_PER_PULSE` |
| 编码器符号 | `ENC_SIGN_A..D` |
| 电机 | `POL_*` · TRIM · 死区 |
| 运动 | TOL · timeout · straighten Kp · slowdown |
| 停车 | `DEFAULT` / `MOTION_DONE` stop mode |
| 速度环 P4 | KP/KI/`I_LIMIT` · 标度 |
| 地面 P3 | scale · limit（angle_gain 默认 1） |
| 巡线 | `LT_*` · 权重 · debounce/search |
| IMU P5 | `IMU_ENABLED` · `HEADING_SOURCE` 0/1 |

```c
void Chassis_SetSurface(chassis_surface_t s);
void Chassis_SetTrim(uint8_t left, uint8_t right);
void Chassis_SetTurnBias(int8_t bias); /* 可选 */
```

---

## 7. 编码器

- ISR：GROUP1 四倍频查表，短路径  
- `Update`：差分 → 侧向平均 → 速度与 odom  
- 首次 Update 不积分  
- 可选：指令非零且脉冲长期为 0 → 故障/打滑 flag  

---

## 7.1 陀螺仪 / IMU（P5）

| 状态 | 行为 |
|------|------|
| `IMU_ENABLED=0` | stub；航向=编码器 |
| `IMU_ENABLED=1` + source=1 | `Imu_Update`→yaw 作 heading |
| source=2 | **未实现，禁止当融合用** |
| 零点 | 见 §2.7 表 |

见 [pins.md §9](pins.md)、[plan.md §4.2](plan.md)。参考 odometry IMU601，**勿占 UART1=TRANS**。

---

## 8. 联动与 main 仲裁

```text
灰度 ─► LineTrack ─► Arcade(LINE) ─┐
编码器 ─► 测速/里程 ───────────────►├─ Update ─► Motor×4
IMU(P5) ─► yaw ───────────────────►│
邮箱 ◄─ 遥控 ISR（只写）            │
main ─► Arcade/Go/Turn / Stop ────┘
```

**优先级**：急停 > 遥控 > Busy(MOTION) > 巡线 > 仅 IDLE 时 Stop。  
**HOLD（持续/遥控）时不要每圈 Stop。**

```c
for (;;) {
    /* 取邮箱 */
    Chassis_Update(dt);
    if (estop) { Chassis_Abort(); Chassis_Enable(false); continue; }
    if (remote) {
        LineTrack_SetEnable(false);
        Chassis_Arcade(cmd.throttle, cmd.turn);
    } else if (Chassis_Busy()) {
        ;
    } else if (LineTrack_IsEnabled()) {
        LineTrack_Update();
    } else if (Chassis_GetState() == CHASSIS_STATE_IDLE) {
        Chassis_Stop(CHASSIS_STOP_DEFAULT);
    }
}
```

---

## 9. API 清单

```text
必选 P0–P1:
  Chassis_Init / Enable / Update / GetState / Busy / Abort
  Chassis_Stop(mode)
  Chassis_SetLR / Arcade
  Chassis_Go / Turn
  Chassis_SetTrim
  Chassis_ResetOdom / GetOdom / GetHeadingDeg / ResetHeading

P2: LineTrack_*
P3: SetSurface / SetTurnBias / straighten 完善
P4: SetSpeedMode(SPEED) + 抗饱和 PI
P5: Imu_* ；HEADING_SOURCE 0|1

可选调试: GoBlock / TurnBlock / Motor_Set 点动
```
