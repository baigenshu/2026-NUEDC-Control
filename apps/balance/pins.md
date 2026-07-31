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
| 校零启动按键 | PB8 | `GPIO_BALANCE_KEY_START_PIN` · 上拉输入，按下低电平 |

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
| `vision_uart.*` | UART 收 0x02/0x12/0x13 |
| `ball_ctrl_*` | 位置 PD 闭环任意定点停球 |
| `tmc2209_*` | 上电写 IHOLD/IRUN |
| `main.c` | SysTick + Poll + Update |

```c
BallCtrl_Init();
BallCtrl_SetTargetMm_x100(0);   /* 停在 O；改此即任意定点 */
/* 人工调平后按 PB8 或收 0x13 start：稳定采样、校准视觉偏置、机械置零、启控 */
/* 主循环：VisionUart_Poll(); 启动控制轮询；BallCtrl_Update(); */
```

## 4. 视觉球位 UART

| 项 | 约定 |
|----|------|
| 协议 | type=`0x02` 定长 13 B · 见 [docs/vision_proto.md](docs/vision_proto.md) |
| 头文件 | `src/Hardware/Inc/ball_proto.h` |
| 波特率 | 115200 8N1 |
| 接线 | 视觉 TX → **PA31 (UART0 RX)** · 共地；可选 PA28 TX |
| 发送端 | `apps/maixcam/opencv`（`pack_ball_frame`） |
| 定点命令 | type=`0x12` · `target_mm` 整 mm |
| 校零启动 | `AA 55 13 01 00 00 00 00 00 00 00 00 14` |
| 停止/释放电机 | `AA 55 13 00 00 00 00 00 00 00 00 00 13` |

## 5. 极性与标定

- 协议：视觉 `pos_mm` + = O 右侧（与图示一致）。
- 控制：`BALL_CTRL_SIGN` / `STEPPER_DIR_SIGN`；实装反了只改其一。
- 上电后闭环和电机保持关闭，可先通过 0x12 设置目标点。
- 人工将摆杆调平，并把小球静置在 O 点或指定目标点；松开后再按 PB8。
- 启动请求会重新采集 8 帧有效球位；极差不超过 3 mm 时，以其平均值校准视觉偏置、将当前电机位置记为软件零点并启动闭环。视觉无球或球仍在移动时会继续待机。
- 运行中按 PB8 不会重复校零。需要重新校准时，先发送 0x13 stop，手动调平，再按 PB8 或发送 0x13 start。
