# video_send — 钢珠检测直播 + 手机录制

路径：`apps/stream/maix_esp32/video_send/`

## 功能

1. 连 **MaixCam-ESP** 热点（由同目录 `../video_receive` 提供）  
2. **Phone Web**：YOLOv8 检测钢珠 → 画框 → 网页直播  
3. 手机 **开始/结束录制** → 只保存在手机（含检测框）  
4. ESP 只当热点（不上屏、不拉流）  

## 模型部署（必须）

与 `apps/maixcam/detect_ball` 相同：

| 文件 | 说明 |
|------|------|
| `steel_ball.mud` | 模型描述 |
| `steel_ball_int8.cvimodel` | 权重（与 mud 同目录） |

**查找顺序**（`detect_util.find_model`）：

1. 本目录 `video_send/steel_ball.mud`  
2. `video_send/models/steel_ball.mud`  
3. 仓库 `apps/maixcam/detect_ball/steel_ball.mud`（开发机相对路径）  
4. 设备 `/root/models/steel_ball.mud`  
5. `/root/detect_ball/`、`/root/video_send/`、`/maixapp/apps/video_send/`  

推荐（在仓库根或本机）：

```bash
# 拷到本应用目录再 deploy
cp apps/maixcam/detect_ball/steel_ball.mud apps/stream/maix_esp32/video_send/
cp apps/maixcam/detect_ball/steel_ball_int8.cvimodel apps/stream/maix_esp32/video_send/

# 或设备上
mkdir -p /root/models
# scp 上述两文件到 /root/models/
```

## 使用

```bash
# ESP
cd apps/stream/maix_esp32/video_receive
pio run -t upload

# Maix：MaixVision 打开本目录 main.py，或 maixtool deploy
```

1. 先开 ESP 热点  
2. 手机连 `MaixCam-ESP` / `grx060313`  
3. 跑本应用 → **Phone Web**  
4. 浏览器 `http://MaixIP:8000`  

## 文件

| 文件 | 作用 |
|------|------|
| `main.py` | WiFi、模式、入口 |
| `web_server.py` | Flask 直播 + 手机录 |
| `detect_util.py` | 模型路径与画框 |
