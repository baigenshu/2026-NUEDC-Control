# Keil + GCC 双环境说明

本仓库三个子工程（`template` / `gimbal` / `diansai`）同时支持：

| 环境 | 怎么用 |
|------|--------|
| **Keil** | 打开 `*/keil/*.uvprojx`，与原来完全一样 |
| **GCC + VS Code** | 打开对应**子工程目录**（有 `Makefile` 的那一层），`Ctrl+Shift+B` 或 `mingw32-make` |

## 路径配置（只改一处）

编辑仓库根目录 `toolpaths.mk`，然后在每个子工程执行：

```bat
mingw32-make apply-paths
```

会更新该工程的 `.vscode/*`（IntelliSense / 任务 / 调试 / 终端 PATH）。

## 常用命令（在子工程目录）

```bat
mingw32-make -j8      :: 编译 → build/app.out / .hex
mingw32-make flash    :: J-Link 烧录
mingw32-make clean
mingw32-make syscfg   :: 按 empty.syscfg 重新生成 ti_msp_dl_config.*
mingw32-make syscfg-gui
```

## 新增源文件

- **Keil**：在 uvprojx 里添加
- **GCC**：在该工程 `Makefile` 的 `SRCS` / `INCLUDES` 里添加

两边列表需各自维护，源码目录不搬动。

## 目录约定

- 业务源码、`empty.syscfg`、`ti_msp_dl_config.*` 仍在工程根
- GCC 启动文件与链接脚本在 `common/`
- GCC 产物在 `build/`（已 gitignore）
- Keil 仍用 `keil/startup_*_uvision.s` 与 `Objects/`
