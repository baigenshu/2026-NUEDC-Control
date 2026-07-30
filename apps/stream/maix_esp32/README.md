# MaixCAM + ESP32 推流套件

```text
apps/stream/maix_esp32/
├── video_receive/   # ESP32：仅 SoftAP 热点
└── video_send/      # Maix：钢珠检测直播 + 手机录制
```

| 目录 | 平台 | 作用 |
|------|------|------|
| `video_receive` | ESP32 PlatformIO | `MaixCam-ESP` 热点 |
| `video_send` | MaixCAM MaixPy | YOLO 检测 + Flask 网页 |

模型来源：`apps/maixcam/detect_ball/`（需拷到 `video_send/` 或设备 `/root/models/`）。

详见各子目录 `README.md`。
