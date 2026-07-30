# Ball OpenCV → balance + MJPEG

与 `apps/balance` 视觉协议对接（`docs/vision_proto.md`）。  
启动扫 WiFi 二维码连热点 → 本机屏 HUD + 手机 MJPEG 双显。

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
| `0x02` | 球位 pos mm + conf + cx/cy | ≥20 ms |
| `0x12` | 停球定点（屏 **Set** 拖动 / Reset / 上电同步） | 变更时 |

- `conf < 30` 强制 `found=0`（与 MCU `BALL_CONF_MIN` 一致）
- 退出时发一帧 lost，主控倾角回水平

## WiFi 扫码（每次启动必扫）

**无硬编码 SSID/密码。** 启动后先进入扫码界面，扫到可联网二维码并连接成功后，才开启检测与推流。

1. 手机开热点  
2. 系统「分享 WiFi 二维码」，或 [maixhub.com/wifi](https://maixhub.com/wifi) 生成  
3. 对准 Maix 摄像头  
4. 屏显 `WiFi OK` + IP 后进入主界面  

支持 payload：

```text
WIFI:T:WPA;S:热点名;P:密码;;
WIFI:T:nopass;S:开放热点;;
ssid|password
```

连接失败会提示 `Fail, rescan` 并继续扫。

| 常量 | 默认 |
|------|------|
| `WIFI_CONNECT_TIMEOUT` | 60 s |

## 屏显 / 触摸

- 黄线：O 点  
- 粉线：当前 setpoint（Set 模式可拖）  
- **Exit | Set/Done | Reset**

## 手机双显（MJPEG）

1. 扫码连上后，串口打印：`[ball] MJPEG: http://<MaixIP>:8000/stream`  
2. 手机浏览器 / App 打开该 URL（`/` 同流）  
3. 画面带 HUD（ROI / 球点 / SP / FPS），与本机屏一致  
4. **SP 仍在 Maix 触摸屏设置**，手机默认只监视  

| 常量 | 默认 | 说明 |
|------|------|------|
| `ENABLE_MJPEG` | True | 关推流可关 |
| `HTTP_PORT` | 8000 | |
| `JPEG_QUALITY` | 45 | |
| `MJPEG_EVERY_N` | 1 | 隔帧编码减负 |

## 运行

MaixVision 打开本目录 `main.py` 运行，或按 `app.yaml` 安装。

ROI 改文件头 `ROI_*`；杆长改 `BAR_LEN_MM`。
