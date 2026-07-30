# balance 引脚与外设映射

> 工程：apps/balance · **TMC_A 单轴步进驱动**（纯驱动层，业务另接）  
> 主控：MSPM0G3507 · 云台/平衡驱动板  
> 依据：SCH 云台驱动原理图 · 更新 2026-07-29

---

## 1. 驱动占用

| 功能 | 引脚 | 宏 |
|------|------|----|
| EN（ENN，低有效使能） | PA14 | `GPIO_STEPPER_EN_PIN` |
| DIR | PA13 | `GPIO_STEPPER_DIR_PIN` |
| STEP | PA12 | `GPIO_STEPPER_STEP_PIN` |
| 脉冲时基 | TIMG7 | `STEP_TIM_INST` · 10 µs ZERO IRQ |
| 端口 | GPIOA | `GPIO_STEPPER_PORT` |

硬件固定：MS1/MS2=GND · VM=+12V · VIO=3V3 · TMC_B 未用。

## 2. 电机 / 机械（42×23 + M5）

| 参数 | 值 |
|------|-----|
| 电机 | 42×23 · 1.8° · **1.2 A 额定** · 4.2 Ω · 4.0 mH · 0.16 N·m |
| 建议 IRUN | **0.7–0.9 A**（轻载，降发热；电位器/VREF 硬件调） |
| 建议 IHOLD | **0.25–0.4 A**；软件到位/丢球会 **关 EN** |
| 导程 | **0.8 mm/圈**（M5 实测） |
| 微步 | 1/8 × 200 = 1600 step/rev → **2000 step/mm** |

## 3. 软件

| 文件 | 说明 |
|------|------|
| `stepper_*` | 丝杆开环位置 + 梯形加减速 |
| `vision_uart.*` | UART 收 0x02/0x12 |
| `ball_ctrl_*` | PD 闭环任意定点停球 |
| `main.c` | SysTick + Poll + Update |

```c
BallCtrl_Init();
BallCtrl_SetTargetMm_x100(0);   /* 停在 O；改此即任意定点 */
BallCtrl_Enable(true);
/* 主循环：VisionUart_Poll(); BallCtrl_Update(); */
```

## 4. 视觉球位 UART（协议已定，外设待开）

| 项 | 约定 |
|----|------|
| 协议 | type=`0x02` 定长 13 B · 见 [docs/vision_proto.md](docs/vision_proto.md) |
| 头文件 | `src/Hardware/Inc/ball_proto.h` |
| 波特率 | 115200 8N1 |
| 接线 | 视觉 TX → **PA31 (UART0 RX)** · 共地；可选 PA28 TX |
| 发送端 | `apps/maixcam/opencv`（`pack_ball_frame`） |

SysConfig 已启用 **VISION_UART = UART0**（PA28 TX / PA31 RX @ 115200）。

| 软件 | 说明 |
|------|------|
| `vision_uart` | 收 type 0x02 球位 / 0x12 定点 |
| `ball_ctrl` | PD 闭环，任意定点停球 |

## 5. 其它预留

- 按键 PB8、LED PB9、TMC 共 UART PA8/9
- 未启用。
