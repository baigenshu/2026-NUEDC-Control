# video_send — MaixCAM-Pro 视频推流 / 网页录制

固定连 **MaixCam-ESP**，再选模式：

| 模式 | 作用 |
|------|------|
| **TFT Screen** | JpegStreamer；ESP 拉流上屏/SD |
| **Phone Web** | Flask 网页：上半预览 + 录制/列表/回放（文件在 **Maix**） |

## Phone Web（阶段 A）

打开 `http://<MaixIP>:8000`：

- **上半**：实时 MJPEG  
- **开始录制 / 结束录制**：写入 `/root/recordings/VID_xxxx.avi`（AVI+MJPEG，非 MP4）  
- **录像列表 + 播放**：在线 MJPEG 回放  

ESP 读到页面 `data-mode="web"` 后 **不拉流**（只当热点），网页更流畅。

### API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/` | 主页 |
| GET | `/stream` | 直播 |
| POST | `/api/rec/start` | 开始录制 |
| POST | `/api/rec/stop` | 结束录制 |
| GET | `/api/rec/list` | 列表 JSON |
| GET | `/api/rec/status` | 状态 |
| GET | `/play?name=` | 回放页 |
| GET | `/api/rec/play?name=` | 回放 MJPEG |
| GET | `/rec/<name>` | 下载 |

## 使用

1. 运行 `video_receive`（SoftAP）  
2. 手机连 `MaixCam-ESP` / `grx060313`  
3. 跑本应用 → 选 **Phone Web**  
4. 浏览器打开屏上 IP  
5. 点开始/结束录制，列表里点「播放」  

## 文件

| 文件 | 作用 |
|------|------|
| `main.py` | WiFi、模式选择、TFT / Web 入口 |
| `web_server.py` | Flask UI + API |
| `avi_mjpeg.py` | AVI 写入与回放解析 |

## 注意

- 录像在 Maix `/root/recordings`，不是 ESP SD  
- 格式 AVI+MJPEG；电脑可用 VLC 打开下载的文件  
- 退出仍避免强制 `stop`/`del` 防 SIGSEGV  
