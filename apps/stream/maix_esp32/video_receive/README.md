# video_receive — ESP32 SoftAP only

路径：`apps/stream/maix_esp32/video_receive/`

**当前：仅开热点**，TFT / 拉流 / SD 已停用。

## 作用

```text
ESP32 SoftAP: MaixCam-ESP / grx060313
  ← Maix（../video_send）、手机加入
  ← 视频由 Maix 推流，手机浏览器观看
```

## 编译烧录

```bash
cd apps/stream/maix_esp32/video_receive
pio run -t upload
pio device monitor -b 115200
```

## 与 video_send

配对目录：`apps/stream/maix_esp32/video_send`

1. 本程序开热点  
2. Maix 跑 `video_send` → Phone Web  
3. 手机同热点打开 `http://<MaixIP>:8000`  

## 恢复屏幕方案

将 `src/main.cpp` 中 `ENABLE_*` 宏置 1，并恢复完整拉流/TFT 代码（git 历史）。
