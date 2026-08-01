# balance 引脚与外设映射

> apps/balance · 曲柄连杆摆杆 + 视觉球位闭环 · 2026-07-31  
> 主控：MSPM0G3507 · 云台/平衡驱动板  
> 机械：合页铰支 + 25 cm 凹槽摆杆 + 步进曲柄连杆（非丝杆）

---

## 1. 驱动占用

| 功能 | 引脚 | 宏 |
|------|------|----|
| EN（ENN，低有效使能） | PA14 | `GPIO_STEPPER_EN_PIN` |
| DIR | PA13 | `GPIO_STEPPER_DIR_PIN` |
| STEP | PA12 | `GPIO_STEPPER_STEP_PIN` |
| 脉冲时基 | TIMG7 | `STEP_TIM_INST` · 10 µs ZERO IRQ |
| 端口 | GPIOA | `GPIO_STEPPER_PORT` |
| TMC UART TX | PA8 | `TMC_UART_INST` · UART1 · 115200 |
| TMC UART RX | PA9 | `TMC_UART_INST` · UART1 · 115200 |
| 视觉 UART TX | PA28 | `VISION_UART` · UART0 · 115200 |
| 视觉 UART RX | PA31 | `VISION_UART` · UART0 · 115200 |
| 平衡启动按键 | PB8 | `GPIO_BALANCE_KEY_START_PIN` · 上拉输入，按下低电平 |

硬件固定：MS1/MS2=GND · VM=+12V · VIO=3V3 · TMC_B 未用。

## 2. 电机 / 机械（曲柄连杆）

| 参数 | 值 |
|------|-----|
| 电机 | 42 系 · 1.8° · 1.2 A 额定 · TMC2209 |
| 微步 | 1/8 × 200 = 1600 step/rev |
| 执行机构 | 曲柄 → 连杆 → 摆杆倾角 |
| 抽象单位 | 1.00 unit = `STEPPER_STEPS_PER_UNIT`(默认 200) 微步 |
| 软限位 | ±400 微步（约 ±90° 电机轴，防顶死） |
| 电流 | `tmc2209_cfg.h`：IRUN 850 mA / IHOLD 220 mA（可降） |

BallCtrl 输出 `rod_x100`（0.01 unit）经 `Stepper_SetTargetMm_x100()` 换算为曲柄微步。

## 3. 软件

| 文件 | 说明 |
|------|------|
| `stepper_*` | 曲柄开环位置 + 梯形加减速 |
| `vision_uart.*` | UART 收 0x02 球位和 0x13 四键控制（保留 0x12 兼容解析） |
| `ball_ctrl_*` | 位置 PD 闭环任意定点停球 |
| `preset_motion_*` | `0 → +50 → -50 mm` 预设运动 |
| `tmc2209_*` | 上电写 IHOLD/IRUN |
| `main.c` | SysTick + Poll + Update |

```c
BallCtrl_Init();
BallCtrl_SetTargetMm_x100(0);   /* 默认目标：视觉 O 点 */
/* PB8/0x13 Start：机械置零后在 O 点平衡；0x13 Preset：启动往返预设 */
/* 主循环：VisionUart_Poll(); 启动控制轮询；BallCtrl_Update(!PresetMotion_IsActive()); */
```

## 4. 视觉球位 UART

| 项 | 约定 |
|----|------|
| 协议 | type=`0x02` 定长 13 B · 见 [docs/vision_proto.md](docs/vision_proto.md) |
| 头文件 | `src/Hardware/Inc/ball_proto.h` |
| 波特率 | 115200 8N1 |
| 接线 | 视觉 TX → **PA31 (UART0 RX)** · 共地；可选 PA28 TX |
| 发送端 | `apps/maixcam/opencv`（`pack_ball_frame`） |
| Start（PB8 等效） | `AA 55 13 01 00 00 00 00 00 00 00 00 14` |
| ±5 预设 | `AA 55 13 02 00 00 00 00 00 00 00 00 15` |
| Reset / 停止释放电机 | `AA 55 13 00 00 00 00 00 00 00 00 00 13` |

## 5. 极性与标定

- 协议：视觉 `pos_mm` + = O 右侧（与图示一致）。
- 控制：`BALL_CTRL_SIGN` / `STEPPER_DIR_SIGN`；实装反了只改其一。
- 上电后闭环和电机保持关闭，默认目标为视觉 O 点。
- 人工将摆杆调平后松开再按 PB8，或点屏幕 Start；两者均将当前摆臂位置记为软件零点并立即在 O 点启动闭环，不修改视觉球位坐标。
- 需要重新校准时，先点 Reset 或发送 `0x13 Reset`，手动调平，再按 PB8 或 Start。
- ±5 预设只在 O 点闭环稳定后开始固定时序，等待 O 点稳定的时间不属于预设 5 s 动作窗口。轨迹阶段与普通 PID 使用同一 `BALL_CTRL_SIGN` 极性。

| 阶段 | 逻辑摆臂目标 | 相对上阶段步数 | 保持时间 | 用途 |
|------|-------------:|---------------:|---------:|------|
| 正向推送 | `+48`（`+77` 微步） | `+77` | 1200 ms | 使球向视觉 `+50 mm` 运动 |
| 正向制动 | `-16`（`-26` 微步） | `-103` | 250 ms | 给正向运动减速 |
| 反向推送 | `-24`（`-38` 微步） | `-12` | 1200 ms | 使球越过 O 向视觉 `-50 mm` 运动 |
| 反向制动 | `+16`（`+26` 微步） | `+64` | 400 ms | 给反向运动减速 |
| 最终收尾 | PID 目标 `-50 mm` | 由闭环计算 | 至 5 s | 使用 ±5 mm、低速确认稳定停住 |

实测旧参数 `+24` 保持约 700 ms 时，小球仅由约 `+1 mm` 移至 `+8 mm`，因而将仅正向推送的预设专用摆臂幅度扩大到 `+48`。负向 `-24` 保持约 1200 ms 已能使小球最终落在约 `-45 mm`，因此保持不变。逻辑摆臂目标的实际 DIR 引脚方向由 `BALL_CTRL_SIGN` 和 `STEPPER_DIR_SIGN` 共同决定；固定四段总长为 3050 ms。预设动作窗口为 5 s：正向过程中必须至少一次进入 `+50 ±10 mm`，最终必须在 `-50 mm` 按原有到位判据稳定才算完成；否则状态记为超时，但仍将闭环目标保持在 `-50 mm`，供 trace 调参定位。
- 误差连续 8 帧位于 ±5 mm 且速度不超过 3 mm/s 时进入到位保持：清积分，停止步进脉冲并冻结当前执行器位置，不再追逐小范围视觉误差。
- 到位保持采用滞回，球位仍在 ±8 mm 内时保持不动作；超出 ±8 mm 后从零输出恢复闭环微调。
