# line_tracker_car

MSPM0G3507 多工程仓库，同时支持：

- **VS Code + MSPM0 Toolkit + GCC**（插件生成 Makefile / 调试配置）
- **Keil uVision**（各工程 keil/*.uvprojx）
- **MaixCAM 视觉侧 Python**（apps/maixcam）

布局对齐 [vscode-mspm0-toolkit](https://github.com/Railgun19457/vscode-mspm0-toolkit)，并保留仓库内 Source/（TI SDK 源码镜像，供 Keil 直接引用）。

## 仓库结构

```text
line_tracker_car/
├── apps/
│   ├── diansai/                  # 循迹/电赛小车（主工程）
│   ├── gimbal/                   # 双步进云台
│   ├── gimbal_freertos/          # 云台 + FreeRTOS（当前以 GCC 为主）
│   ├── template/                 # 最小空工程
│   └── maixcam/                  # 视觉模块（MaixCAM，Python）
└── Source/                       # SDK 源码镜像（Keil 依赖，勿删）
```

apps/maixcam 是视觉侧 Python 应用，不是 MSPM0 工程，无需 mspm0.project.json / Keil。

每个 MSPM0 工程（diansai / gimbal / template 等）：

```text
apps/<name>/
├── mspm0.project.json            # VS Code 插件识别
├── app.mk                        # GCC 额外业务源（插件不覆盖）
├── src/                          # 业务源码
├── syscfg/                       # app.syscfg + ti_msp_dl_config.*
├── linker/                       # GCC 链接脚本等
└── keil/
    ├── <name>.uvprojx            # Keil 工程
    ├── startup_mspm0g350x_uvision.s
    └── mspm0g3507.sct
```

## Keil 使用

1. 安装 Keil + TI MSPM0G1X0X_G3X0X DFP 包
2. 打开例如：
   - apps/diansai/keil/diansai.uvprojx
   - apps/gimbal/keil/gimbal.uvprojx
   - apps/template/keil/template.uvprojx
3. 工程已指向：
   - 业务源：../src/**
   - SysConfig 生成：../syscfg/**
   - DriverLib / CMSIS：仓库根目录 Source/
4. 编译输出在 keil/Objects/、keil/Listings/（已 gitignore）

说明：gimbal_freertos 暂未恢复 Keil 工程（FreeRTOS 路径依赖 SDK）；需要时可再补。

## VS Code / GCC 使用

1. 安装 MSPM0 Toolkit，配置 gcc / sdk / sysconfig / jlink / make 路径
2. 打开本仓库根目录
3. 侧边栏选择 apps/*，点 初始化工程 或 同步配置 生成 Makefile、toolpaths.mk、.vscode/*
4. 多文件工程生成 Makefile 后：
   - CFLAGS 增加 $(EXTRA_INCLUDES)
   - SRCS 后增加 -include app.mk
5. 用扩展构建 / 烧录 / 调试

GCC 也可使用本机 SDK 路径；Keil 固定用仓库内 Source/，便于双环境同源。

## MaixCAM 视觉模块

目录：apps/maixcam/（子目录分应用）

| 子目录 | 说明 |
|--------|------|
| `collect/` | 检测数据集采样 |
| `detect_ball/` | 钢珠 YOLO 检测 |
| `red_track/` | 红目标 + IMU 串口跟踪 |
| `tools/` | PC 串口测试工具 |

说明见 apps/maixcam/README.md

## 注意

- 不要删除 Source/：Keil 工程依赖它
- MSPM0 业务源只维护一份（apps/*/src、syscfg），Keil / GCC 共用
- Makefile / toolpaths.mk / .vscode 由插件生成，不必手写提交
- 主工程 diansai：main.c 目前仍为电机开环测试
