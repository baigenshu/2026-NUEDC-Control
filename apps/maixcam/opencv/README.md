# Ball OpenCV → balance

与 `apps/balance` 视觉协议对接（`docs/vision_proto.md`）。

## 接线（3.3V 共地）

| MaixCAM | balance |
|---------|---------|
| A16 TX（Cam2: A21） | **PA31** UART0 RX |
| A17 RX 可选 | PA28 TX |
| GND | GND |

波特率 **115200 8N1**。

## 发送

| type | 内容 | 周期 |
|------|------|------|
| `0x02` | 球位 pos 0.1mm + conf + cx/cy | ≥20 ms |
| `0x12` | 停球定点（屏 **SP** 键 / 上电同步） | 变更时 |

- `conf < 30` 强制 `found=0`（与 MCU `BALL_CONF_MIN` 一致）
- 退出时发一帧 lost，主控倾角回水平

## 屏显

- 黄线：O 点  
- 粉线：当前 setpoint  
- **SP**：在 `0 / ±25 / ±50 mm` 间切换并下发 `0x12`  
- **Md / ROI / Exit**：模式、ROI 上下、退出  

## 运行

MaixVision 打开本目录 `main.py` 运行，或按 `app.yaml` 安装。

预设停点改 `SETPOINT_PRESETS_MM`；ROI 改文件头 `ROI_*`。
