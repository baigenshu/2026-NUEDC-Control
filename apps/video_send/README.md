# video_send — 全力直播 + 手机录制

## 当前策略

| 项目 | 说明 |
|------|------|
| WiFi | 固定连 **MaixCam-ESP** |
| 直播 | Maix **只推流**，不写本机录像 |
| 画质 | Web 默认 **320×240**，quality≈50 |
| 录制 | **仅手机浏览器** MediaRecorder → 下载到手机 |
| ESP | 只当热点（屏/拉流已弃用） |

## 使用

1. 烧录并运行 **video_receive**（SoftAP only）  
2. 手机连 `MaixCam-ESP` / `grx060313`（可无 Internet）  
3. Maix 运行本应用 → 选 **Phone Web**  
4. 浏览器打开屏上 `http://x.x.x.x:8000`  
5. 上半看直播；下半 **开始/结束录制** → 自动下载到手机（多为 WebM）  
6. 建议 **Chrome**；部分浏览器不支持 MediaRecorder  

## 模式说明

| 选项 | 状态 |
|------|------|
| Phone Web | **主用**：直播 + 手机录 |
| TFT (off) | 保留入口，ESP 屏方案已停用 |

## 若直播仍卡

在 `main.py` 的 Web `MODES` 中降低：

- `cam_w/h` → 240×180  
- `jpeg_quality` → 40  

## 文件

| 文件 | 作用 |
|------|------|
| `main.py` | WiFi、模式、入口 |
| `web_server.py` | Flask 直播页 + 手机录制 UI |
| `avi_mjpeg.py` | 旧本机 AVI（Web 已不用，可忽略） |

## 注意

- 不再使用 `/root/recordings` 网页录制  
- 手机录的是网页画面，画质上限=直播分辨率  
- 切后台可能中断手机录制  
