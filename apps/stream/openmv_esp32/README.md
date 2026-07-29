# OpenMV H7 + ESP32 无线图传（手机网页）

OpenMV 采图压缩后经 UART 发给 ESP32；ESP32 开 SoftAP，手机浏览器看 MJPEG。

## 接线

| 信号 | OpenMV H7 | ESP32 DOIT DevKit |
|------|-----------|-------------------|
| TX→RX | **P4** (UART3 TX) | **GPIO16** (Serial2 RX) |
| RX←TX | **P5** (UART3 RX) | **GPIO17** (Serial2 TX) |
| GND | GND | GND（必须共地） |

供电：两板各自 USB 即可。

## 串口协议

```text
0xAA 0x55 | len (uint32 little-endian) | JPEG payload
```

波特率默认 **921600**（两端一致）。丢帧/花屏可降到 460800 或 115200。

## 手机使用

1. PlatformIO 打开 `esp32/`，烧录
2. 串口监视器 115200，应看到 AP 与 IP
3. OpenMV IDE 运行 `openmv/main.py`
4. 手机连接 WiFi：
   - SSID：`OpenMV-ESP`
   - 密码：`12345678`
5. 浏览器打开：`http://192.168.4.1/`
6. 可选健康检查：`http://192.168.4.1/health`

## 目录

| 路径 | 说明 |
|------|------|
| `openmv/main.py` | 采图 + 组帧发送 |
| `esp32/` | PlatformIO 工程：收帧 + SoftAP + `/` `/stream` |

## 调参（先通再优）

| 位置 | 常量 | 建议 |
|------|------|------|
| OpenMV | `JPEG_QUALITY` | 30–40 |
| OpenMV | `FRAME_MS` | 50–100 |
| OpenMV | `sensor.QQVGA` | 首版用 160×120 |
| 两端 | `BAUD` / `UART_BAUD` | 须相同 |
| 两端 | `MAX_JPEG` | 默认 16KB（QQVGA 足够；加大需注意 ESP DRAM） |

## 联调顺序

1. 只烧 ESP：手机能开网页（可能暂无图）
2. OpenMV IDE 看 `jpg=` 长度是否为数 KB
3. 接好 UART 后网页应出实时画面
4. 再提高分辨率或质量
