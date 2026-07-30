# MaixCAM 视觉侧

每个子目录是一个独立应用，用 MaixVision 打开对应 `main.py` 运行，或按目录打包安装。

## 目录

```text
apps/maixcam/
├── collect/          # 检测数据集采样（train/val, pos/neg）
├── detect_ball/      # 钢珠 YOLOv8 检测
├── opencv/           # OpenCV 凹槽钢珠 + UART 位置（控球主环）
├── red_track/        # 红色目标 + IMU → UART 跟踪帧（云台）
└── tools/            # PC 工具（串口发测试帧等）
```

| 目录 | 作用 | 设备依赖 |
|------|------|----------|
| `collect/` | 拍图存 `/root/datasets/detect/` | 无模型 |
| `detect_ball/` | 钢珠检测（本机屏） | `steel_ball.mud` + `.cvimodel` |
| `opencv/` | 凹槽 ROI 检测，串口发 pos | OpenCV + UART |
| `../stream/maix_esp32/video_send/` | 钢珠检测 **网页直播** + 手机录 | 同上模型（拷到该目录或 `/root/models`） |
| `red_track/` | 色块跟踪 + 姿态串口 | 云台 UART |
| `tools/` | 在 PC 上跑 | pyserial |

## 部署提示

- 模型文件较大，`detect_ball` 需把 mud/cvimodel 一并上传
- 也可只放到 `/root/models/`，`detect_ball/main.py` 会自动查找
- SSH：`root@ip`，密码 `root`
