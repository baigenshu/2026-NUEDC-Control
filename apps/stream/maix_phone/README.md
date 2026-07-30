# maix_phone — 纯 MJPEG 推流

无网页、无 YOLO、无 Flask。自有 App 拉流即可。

## 地址

```text
http://<MaixIP>:8000/stream
```

Content-Type: `multipart/x-mixed-replace; boundary=frame`（标准 MJPEG）

## 使用

1. Maix 已连局域网（与 App 同网段）
2. 若相机卡住：重启 Maix 后再跑
3. 运行 `main.py`
4. 串口打印 `MJPEG: http://.../stream` 后，App 连接该 URL

## 参数（main.py 顶部）

| 常量 | 默认 |
|------|------|
| `HTTP_PORT` | 8000 |
| `CAM_W/H` | 320×240 |
| `JPEG_QUALITY` | 45 |
