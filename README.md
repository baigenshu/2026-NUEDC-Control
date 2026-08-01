# 2026-NUEDC-Control

2026 年全国大学生电子设计竞赛（NUEDC）控制端多工程仓库。主控芯片 **TI MSPM0G3507**（ARM Cortex-M0+ @ 80 MHz），并含视觉、图传、无线桥接等配套工程。

## 开发环境

| 工具链 | 用途 |
|--------|------|
| **VS Code + MSPM0 Toolkit**（GCC） | 主力开发环境，插件生成 Makefile / 调试配置 |
| **Keil uVision** | 部分工程备用，需安装 TI MSPM0G1X0X_G3X0X DFP |
| **PlatformIO** | ESP8266 工程 |
| **MaixVision** | MaixCAM Python 视觉应用 |

仓库布局对齐 [vscode-mspm0-toolkit](https://github.com/Railgun19457/vscode-mspm0-toolkit)，`Source/` 为 TI SDK 源码镜像（Keil 直接引用）。

## 仓库结构

```text
2026-NUEDC-Control/
├── apps/
│   ├── line_track/              # 🏁 主工程：四路红外巡线 · 四轮独立速度 PID 闭环
│   ├── balance/                 # 🎯 平衡摆杆：视觉球位闭环停球（曲柄连杆）
│   ├── maixcam/                 # 👁  MaixCAM 视觉（Python）
│   │   ├── opencv/              #   OpenCV 凹槽钢珠检测 + UART 位置（控球主环）
│   │   ├── collect/             #   检测数据集采样工具
│   │   └── tools/               #   PC 串口测试帧发送
│   ├── stream/                  # 📡 无线图传
│   │   ├── openmv_esp32/        #   OpenMV H7 + ESP32 SoftAP JPEG 推流
│   │   ├── maix_phone/          #   MaixCAM MJPEG 推流（手机拉流）
│   │   └── MaixStream/          #   推流组件安装包
│   ├── esp8266_ti/              # 📶 ESP-01 ESP-NOW ↔ UART 透明桥
│   └── template/                # 📋 MSPM0 最小空工程模板
└── Source/                      # TI SDK 源码镜像（Keil 依赖，勿删）
    ├── third_party/             # CMSIS / mcuboot
    └── ti/                      # driverlib · drivers · motor_control · boards …
```

MSPM0 工程（`line_track` / `balance` / `template`）统一遵循以下子工程布局：

```text
apps/<name>/
├── src/                         # 业务源码
│   ├── main.c                   # 入口
│   ├── Hardware/                # 外设驱动（Inc + Src）
│   └── Function/                # 控制算法与业务逻辑（Inc + Src）
├── syscfg/                      # SysConfig 生成的 app.syscfg + ti_msp_dl_config.*
├── linker/                      # GCC 链接脚本（插件生成，已 gitignore）
├── keil/                        # Keil 工程文件（可选，部分工程）
└── build/                       # GCC 构建产物（已 gitignore）
```

## 工程一览

| 目录 | 平台 | 状态 | 说明 |
|------|------|------|------|
| `apps/line_track` | MSPM0 | 🏁 **主工程** | 四路红外循迹 + 四轮独立速度 PID 闭环 |
| `apps/balance` | MSPM0 | 🎯 **主工程** | TMC2209 步进 + 曲柄连杆摆杆停球 |
| `apps/maixcam` | MaixCAM | 👁 配套 | OpenCV 凹槽钢珠检测 → UART 位置 + MJPEG |
| `apps/stream/openmv_esp32` | OpenMV+ESP32 | 📡 配套 | QVGA JPEG 无线图传 + 手机录制 |
| `apps/stream/maix_phone` | MaixCAM | 📡 配套 | 纯 MJPEG 推流 |
| `apps/esp8266_ti` | ESP8266 | 📶 配套 | ESP-NOW ↔ UART 透明桥（双固件） |
| `apps/template` | MSPM0 | 📋 模板 | 最小空工程 |

---

## 🏁 line_track — 四路红外巡线

四路红外传感器（TCRT5000 类）**四轮独立编码器闭环速度 PID**，支持多档速度模式与定时停车。

### 硬件配置

| 组件 | 型号 / 引脚 | 说明 |
|------|------------|------|
| MCU | MSPM0G3507 @ 80 MHz | |
| 红外传感器 | G1–G4：PB19, PB17, PA16, PA14 | 左→右 p1..p4，高有效（黑线=1） |
| 电机驱动 | TB6612 类 H 桥 ×4 | PWM 周期 4000，最大占空比 3800 |
| 电机 | MG310 霍尔编码减速电机 | CPR=13，减速比 30，四倍频 |
| 编码器 | 四路 GPIO 双边沿四倍频 | 轮位：A 左后 · B 左前 · C 右前 · D 右后 |
| OLED | I²C OLED | 实时显示速度、红外状态、编码器、计时 |
| 按键 | PA17（RUN）· PA15（SPD） | 启停巡线 / 切换速度档 |
| IMU | 汇电籽-601（UART1 DMA） | 姿态传感器（已集成，待接入控制） |

### 控制架构

```text
按键 RUN ──► 使能/停止
按键 SPD ──► 速度档 M0(停) → M1(三档持续) → M2(三档15s停) → M3(二档7s停)

传感器 IR1..4 ──► 加权线位置 ──► 位置式 PID ──► 左右差速目标
                                                      │
                MG310 编码器反馈 ◄── 四路独立增量式 PI ◄──┘
                                                      │
                                                      ▼
                                                PWM → TB6612 → 四轮
```

### 传感器几何与参数

传感器非等距布局：p1–p2 = 32.9 mm · p2–p3 = 14.5 mm · p3–p4 = 32.9 mm，线宽约 18 mm。
加权策略：外侧 ±2.5 主导回救，内侧 ±0.8 按弧半径标定（线在 p2 时比例项恰好抱住 R = 0.5 m 圆弧，不依赖积分）。

丢线补偿：线落入传感器间隙时，按最后已知方向瞬间施加大幅修正（`LF_LOST_ERR = 5.0`），内轮可停转/反转实现 pivot 急转回救。

所有 PID 参数集中在 `src/Function/Inc/line_follow_cfg.h`，需上机精调。

### 速度档位

| 档位 | 基速 (pulses/10ms) | 超时 | 说明 |
|------|---------------------|------|------|
| M0 (STOP) | 0 | — | 停止 |
| M1 | 20（GEAR_3） | — | 三档持续 |
| M2 | 20（GEAR_3） | 15 s | 三档自动停 |
| M3 | 15（GEAR_2） | 7 s | 二档自动停 |

### OLED 显示

```
L1  SPD:+xxxx       速度设定值
L2  IR:xxxx E:±xx   红外位掩码 + 线偏离误差
L3  A:+xxx B:+xxx   编码器增量 L
L4  TRK/OFF Mx T:xxx.xx  运行状态 + 档位 + 圈时 (s)
```

### 串口遥测

UART DEBUG 每 1 s 输出心跳，含使能状态、模式、速度、红外掩码、位置误差、四轮编码器增量。按键事件和自动停车时有对应日志。

---

## 🎯 balance — 视觉球位闭环停球

**合页 + 凹槽摆杆 + 曲柄连杆** 机构，通过 MaixCAM 视觉反馈实现钢珠在凹槽上的定点停球。

### 硬件配置

| 组件 | 引脚 / 接口 | 说明 |
|------|------------|------|
| 步进电机 | STEP=PA12 · DIR=PA13 · EN=PA14 | TIMG7 10 µs 节拍 bit-bang |
| 驱动芯片 | TMC2209（UART） | 电流 + 微步配置 |
| 视觉输入 | UART0（接 MaixCAM） | 13B 小端帧协议 |
| 机械结构 | 合页 + 凹槽摆杆 + 曲柄连杆 | 电机轴角 → 摆杆倾角 → 钢珠位置 |

### 控制链路

```text
MaixCAM ──► UART 0x02/0x12 帧 ──► VisionUart 解析 ──► 球位滤波 + 速度估计
                                                           │
                                                           ▼
                                          位置 PD + 制动/防振荡策略
                                                           │
                                                           ▼
                                             TMC2209 → 步进电机 → 曲柄倾角
```

### 视觉协议

- **0x02 球位帧**：`AA 55 | 02 | flags | pos_mm i16 | cx i16 | cy i16 | conf | mode | csum`
- **0x12 定点帧**：`AA 55 | 12 | 00 | target_mm i16 | pad×6 | csum`
- pos / target 单位为整 mm，小端序

### 控制策略

多级 PD 增益（远/近/制动/微动），含速度前馈制动、bias 自适应学习（仅低速区）、丢球超时回中。所有参数集中在 `src/Function/Inc/ball_ctrl_cfg.h`。

---

## 👁 maixcam — MaixCAM 视觉

每个子目录是一个独立 MaixCAM 应用，用 MaixVision 打开对应 `main.py` 运行。

| 目录 | 功能 | 依赖 |
|------|------|------|
| `opencv/` | **控球主环**：扫码连热点 → OpenCV 凹槽钢珠检测 → `0x02`/`0x12` UART 帧 + 屏显 HUD + 手机 MJPEG | OpenCV + UART + WiFi QR |
| `collect/` | 检测数据集采样 | — |
| `tools/` | PC 串口测试工具（`send_ball_frame.py` / `send_ball_setpoint.py` / `send_track_frame.py`） | pyserial |

### opencv（控球主环）

白杆凹槽 + 暗色钢珠检测，支持：
- 双模式检测（亮/暗自适应） + 投影法回退
- 一维匀速跟踪滤波器（`POS_ALPHA` / `TRACK_VEL_ALPHA`），抗跳变、短时 hold
- 跟踪锁定后局部搜索（`SEARCH_HALF_W`），加速
- 黑白对比评分 + 阴影过滤
- 板载补光 LED（B3）
- 屏显 HUD（球位、定点、红外掩码、FPS、WiFi 状态）
- 触摸交互：Menu → WiFi 重连 / Set 定点拖拽 / Reset 归零
- MJPEG 推流：`http://<IP>:8000/stream`

**与 balance 对接**：Maix A16 TX → balance PA31（MaixCAM2 则为 A21/UART4）

**YOLO 模型**（历史，`detect_ball/`）：`/root/models/2026H/steel_ball_v11n/yolo11n_ball.mud`

---

## 📡 stream — 无线图传

| 子目录 | 平台 | 说明 |
|--------|------|------|
| `openmv_esp32/` | OpenMV H7 + ESP32 | QVGA 320×240 JPEG 推流（SoftAP），手机浏览器 `http://192.168.4.1/` |
| `maix_phone/` | MaixCAM | 纯 MJPEG 推流 `http://<MaixIP>:8000/stream`（无检测，备用） |

### openmv_esp32

- OpenMV 端：QVGA JPEG quality 35 @ 100 ms（`openmv/main.py`）
- ESP32 端：FreeRTOS 收串口 + 多客户端推流（PlatformIO，`esp32/`）
- 接线：OpenMV P4(TX)→ESP32 GPIO16 · GND–GND
- 稳画机制：发送缓冲写保护 + clients 互斥锁

---

## 📶 esp8266_ti — ESP-NOW 无线桥

ESP-01 双固件（PlatformIO，板型 `esp01_1m`），实现 MCU UART ↔ ESP-NOW 双向透明桥。

```bash
cd apps/esp8266_ti
pio run -e sender    # 车端：MCU UART ↔ ESP-NOW
pio run -e receiver  # 对端：ESP-NOW ↔ PC USB UART
```

| 环境 | 定义 | 说明 |
|------|------|------|
| `sender` | `-DNODE_SENDER` | 车端：静默 UART，无 banner |
| `receiver` | `-DNODE_RECEIVER` | PC 端：打印 MAC 和就绪信息 |

默认广播 peer；正式联调时在 `include/config.h` 中将 `PEER_MAC` 改为对端 STA MAC。

---

## 📋 template — MSPM0 最小工程

仅含 `SYSCFG_DL_init()` + 空循环的 MSPM0G3507 空模板，可用作新工程起点。

---

## VS Code / GCC 开发

1. 安装 [MSPM0 Toolkit](https://github.com/Railgun19457/vscode-mspm0-toolkit)，配置 GCC / SDK / SysConfig / JLink / Make
2. 打开仓库根目录
3. 侧边栏选择目标工程（如 `apps/line_track`），**初始化工程** 或 **同步配置**
4. 使用扩展构建 / 烧录 / 调试

插件生成物（`Makefile` / `toolpaths.mk` / `.vscode` / `build/` / `linker/` / `mspm0.project.json`）均已 gitignore，无需手动维护。仅需提交 `src/` 与 `syscfg/`。

## Keil 开发

1. 安装 Keil + TI MSPM0G1X0X_G3X0X DFP
2. 打开对应工程 `.uvprojx`（位于 `apps/<name>/keil/`）
3. 工程已配置指向 `../src/**`、`../syscfg/**` 及仓库根 `Source/`
4. 输出目录 `keil/Objects/` / `keil/Listings/` 已 gitignore

## MaixCAM 部署

- 用 MaixVision 打开对应 `main.py` 运行，或按目录打包安装
- SSH：`root@<ip>`，密码 `root`
- 模型文件较大，需将 `.mud` / `.cvimodel` 一并上传至 `/root/models/`

## 注意事项

- **不要删除 `Source/`**：Keil 工程依赖此 SDK 镜像
- MSPM0 业务源码只维护一份（`apps/*/src` + `syscfg`），Keil / GCC 共用
- 插件生成文件已 gitignore，仅提交业务源与 `syscfg` 即可
- 非 MSPM0 工程（`maixcam` / `stream` / `esp8266_ti`）无需 `mspm0.project.json`
