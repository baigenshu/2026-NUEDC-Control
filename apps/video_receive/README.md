# video_receive — ESP32 SoftAP only

**当前：仅开热点**，TFT 预览 / 拉流 / SD 录制已停用（宏为 0）。

## 作用

```text
ESP32 SoftAP: MaixCam-ESP / grx060313
  IP 通常 192.168.4.1
  ← MaixCAM、手机加入
  ← 视频由 Maix 推流，手机浏览器观看
  ← 本机不上屏、不拉流、不写 SD
```

## 编译烧录

```bash
cd apps/video_receive
pio run -t upload
pio device monitor -b 115200
```

串口应见：`Mode: hotspot only`、定期 `STA=n`。

## 恢复屏幕方案（以后）

在 `src/main.cpp` 顶部将：

```cpp
ENABLE_TFT_PREVIEW  1
ENABLE_STREAM_PULL  1
ENABLE_SD_RECORD    1  // 如需
```

并恢复完整拉流/TFT/SD 代码（可从 git 历史取回）。

## 与 video_send

1. 本程序开热点  
2. Maix 跑 `video_send` → **Phone Web**  
3. 手机连同一热点，打开 `http://<MaixIP>:8000`  
4. 网页上「开始/结束录制」→ 文件保存在**手机**  
