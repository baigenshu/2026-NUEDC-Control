# video_receive — ESP32 无线预览 + SD 录制

ESP32 开 **SoftAP**，从 MaixCAM-Pro（`apps/video_send`）拉 **HTTP JPEG/MJPEG** 流：

1. 解码显示在 **ST7789**
2. 同时把完整 JPEG 帧写入 SD：**AVI + MJPEG**（**不是 MP4**）

## 架构

```text
MaixCAM (video_send)
  → HTTP /stream (JPEG 帧)
ESP32 (video_receive)
  → SoftAP 始终开启
  → 读 Maix 首页 data-mode：
       tft → 拉 /stream → TFT + 可选 SD AVI
       web → 不拉流（AP only，把带宽给手机网页）
```

## 与 video_send 模式配合

| Maix 选择 | ESP 行为 | 手机 |
|-----------|----------|------|
| TFT Screen | 拉流上屏 / 可录卡 | 可顺带打开网页（抢带宽） |
| Phone Web | **不拉流**，屏显 AP only | 网页更流畅（推荐） |


## 录制说明

| 项 | 值 |
|----|-----|
| 格式 | **`.avi` + MJPEG**（VLC 可播） |
| 路径 | `/REC/VID_0001.avi` … |
| 分辨率 | 与推流一致，默认 160×120 |
| 自动录 | 有流且 SD 就绪即开始（`AUTO_RECORD=1`） |
| 分卷 | 约 `MAX_FRAMES_PER_FILE`（默认 4500）帧后新文件 |
| 断流 | 自动 `end()` 回写 AVI 头，避免半截文件 |

**不是 MP4。** 普通 ESP32 无法实时 H.264。电脑可用 VLC 打开 AVI；需要 MP4 时用 FFmpeg 转换。

## 默认约定

| 项 | 值 |
|----|-----|
| SoftAP | `MaixCam-ESP` / `grx060313` |
| 拉流 | `http://192.168.4.2:8000/stream` |
| 显示 | 160×120 居中 |
| SD_CS | **GPIO 13**（可改） |

## 硬件接线

### TFT（现有）

| 信号 | ESP32 |
|------|-------|
| SCLK | 18 |
| MOSI | 23 |
| MISO | 19 |
| TFT_CS | 5 |
| DC | 2 |
| RST | 4 |
| BL | 15 |

### SD（与 TFT 共 SPI）

红板 14 针模组有独立 **SD_CS**（厂商 Arduino 例程逻辑脚 10；接到 ESP32 空闲脚）：

| 信号 | ESP32 |
|------|-------|
| SCLK / MOSI / MISO | 与 TFT 并联 18 / 23 / 19 |
| **SD_CS** | **13**（`#define SD_CS`，按实际改） |
| 3V3 / GND | 共电共地 |

访问 SD 时拉高 TFT_CS；访问屏时拉高 SD_CS。

## 编译烧录

```bash
cd apps/video_receive
pio run -t upload
pio device monitor -b 115200
```

串口应见：`SD OK`、`AVI rec start: /REC/VID_xxxx.avi`、`FPS: ... recF:...`

## 联调

1. 插 **FAT32** SD 卡，接好 SD_CS  
2. 烧录 ESP32，确认 `SD OK`（失败则预览仍可用）  
3. 跑 `video_send` 选 **ESP32 TFT**  
4. 有流后自动 REC；拔卡前尽量断流/断电前等几秒以便收尾  
5. 电脑打开 `/REC/*.avi`（VLC）

## 排障

| 现象 | 处理 |
|------|------|
| SD FAIL | 查 SD_CS 接线；卡格式 FAT32；降 `SD_SPI_HZ` |
| 花屏 | SPI 冲突：确认 CS 互斥；线尽量短 |
| 录制卡顿 | 正常；可加大 `RECORD` 间隔或降推流帧率 |
| AVI 打不开 | 是否异常断电；看是否有 `AVI rec stop` 日志 |
| 一直 Waiting STA | SSID/密码；Maix 是否连上 |

## 文件

| 文件 | 作用 |
|------|------|
| `src/main.cpp` | SoftAP、拉流、预览、录制集成 |
| `src/avi_mjpeg.*` | AVI MJPEG 写入 |
| `platformio.ini` | 依赖 |

## 参考

- 模组资料：`电赛备赛/模块-14针红板带触摸模块`（SD_CS 独立）
- 发送端：`apps/video_send`
