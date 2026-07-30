# 视觉球位 UART 协议（balance）

> 状态：**已定稿** · 2026-07-30  
> 用途：MaixCAM / 视觉模块 → balance 主控，上报钢珠相对 O 点的一维位置  
> 发送端参考：`apps/maixcam/opencv/main.py`（`pack_ball_frame`）  
> MCU 头文件：`src/Hardware/Inc/ball_proto.h`

---

## 1. 物理层

| 项 | 约定 |
|----|------|
| 电平 | 3.3 V TTL，**必须共地** |
| 波特率 | **115200** 8N1 |
| 流向 | 视觉 **TX →** 主控 **RX**（控球只需下行；回传可选） |
| balance 接线（预留） | UART0：`PA31=RX` · `PA28=TX` |
| MaixCAM | A16=UART0_TX · A17=UART0_RX（MaixCAM2：A21/A22 → UART4） |
| 节流 | 视觉侧建议 ≥ 20 ms/帧（≤ 50 Hz），与 `TX_MIN_MS` 一致 |

启动文本（可选，便于串口助手观察，**不参与二进制解析**）：

```text
BALL ready\r\n
```

---

## 2. 协议族

与云台红点帧同属一套 framing，靠 **type** 区分：

| type | 长度 | 用途 | 工程 |
|------|------|------|------|
| `0x01` | 15 B | 红点误差 + IMU | `maixcam/red_track`（云台，本板忽略） |
| **`0x02`** | **13 B** | **钢珠一维位置** | **balance 控球（本协议）** |

> 图传 JPEG 也用 `AA 55` 但后续为 `u32 len + jpeg`，**禁止与控球口混用且无分帧**。

---

## 3. 球位帧 · type = `0x02`（定长 13 字节）

### 3.1 字节布局（小端 LE）

```text
Offset  Size  字段         类型     说明
─────────────────────────────────────────────────────────
 0      1     magic0       u8       固定 0xAA
 1      1     magic1       u8       固定 0x55
 2      1     type         u8       固定 0x02
 3      1     flags        u8       bit0 = found（1=锁定，0=丢失）
 4–5    2     pos_mm       i16 LE   相对 O 的位置，单位 **1 mm**（整毫米）
 6–7    2     cx           i16 LE   像素质心 x（调试）
 8–9    2     cy           i16 LE   像素质心 y（调试）
10      1     conf         u8       置信度 0–100
11      1     mode         u8       检测模式：0=BRI / 1=DRK / 2=AUT
12      1     checksum     u8       sum(bytes[2..11]) & 0xFF
```

**总长 13 字节**。body = `bytes[2..11]`（10 字节），checksum 仅覆盖 body。

### 3.2 打包（与视觉侧一致）

```c
/* Python 等价：struct.pack("<BBhhhBB", type, flags, pos, cx, cy, conf, mode) */
uint8_t body[10];
body[0] = BALL_FRAME_TYPE;           /* 0x02 */
body[1] = found ? 0x01 : 0x00;
/* pos_mm / cx / cy：int16 little-endian；pos 单位 1mm */
body[8] = conf;                      /* 0..100 */
body[9] = mode;                      /* 0/1/2 */
checksum = sum(body[0..9]) & 0xFF;
frame = {0xAA, 0x55} + body + {checksum};
```

### 3.3 示例

| 场景 | 十六进制（示意） | 含义 |
|------|------------------|------|
| 丢球 | `AA 55 02 00 00 00 00 00 00 00 00 00 02` | found=0，pos=0，csum=0x02 |
| 球在 O 右侧 +12 mm | type=02 flags=01 pos_mm=+12 … | 整毫米 |
| 球在 O 左侧 −5 mm | pos_mm=−5（`FB FF` LE） | 整毫米 |

> 校验和随 payload 变化，上表丢球行 csum 仅为 body 全 0 时的 `0x02`。

---

## 4. 坐标与单位

```text
        负方向 ←———— O ————→ 正方向
              |   凹槽 / 摆杆轴向
              └── 视觉 ROI 水平中线 = O（O_OFFSET_PX）
```

| 量 | 定义 |
|----|------|
| 原点 O | 凹槽 ROI 水平中心（视觉标定，对应机械中位） |
| `pos_mm` | 球心相对 O，**+1 = +1 mm**（视觉已量化，抑抖） |
| 量程 | int16 → 理论 ±3276.7 mm；实际由杆长（约 ±125 mm @ 250 mm 杆）与软限位约束 |
| 极性 | **与步进正方向同号**：视觉 + → 丝杆目标 +（若实装反了，只在 MCU 或 `STEPPER_DIR_SIGN` 取反，**协议本身不改号**） |

### 与步进 API 换算

| 视觉 | 步进 / 控制（balance） |
|------|------------------------|
| `pos_mm`（整 mm） | `mm_x100 = pos_mm * 100` |
| 分辨率 | 视觉 **1 mm**（滞回量化）；步进约 0.0005 mm/step |

```c
/* found==1 时跟球；内部仍用 0.01 mm */
int32_t ball_mm_x100 = ball_pos_to_mm_x100(msg.pos_mm); /* *100 */
```

---

## 5. flags / conf / mode

| 字段 | 规则 |
|------|------|
| `flags.bit0` | **1**：本帧球有效，可信赖 `pos_mm`；**0**：丢失，**控制环不得当新目标** |
| `flags` 其余位 | 保留，接收端忽略 |
| `conf` | 0–100；建议 `< BALL_CONF_MIN`（默认 30）时等同丢球 |
| `mode` | 仅遥测；MCU 可不使用 |
| `cx/cy` | 像素坐标，调试；闭环**只用** `pos_mm` |

---

## 6. 接收状态机（MCU）

```text
IDLE ──(0xAA)──► GOT_AA ──(0x55)──► GOT_HDR
                                      │
                                      ▼ 收满 10 字节 body
                                   CHECK ──csum OK && type==0x02──► 交付业务
                                      │ 失败 / 非 0x02
                                      └──► IDLE（type==0x01 可整帧丢弃或另解析）
```

要点：

1. 任意阶段收到错误字节 → 若为 `0xAA` 则回到 `GOT_AA`，否则 `IDLE`。
2. **先校验 type 与长度**：`0x02` 定长 13；勿按变长解析。
3. 校验：`sum(buf[2..11]) & 0xFF == buf[12]`。
4. **超时**：建议 `BALL_UART_TIMEOUT_MS = 100`；超时视为丢球，触发安全策略。
5. 文本 `BALL ready\r\n` 不会通过 magic 匹配，自然被状态机忽略。

---

## 7. 业务约定（控制侧）

| 项 | 建议 |
|----|------|
| 有效帧 | `found && conf >= 30 && csum_ok` |
| 丢球 / 超时 | 保持上次目标 **或** 缓回 O（业务定）；禁止继续跟噪声 |
| 软限位 | 目标仍受 `STEPPER_SOFT_*` 钳位 |
| 频率 | 视觉 ≤50 Hz；步进 IRQ 更高，主环按最新有效帧更新目标即可 |
| 滤波 | 视觉已做 `POS_ALPHA`；MCU 可选再一阶低通 |

---

## 8. 定点命令 · type = `0x12`（可选，同 13 字节）

用于 PC / 上位机 / 第二路串口源改停球点（也可用固件 `BallCtrl_SetTarget*`）。

| Offset | 字段 | 说明 |
|--------|------|------|
| 0–1 | `AA 55` | |
| 2 | type=`0x12` | |
| 3 | flags | 保留，发 0 |
| 4–5 | target_mm | i16 LE，单位 **1 mm**，相对 O |
| 6–11 | pad | 填 0 |
| 12 | checksum | `sum(bytes[2..11]) & 0xFF` |

MCU：`VisionUart` 解析后 → `BallCtrl_SetTargetMm`。

---

## 9. 非目标（本协议不做）

- 不承载 JPEG / 图像  
- 不替代 ESP-NOW `A5 5A` 链路；无线时仅把 **本 13 B 帧** 作 payload 透明转发  

---

## 10. 版本

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-07-30 | 与 `maixcam/opencv` type=0x02 对齐定稿 |
| 1.1 | 2026-07-30 | 增加 type=0x12 定点；MCU 闭环实现 |
