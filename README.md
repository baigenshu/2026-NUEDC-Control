# 2026-NUEDC-Control

2026 电赛控制端多工程仓库。主控 **MSPM0G3507**，并含视觉 / 图传 / 无线桥接等配套工程。

同时支持：

- **VS Code + MSPM0 Toolkit + GCC**（插件生成 Makefile / 调试配置）
- **Keil uVision**（部分工程 `keil/*.uvprojx`）
- **MaixCAM / OpenMV / ESP8266 / ESP32** 侧脚本与固件

布局对齐 [vscode-mspm0-toolkit](https://github.com/Railgun19457/vscode-mspm0-toolkit)，仓库内 `Source/` 为 TI SDK 源码镜像（Keil 直接引用）。

## 仓库结构

```text
2026-NUEDC-Control/
├── apps/
│   ├── car/                      # 主工程：四轮差速 + 8 路灰度巡线（H 题第 2 项）
│   ├── odometry/                 # 编码器 + IMU601 里程计融合联调
│   ├── balance/                  # 步进丝杆驱动（TMC 单轴，业务待接）
│   ├── NRF24L01/                 # nRF24L01 无线收发联调
│   ├── test/                     # UART 全双工回显等板级测试
│   ├── template/                 # MSPM0 最小空工程
│   ├── maixcam/                  # MaixCAM 视觉（Python）
│   ├── stream/                   # 无线图传
│   │   ├── openmv_esp32/         # OpenMV H7 + ESP32 SoftAP 推流
│   │   └── maix_phone/           # Maix 连手机热点 + YOLO 直播
│   └── esp8266_ti/               # ESP-01 ESP-NOW ↔ UART 透明桥
└── Source/                       # SDK 源码镜像（Keil 依赖，勿删）
```

非 MSPM0 工程（`maixcam` / `stream` / `esp8266_ti`）无需 `mspm0.project.json`。

## 工程一览

| 目录 | 平台 | 说明 |
|------|------|------|
| `apps/car` | MSPM0 | **主工程**：四轮差速、8 路灰度、OLED；B21 启动顺时针巡线一圈并显示总时间。见 `apps/car/docs/` |
| `apps/odometry` | MSPM0 | 双编码器 + IMU601 @100Hz 融合，串口打印位姿 |
| `apps/balance` | MSPM0 | TMC 步进（STEP/DIR/EN + 梯形加减速），入口仅初始化 |
| `apps/NRF24L01` | MSPM0 | nRF24L01 SPI 接收 demo，UART 打印 |
| `apps/test` | MSPM0 | DEBUG UART 回显 / 心跳 |
| `apps/template` | MSPM0 | 空模板 |
| `apps/maixcam` | MaixCAM | 采样 / YOLO 钢珠 / OpenCV 控球 / 红目标跟踪。见 `apps/maixcam/README.md` |
| `apps/stream/openmv_esp32` | OpenMV+ESP32 | QVGA JPEG 无线图传 + 手机录制。见子目录 README |
| `apps/stream/maix_phone` | MaixCAM | 连手机热点，YOLO11 钢珠检测 Web MJPEG |
| `apps/esp8266_ti` | ESP8266 | PlatformIO：`sender` / `receiver`，MCU UART ↔ ESP-NOW |

### MSPM0 子工程布局

```text
apps/<name>/
├── mspm0.project.json            # VS Code 插件识别（本地生成，gitignore）
├── src/                          # 业务源码
│   ├── main.c
│   ├── Hardware/                 # 驱动（电机 / 编码器 / 灰度 / OLED …）
│   └── Function/                 # 业务（底盘 / 巡线 / 任务状态机 …）
├── syscfg/                       # app.syscfg + ti_msp_dl_config.*
├── linker/                       # GCC 链接脚本（插件生成）
└── keil/                         # 可选：*.uvprojx + 启动/分散加载
```

有文档的工程：

- `apps/car/docs/`：`plan.md` 架构 · `pins.md` 引脚 · `api.md` API
- `apps/maixcam/README.md`、`apps/stream/*/README.md`

## car（主工程）

H 题第 2 项：A 点 B21 启动 → 顺时针巡线一圈 → 停回 A → OLED 显示总时间。

```text
B21 ──► LapTask 状态机
Gray ──► LineTrack PD ──► Chassis_Arcade ──► Motor A/B/C/D
Encoder ──► Chassis odom ──► LapTask 里程锁
main ──► OLED：状态 / 时间 / 灰度 / 里程·误差
```

轮位：前 B(左前) C(右前) · 后 A(左后) D(右后)。配置集中在 `chassis_cfg.h`。

## VS Code / GCC

1. 安装 [MSPM0 Toolkit](https://github.com/Railgun19457/vscode-mspm0-toolkit)，配置 gcc / sdk / sysconfig / jlink / make
2. 打开本仓库根目录
3. 侧边栏选择 `apps/car` 等，**初始化工程** 或 **同步配置**（生成 Makefile、`toolpaths.mk`、`.vscode/*`）
4. 用扩展构建 / 烧录 / 调试

`Makefile` / `toolpaths.mk` / `.vscode` / `build/` 由插件生成，已 gitignore，不必手写提交。

## Keil

1. 安装 Keil + TI MSPM0G1X0X_G3X0X DFP
2. 打开例如：
   - `apps/template/keil/template.uvprojx`
   - `apps/odometry/keil/template.uvprojx`
   - `apps/NRF24L01/keil/template.uvprojx`
3. 工程指向 `../src/**`、`../syscfg/**`、仓库根 `Source/`
4. 输出在 `keil/Objects/`、`keil/Listings/`（已 gitignore）

`car` / `test` / `balance` 当前以 **GCC + Toolkit** 为主，暂无 Keil 工程。

## 视觉与图传

| 路径 | 作用 |
|------|------|
| `maixcam/collect/` | 检测数据集采样 |
| `maixcam/detect_ball/` | 钢珠 YOLO11 检测 → 串口 |
| `maixcam/opencv/` | 凹槽钢珠 OpenCV + UART 位置（控球） |
| `maixcam/red_track/` | 红目标 + IMU → UART（云台） |
| `maixcam/tools/` | PC 串口发测试帧 |
| `stream/openmv_esp32/` | OpenMV → ESP32 SoftAP → 手机浏览器 |
| `stream/maix_phone/` | Maix STA 连手机热点 + 检测直播 |

YOLO 模型默认：`/root/models/2026H/steel_ball_v11n/yolo11n_ball.mud`（及对应 `.cvimodel`）。

## ESP8266 无线桥

`apps/esp8266_ti`（PlatformIO，板型 `esp01_1m`）：

```bash
cd apps/esp8266_ti
pio run -e sender    # 车端：MCU UART ↔ ESP-NOW
pio run -e receiver  # 对端：ESP-NOW ↔ UART
```

默认广播 peer；正式联调请把 `include/config.h` 中 `PEER_MAC` 改为对端 STA MAC。

## 注意

- **不要删除 `Source/`**：Keil 依赖
- MSPM0 业务源只维护一份（`apps/*/src`、`syscfg`），Keil / GCC 共用
- 插件生成文件已 gitignore；提交业务源与 `syscfg` 即可
- 主工程当前为 **`apps/car`**（巡线一圈任务），非旧版 diansai / gimbal
