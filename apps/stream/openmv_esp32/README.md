# OpenMV H7 + ESP32 无线图传（QVGA + 手机录制）

录制 UI + FreeRTOS 收串口 + 客户端推流。已做 **A/B 稳画**（未做 C 阻塞拉流）。

## 当前参数（A）

| 项 | 值 |
|----|-----|
| 分辨率 | **QVGA 320×240** |
| JPEG quality | 35 |
| FRAME_MS | **100** |
| 波特率 | **460800**（两端一致） |
| MAX_JPEG | **24KB**（两端一致） |

## 稳画改动（B）

- 推流时 `sending_buf`：禁止覆盖正在发送的 JPEG 缓冲  
- `clients_mux`：保护 `web_clients` / `client_count`  

未改：`/stream` 仍为登记客户端后 return + `push_task` 推送。

## 接线

| 信号 | OpenMV | ESP32 |
|------|--------|-------|
| TX→RX | P4 | GPIO16 |
| RX←TX | P5 | GPIO17（可选） |
| GND | GND | GND |

## 使用

1. 烧录 `esp32/`  
2. 运行 `openmv/main.py`  
3. 手机连 `OpenMV-ESP` / `12345678`  
4. 打开 `http://192.168.4.1/`  

## 调试

ESP 串口 115200：`frames / len / drops / resync / fail / clients / rx`  
`http://192.168.4.1/health`
