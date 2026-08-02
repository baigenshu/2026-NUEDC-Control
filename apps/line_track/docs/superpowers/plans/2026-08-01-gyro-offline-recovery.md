# 陀螺仪辅助丢线恢复（方案 A）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 IR 丢线（`s_active==0`）期间，用 IMU601 航向角速率维持丢线前的转弯速率，使车穿过盲区找回线、不跑飞；在线循迹完全不变。

**Architecture:** 每 10ms 控制周期算 `yaw_rate`（°/s，右转为正）。在线时 IR 循迹不变，同时 `rate_ref` 低通跟踪 `yaw_rate`；丢线时绕过 IR PID，改用 `pid = LF_K_YAW·(rate_ref − yaw_rate)` 喂同一套差速公式。丢线超 500ms 或 IMU 未就绪则停车。

**Tech Stack:** MSPM0G3507 (Cortex-M0+, bare-metal C), SysConfig, Make + arm-none-eabi-gcc, 无单元测试框架（验证 = 编译 + 烧录 + 串口观测）。

## Global Constraints

- 平台：MSPM0G3507 LQFP-64，80MHz，nano.specs（不支持 float printf，遥测用 `(long)(x*10.0f)`）。
- 控制周期 10ms（`main.c` `SAMPLE_MS=10`，`LineFollow_Update` 每周期调一次）。
- Makefile 不跟踪头文件依赖：**改任何 `.h` 后必须 `make clean` 再 `make`**。
- IMU601 已就绪：`IMU601_Attitude.yaw`（°，float）、`IMU601_FrameCount`（uint32_t，0=未收到帧）。`#include "imu601.h"`。
- 串口观测规则：**只在用户说"开始抓包"时**才启动 COM15 抓包；否则由用户直接观测/反馈。
- 不改 `speed_ctrl` / `imu601` / `main.c`（`main.c` 100ms yaw 透传保留）。

## 关键设计决策（spec 未明的歧义，在此明确）

- **丢线超时 = 锁定停车**：超 500ms 未找回线 → `stop_all()` 并置 `s_loss_latched`，**保持停车直到用户重新按 KEY_RUN**（`SetEnable(true)→Reset` 清锁）。不自动恢复——高速下盲转 500ms 后车多半已离轨，自动恢复危险。短时丢线（<500ms）未停车时，找回线立即切回 IR（在线分支自动复位 `s_loss_ms`）。
- **`yaw_rate` 单位用 °/s**（`delta°/周期 × 100`），与 spec 给的 `LF_K_YAW=0.3` 初值单位 `(脉冲/10ms)/(°/s)` 一致。
- **`rate_ref` 仅在线更新**，丢线冻结作设定值。
- **首周期 `yaw_rate=0`**（`s_yaw_has_prev` 守卫），避免上电 yaw 跳变污染 `rate_ref`。

## File Structure

- **Modify** `src/Function/Inc/line_follow_cfg.h` — 新增 5 个宏（4 个设计参数 + 控制周期常量）。
- **Modify** `src/Function/Src/line_follow.c` — `#include "imu601.h"`；新增 5 个静态状态；`LineFollow_Update` 加 yaw_rate 计算 + 在线/丢线分支；`LineFollow_Reset` 清新状态。
- 不新建文件。

---

### Task 1: 新增陀螺仪丢线恢复参数

**Files:**
- Modify: `src/Function/Inc/line_follow_cfg.h`（在 `LF_MAX_SPD_FRAC` 之后、速度 PID 段之前插入）

**Interfaces:**
- Produces: `LF_K_YAW`、`LF_RATE_ALPHA`、`LF_LOSS_TIMEOUT_MS`、`LF_YAW_SIGN`、`LF_CTRL_DT_MS`（供 Task 2 使用）

- [ ] **Step 1: 插入参数块**

在 `line_follow_cfg.h` 第 47 行 `#define LF_MAX_SPD_FRAC (1.25f)` 之后、第 49 行速度 PID 注释块之前，插入：

```c

/* ================================================================
 * 陀螺仪丢线恢复（方案 A：航向角速率保持）
 *   丢线时绕过 IR PID，按丢线前转弯角速率 rate_ref 继续转
 *   pid = LF_K_YAW * (rate_ref - yaw_rate)
 *   yaw_rate 单位 °/s（右转为正）；rate_ref 在线低通跟踪 yaw_rate，丢线冻结
 *   丢线持续 > LF_LOSS_TIMEOUT_MS → 锁定停车（需 KEY_RUN 重使能）
 * ================================================================ */
#define LF_K_YAW                 (0.3f)   /* 角速率环增益 (脉冲/10ms)/(°/s)，上机调 */
#define LF_RATE_ALPHA            (0.2f)   /* rate_ref 低通系数 */
#define LF_LOSS_TIMEOUT_MS       (500u)   /* 丢线超时阈值 (ms) */
#define LF_YAW_SIGN              (+1)     /* ±1：右转 yaw 增加取 +1，否则 -1，上机定 */
#define LF_CTRL_DT_MS            (10u)    /* 控制周期 ms（= main.c SAMPLE_MS）*/
```

- [ ] **Step 2: 验证编译（纯宏，无行为变化）**

Run: `make clean && make`
Expected: 0 errors。（此时新宏未被引用，不影响行为。）

- [ ] **Step 3: Commit**

```bash
git add src/Function/Inc/line_follow_cfg.h
git commit -m "feat(line_follow): add gyro off-line recovery params (unused)"
```

---

### Task 2: 实现丢线恢复逻辑（yaw-rate hold）

**Files:**
- Modify: `src/Function/Src/line_follow.c`（include、静态状态、`LineFollow_Update`、`LineFollow_Reset`）

**Interfaces:**
- Consumes: `IMU601_Attitude.yaw`（volatile float，°）、`IMU601_FrameCount`（volatile uint32_t）from `imu601.h`；Task 1 的 5 个宏。
- Produces: 行为变化（丢线时角速率保持），无新公开 API。

- [ ] **Step 1: 加 include**

在 `line_follow.c` 第 16 行 `#include "encoder.h"` 之后加一行：

```c
#include "imu601.h"
```

- [ ] **Step 2: 加静态状态**

在 `line_follow.c` 第 28 行 `static float s_integral;` 之后插入：

```c

/* 陀螺仪丢线恢复状态 */
static float    s_yaw_prev;        /* 上周期 yaw（已乘 LF_YAW_SIGN）*/
static bool     s_yaw_has_prev;    /* 首周期守卫 */
static float    s_rate_ref;        /* 在线跟踪的转弯角速率参考 (°/s) */
static uint16_t s_loss_ms;         /* 连续丢线累计时间 (ms) */
static bool     s_loss_latched;    /* 丢线超时锁定（需 KEY_RUN 重使能）*/
```

- [ ] **Step 3: `LineFollow_Reset` 清新状态**

在 `line_follow.c` `LineFollow_Reset` 中，第 68 行 `s_active = 0;` 之后插入：

```c
    s_yaw_prev     = 0.f;
    s_yaw_has_prev = false;
    s_rate_ref     = 0.f;
    s_loss_ms      = 0u;
    s_loss_latched = false;
```

- [ ] **Step 4: 重写 `LineFollow_Update` 的核心段**

将 `line_follow.c` 第 104–172 行整个 `LineFollow_Update` 函数替换为：

```c
void LineFollow_Update(void)
{
    uint8_t  i;
    float    sum;
    float    deriv;
    float    pid;
    float    pid_lim;
    float    maxv;
    float    left_f, right_f;
    float    yaw_now, delta, yaw_rate;

    if (!s_enabled) {
        stop_all();
        return;
    }
    if (s_loss_latched) {           /* 丢线超时已锁定停车，等 KEY_RUN 重使能 */
        stop_all();
        return;
    }

    /* ---- 0. 航向角速率（°/s，右转为正）---- */
    yaw_now = IMU601_Attitude.yaw * (float)LF_YAW_SIGN;
    if (s_yaw_has_prev) {
        delta = yaw_now - s_yaw_prev;
        if (delta > 180.f)   delta -= 360.f;   /* 0↔360 跨越处理 */
        if (delta < -180.f)  delta += 360.f;
        yaw_rate = delta * 100.f;              /* dt=10ms → °/s */
    } else {
        yaw_rate = 0.f;                        /* 首周期 */
    }
    s_yaw_prev     = yaw_now;
    s_yaw_has_prev = true;

    /* ---- 1. 读传感器 ---- */
    Ir4_ReadRaw(s_sensor);
    s_mask   = 0;
    s_active = 0;
    for (i = 0; i < IR4_CH_COUNT; ++i) {
        if (s_sensor[i]) {
            s_mask |= (uint8_t)(1u << i);
            s_active++;
        }
    }

    /* ---- 2/3. 在线：IR 位置 PID；丢线：航向角速率保持 ---- */
    if (s_active > 0u) {
        /* 在线：加权线位置 */
        sum = 0.f;
        for (i = 0; i < IR4_CH_COUNT; ++i) {
            if (s_sensor[i])
                sum += s_w[i];
        }
        s_error = sum / (float)s_active;

        /* 位置 PID（位置式，积分限幅）*/
        s_integral += s_error;
        if (s_integral > LF_I_MAX)  s_integral = LF_I_MAX;
        if (s_integral < -LF_I_MAX) s_integral = -LF_I_MAX;
        deriv = s_error - s_last_error;
        pid   = LF_KP * s_error + LF_KI * s_integral + LF_KD * deriv;

        /* 角速率参考低通跟踪（在线更新，丢线冻结）*/
        s_rate_ref += LF_RATE_ALPHA * (yaw_rate - s_rate_ref);
        s_loss_ms  = 0u;
    } else {
        /* 丢线 */
        if (IMU601_FrameCount == 0u) {         /* 陀螺仪未就绪 → 停车（不瞎转）*/
            stop_all();
            return;
        }
        s_loss_ms += LF_CTRL_DT_MS;
        if (s_loss_ms > LF_LOSS_TIMEOUT_MS) {  /* 丢线超时 → 锁定停车 */
            s_loss_latched = true;
            stop_all();
            return;
        }
        /* 航向角速率保持：按丢线前转弯速率继续转 */
        pid     = LF_K_YAW * (s_rate_ref - yaw_rate);
        s_error = s_last_error;                /* 遥测：保留最后已知线偏移 */
    }

    /* ---- 4. PID 输出限幅 + 有符号差速 ---- */
    pid_lim = (float)s_base_spd * LF_PID_OUT_FRAC;
    if (pid > pid_lim)   pid = pid_lim;
    if (pid < -pid_lim)  pid = -pid_lim;

    left_f  = (float)s_base_spd + pid;
    right_f = (float)s_base_spd - pid;
    maxv    = (float)s_base_spd * LF_MAX_SPD_FRAC;
    if (left_f  >  maxv) left_f  =  maxv;
    if (left_f  < -maxv) left_f  = -maxv;
    if (right_f >  maxv) right_f =  maxv;
    if (right_f < -maxv) right_f = -maxv;

    /* ---- 5. 送内环速度 PID ---- */
    SpeedCtrl_SetTargetLR((int16_t)left_f, (int16_t)right_f);
    SpeedCtrl_Update();

    /* ---- 6. 遥测 ---- */
    for (i = 0; i < 4; ++i)
        s_enc[i] = SpeedCtrl_GetEncoderDelta((speed_id_t)i);

    s_last_error = s_error;
}
```

变更要点（相对原函数）：新增 step 0（yaw_rate）+ `s_loss_latched` 早退；原 step 2/3 拆成在线/丢线两分支；在线分支末尾加 `rate_ref` 低通 + `s_loss_ms=0`；丢线分支用角速率环算 `pid`、不碰积分；PID 输出限幅移到分支后共享。在线路径的 IR 误差/PID/差速/限幅数值完全不变。

- [ ] **Step 5: 编译**

Run: `make clean && make`
Expected: 0 errors, 0 warnings。

- [ ] **Step 6: Commit**

```bash
git add src/Function/Src/line_follow.c
git commit -m "feat(line_follow): gyro yaw-rate hold during IR off-line"
```

---

### Task 3: 烧录 + 上车验证 + 调参

**Files:** 无代码改动（仅在 `line_follow_cfg.h` 调 `LF_YAW_SIGN` / `LF_K_YAW` 后重编重烧）

无单元测试框架。验证 = 编译通过 → 用户烧录 → 串口/现象观测。串口抓包按规则只在用户说"开始抓包"时启动。

- [ ] **Step 1: 烧录，直线段回归**

用户烧录。观察：直线段循迹与改前一致，HB 行 `m=` 以居中（`06`）为主，`e≈0`，`st=1`，`enc` 四轮正常。**不应有任何回归。**

- [ ] **Step 2: 弧段丢线恢复**

跑急弯/弧。预期：丢线时（`m=00`）车按原转弯方向继续转、短时找回线（`m=` 恢复非 00），**不跑飞**。`m=00` 持续应 <500ms。

- [ ] **Step 3: 长时丢线停车**

把车抬离线（或跑完全无线区）>500ms。预期：车自锁停车（`st=0`、`enc=0`），且**不再自动恢复**；按 KEY_RUN 关再开后恢复正常。

- [ ] **Step 4: 校 `LF_YAW_SIGN`**

若丢线时车**转向方向反了**（往外转/跑飞加剧）：把 `line_follow_cfg.h` 里 `LF_YAW_SIGN` 由 `(+1)` 改为 `(-1)`，`make clean && make` 重烧。预期：丢线时转向与原弯一致。

- [ ] **Step 5: 调 `LF_K_YAW`**

- 丢线时**转弯不够**（切线往外飘、找回线慢）→ 加大 `LF_K_YAW`（如 0.3→0.5→0.8）。
- 丢线时**振荡/抖动**→ 减小 `LF_K_YAW`（如 0.3→0.2）。
每次改后 `make clean && make` 重烧。

- [ ] **Step 6: Commit 最终参数**

```bash
git add src/Function/Inc/line_follow_cfg.h
git commit -m "tune(line_follow): gyro recovery sign/gain after on-car test"
```

---

## Self-Review

**Spec coverage：** yaw_rate 计算（含 0↔360 跨越 + LF_YAW_SIGN）✓；在线 IR 不变 + rate_ref 低通 ✓；丢线 pid=K_YAW·(rate_ref−yaw_rate) + 同差速/限幅 ✓；丢线 >500ms 停车 ✓（锁定）；IMU 未就绪丢线停车 ✓；找回线切回 IR ✓（非锁定短时丢线）；4 参数 + Reset 清状态 ✓；只动 line_follow.c + cfg.h ✓。

**Placeholder 扫描：** 无 TBD/TODO，所有代码完整，初值明确（K_YAW=0.3/ALPHA=0.2/TIMEOUT=500/SIGN=+1）。

**类型一致：** `s_yaw_prev`(float)、`s_yaw_has_prev`(bool)、`s_rate_ref`(float)、`s_loss_ms`(uint16_t)、`s_loss_latched`(bool)；`LF_YAW_SIGN` 用 `(float)` 强转；`LF_CTRL_DT_MS`(10u) 累加到 uint16_t，与 `LF_LOSS_TIMEOUT_MS`(500u) 同类型比较；`IMU601_Attitude.yaw`/`IMU601_FrameCount` 为 volatile，读入局部安全。一致。

**已知偏离 spec（已明确）：** spec step 5"重新找到线→切回 IR"对超时后的情形未定义；本计划把超时定为**锁定停车**（需 KEY_RUN 重使能），更安全。短时丢线（<500ms）找回线仍自动切回 IR，符合 spec。
