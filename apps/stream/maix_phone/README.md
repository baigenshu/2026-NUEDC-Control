# maix_phone — 钢珠检测直播（手机热点）

路径：`apps/stream/maix_phone/`

**无 ESP32**。Maix 作为 STA 连接手机个人热点，同局域网推流。

## 功能

1. 连接手机热点 `Xiaomi x`（密码见 `main.py` 常量）
2. **Phone Web**：YOLO11 钢珠检测 → 画框 → Flask MJPEG；手机浏览器开始/结束录制（仅存手机）

## 模型部署（必须）

与 `apps/maixcam/detect_ball` 相同（`nn.YOLO11`，`CONF_TH=0.2`）：

| 默认路径 | 说明 |
|----------|------|
| `/root/models/2026H/steel_ball_v11n/yolo11n_ball.mud` | 模型描述 |
| 同目录下对应 `.cvimodel` | 权重 |

查找顺序见 `detect_util.find_model`（上述绝对路径 / 本目录 / `/root/models/` 等）。也可改 `detect_util.MODEL_MUD`。

```bash
# 设备上建议目录
# /root/models/2026H/steel_ball_v11n/yolo11n_ball.mud
# /root/models/2026H/steel_ball_v11n/*.cvimodel
```

## 使用

1. 手机打开个人热点（关闭 AP 隔离/访客网络）
2. MaixVision 打开本目录 `main.py`，或 `maixtool deploy`
3. 连上后自动进入 Web 直播：`http://<MaixIP>:8000`

热点 SSID/密码改 `main.py` 顶部：

```python
PHONE_SSID = "Xiaomi x"
PHONE_PASS = "20060313"
```

## 文件

| 文件 | 作用 |
|------|------|
| `main.py` | 连热点、入口 |
| `web_server.py` | Flask 直播 + 手机录 |
| `detect_util.py` | 模型路径、YOLO11、画框 |

## 相对旧 maix_esp32

- 已删除 ESP SoftAP（`video_receive`）
- 组网：手机热点，不再依赖 `MaixCam-ESP`
