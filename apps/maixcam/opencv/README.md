# Ball OpenCV → balance + MJPEG

与 `apps/balance` 视觉协议对接（`docs/vision_proto.md`）。  
启动复用已有 WiFi，或扫码连接（可 Skip）；有网才开本机屏 HUD + 手机 MJPEG。

## 接线（3.3V 共地）

| MaixCAM | balance |
|---------|---------|
| A16 TX（Cam2: A21） | **PA31** UART0 RX |
| A17 RX 可选 | PA28 TX |
| GND | GND |

波特率 **115200 8N1**。

## 发送

| type | 内容 | 周期 |
|------|------|------|
| `0x02` | 球位 pos 1mm + conf + cx/cy | 每检测帧（`TX_MIN_MS=0`） |
| `0x13` | 控制 action：`0` Reset、`1` Start、`2` ±5 预设 | 触摸时，连发 3 帧 |

- `conf < 30` 强制 `found=0`（与 MCU `BALL_CONF_MIN` 一致）
- 退出时发一帧 lost，主控倾角回水平
- 本地跟踪：`update_track` 一维匀速预测，抑制 ROI 跳变；短时丢球/跳变可 hold

## WiFi 启动流程

1. **已连接** → 直接复用现有 IP，进入主界面  
2. **未连接** → 扫码界面（右下 **Skip** 可跳过）  
3. **Skip / 无网** → 仍可检测 + UART，**禁用 MJPEG 图传**  
4. 主页 **Menu → WiFi** 可手动重连（同样可 Skip）

无硬编码 SSID/密码。支持 payload：

```text
WIFI:T:WPA;S:热点名;P:密码;;
WIFI:T:nopass;S:开放热点;;
ssid|password
```

| 常量 | 默认 |
|------|------|
| `WIFI_CONNECT_TIMEOUT` | 60 s |

## 屏显 / 触摸

- 黄线：O 点
- 顶部：球位置和当前控制模式
- 右上：FPS + `Live`（手机在看）/ `WiFi`（有网）/ `NoNet`  
- 底栏：**Menu** | **Start** | **±5** | **Reset**
  - Menu 展开：上方 **WiFi** / **Exit**  
  - 点空白处可收起 Menu
  - Start：设置当前摆臂为机械 0 点，并在视觉 O 点启动普通平衡
  - ±5：仅在 O 点平衡稳定后启动固定四段摆臂轨迹 `0 → +50 → -50 mm`；正向推送已按实测提高至专用更大幅度，正向运动中必须进入 `+50 ±10 mm`，最终在 `-50 mm` 稳定停住；5 s 内未完成则标记超时但仍收敛到 `-50 mm`
  - Reset：停止闭环、释放电机，并恢复默认 O 点目标

## 手机双显（MJPEG）

仅在有有效 WiFi 时启用：

1. 串口打印：`[ball] MJPEG: http://<MaixIP>:8000/stream`  
2. 手机浏览器打开该 URL（`/` 同流）  
3. 画面带 HUD，与本机屏一致  
4. 控制按钮仅在 Maix 触摸屏操作，手机默认只监视

| 常量 | 默认 | 说明 |
|------|------|------|
| `ENABLE_MJPEG` | True | 总开关；无 WiFi 时仍不推流 |
| `HTTP_PORT` | 8000 | |
| `JPEG_QUALITY` | 45 | 异步编码，不挡检测 |
| `MJPEG_EVERY_N` | 2 | 有客户端时隔帧编码 |
| `DISP_EVERY_N` | 3 | 本机 LCD 隔帧；无刷新帧跳过 HUD |
| `SEARCH_HALF_W` | 56 | 跟踪锁定后局部搜索半宽 (px) |
| `SKIP_BLUR` | True | 细 ROI 跳过 blur |

## 跟踪参数

| 常量 | 默认 | 说明 |
|------|------|------|
| `TRACK_VEL_ALPHA` | 0.35 | 速度估计平滑 |
| `TRACK_MAX_JUMP_MM` | 28.0 | 相对预测的最大跳变 |
| `TRACK_HOLD_FRAMES` | 1 | 短时丢球/跳变 hold 帧数 |

## 运行

MaixVision 打开本目录 `main.py` 运行，或按 `app.yaml` 安装。

ROI 改文件头 `ROI_*`；杆长改 `BAR_LEN_MM`。
