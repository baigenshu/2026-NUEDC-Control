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

## 2. 机械默认（stepper_cfg.h）

| 参数 | 值 |
|------|-----|
| 导程 | **0.8 mm/圈**（M5 实测） |
| 微步 | 1/8 × 200 = 1600 step/rev |
| 分辨率 | **2000 step/mm** |

## 3. 软件

| 文件 | 说明 |
|------|------|
| `src/Hardware/Inc/stepper_cfg.h` | 导程/微步/速度/软限位 |
| `src/Hardware/Inc/stepper.h` | 驱动 API |
| `src/Hardware/Src/stepper.c` | 开环位置 + 梯形加减速 |
| `src/main.c` | 仅 `SYSCFG_DL_init` + `Stepper_Init` |

```c
Stepper_Init();
Stepper_SetEnable(true);
Stepper_SetSpeedSps(4000);
Stepper_SetTargetMm_x100(200); /* +2.00 mm */
while (Stepper_IsBusy()) { }
```

## 4. 预留（业务侧自行配置）

- 板载 UART0 排针 PA28/PA31、按键 PB8、LED PB9、TMC 共 UART PA8/9
- 当前 **SysConfig 未启用** 上述外设，避免与纯驱动耦合。
