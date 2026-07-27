# car 引脚与外设映射

> 主控：MSPM0G3507 · LQFP-64 · 板卡 **LCKFB-TMX-MSPM0G3507**  
> 工程：`apps/car` · 四轮差速  
> **依据**：实板原理图 = 本文件 = [`app.syscfg`](../syscfg/app.syscfg) / [`ti_msp_dl_config.h`](../syscfg/ti_msp_dl_config.h)  
> 软件架构 [plan.md](plan.md) · API [api.md](api.md)  
> 更新：2026-07-28

---

## 1. 总览

| 外设 | 接口 |
|------|------|
| 电机 A/B/C/D | PWM + GPIO 方向 · STBY |
| 编码器 A/B/C/D | GPIOB 双边沿中断 |
| 灰度 G1~G8 | GPIO 上拉输入 |
| DEBUG | UART0 · PA10 TX / PA11 RX · 115200 |
| TRANS | UART1 · PA8 TX / PA9 RX · 115200 |
| OUT2 | UART2 · PA23 TX / PA24 RX · 115200 |
| OLED | SPI1 · PB8/9 + CS/DC/RES（无 MISO） |
| **按键 B21** | **PB21** · `GPIO_KEY_B21_*` · 上拉，按下=低 |
| **IMU（预留）** | 总线/引脚 TBD · 见 §9 |
| SWD | PA19 / PA20 |

### 车体轮位

| 电机 | 轮位 | 侧 |
|------|------|----|
| **A** | 右后 | 右 |
| **B** | 右前 | 右 |
| **C** | 左前 | 左 |
| **D** | 左后 | 左 |

```text
        前
   C(左前)   B(右前)
   D(左后)   A(右后)
        后
```

- 左 = C + D · 右 = B + A  
- 编码器与电机同名：EncA…D  
- **电机极性** `POL_*`、**编码器符号** `ENC_SIGN_*`、配平 `LEFT/RIGHT_TRIM` 均在 `chassis_cfg.h`（上电实测；POL 与 ENC_SIGN **分开**标定）

---

## 2. 电机

### 2.1 方向与使能

| 网标 | 引脚 | 宏 |
|------|------|----|
| AIN1 / AIN2 | PB13 / PB15 | `GPIO_MOTOR_AIN1/2_PIN` |
| BIN1 / BIN2 | PB4 / PB6 | `GPIO_MOTOR_BIN1/2_PIN` |
| CIN1 / CIN2 | PB1 / PB2 | `GPIO_MOTOR_CIN1/2_PIN` |
| DIN1 / DIN2 | PB3 / PB7 | `GPIO_MOTOR_DIN1/2_PIN` |
| STBY | PB16 | `GPIO_MOTOR_STBY_PIN`（初始低=待机） |

端口：`GPIO_MOTOR_PORT` = GPIOB。

| 模式 | IN1/IN2 | PWM | 说明 |
|------|---------|-----|------|
| 正转 | L / H | duty | 前进方向再由 `POL_*` 映射到 API「+」 |
| 反转 | H / L | duty | |
| **COAST** | L / L | 0 | 滑行 |
| **BRAKE** | H / H | 0 | 短刹（TB6612 类） |
| 待机 | — | — | STBY=0 |

策略代码应通过 `motor` / `chassis` 访问，避免散落宏。

### 2.2 PWM

| 网标 | 轮 | 引脚 | 定时器 | 宏 |
|------|----|------|--------|----|
| PWMA | A 右后 | PA12 | TIMG0 CCP0 | `GPIO_PWMA_C0_*` · `PWMA_INST` |
| PWMC | C 左前 | PA13 | TIMG0 CCP1 | `GPIO_PWMA_C1_*` · 同上 |
| PWMB | B 右前 | PA21 | TIMG6 CCP0 | `GPIO_PWMB_C0_*` · `PWMB_INST` |
| PWMD | D 左后 | PA22 | TIMG6 CCP1 | `GPIO_PWMB_C1_*` · 同上 |

- period = 4000 · 时钟 40 MHz → 约 **10 kHz**  
- **TIMG0 专用于 PWMA**，不得再作里程/控制节拍定时器（与 odometry 工程不同）

```c
DL_TimerG_setCaptureCompareValue(PWMA_INST, duty, GPIO_PWMA_C0_IDX); // A
DL_TimerG_setCaptureCompareValue(PWMA_INST, duty, GPIO_PWMA_C1_IDX); // C
DL_TimerG_setCaptureCompareValue(PWMB_INST, duty, GPIO_PWMB_C0_IDX); // B
DL_TimerG_setCaptureCompareValue(PWMB_INST, duty, GPIO_PWMB_C1_IDX); // D
DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_STBY_PIN);
```

---

## 3. 编码器

| 相 | 引脚 | 宏组 | 轮 |
|----|------|------|----|
| E1A / E1B | PB0 / PB5 | `GPIO_ENCODERA_*` | A 右后 |
| E2A / E2B | PB23 / PB18 | `GPIO_ENCODERB_*` | B 右前 |
| E3A / E3B | PB27 / PB22 | `GPIO_ENCODERC_*` | C 左前 |
| E4A / E4B | PB24 / PB26 | `GPIO_ENCODERD_*` | D 左后 |

GPIOB · 上拉 · 双边沿 · `GPIOB_INT` / GROUP1 · 四倍频查表。  
软件：`ENC_SIGN_*` 使「车体前进 → 脉冲增加」；侧向平均与航向公式见 [plan.md §5.2](plan.md)。

---

## 4. 灰度 G1~G8

| 网标 | 引脚 | 宏 | 权重（对照，**以 cfg 为准**） |
|------|------|----|------------------------------|
| G1 | PB19 | `GPIO_GRAY_PIN_0` | -3500 |
| G2 | PB17 | `GPIO_GRAY_PIN_1` | -2500 |
| G3 | PA16 | `GPIO_GRAY_PIN_2` | -1500 |
| G4 | PA14 | `GPIO_GRAY_PIN_3` | -500 |
| G5 | PB20 | `GPIO_GRAY_PIN_4` | +500 |
| G6 | PB25 | `GPIO_GRAY_PIN_5` | +1500 |
| G7 | PA25 | `GPIO_GRAY_PIN_6` | +2500 |
| G8 | PA27 | `GPIO_GRAY_PIN_7` | +3500 |

上拉输入；黑线=1、浅色=0；`bit0=G1 … bit7=G8`。  
`Gray_GetPosition()` = 各 bit 与 `GRAY_WEIGHT_*`（cfg）加权和；巡线 PID 量纲与此一致。

---

## 5. 串口

| 名称 | 实例 | TX | RX | 波特率 | 宏前缀 |
|------|------|----|----|--------|--------|
| DEBUG | UART0 | PA10 | PA11 | 115200 | `DEBUG_UART` |
| TRANS | UART1 | PA8 | PA9 | 115200 | `TRANS_UART` |
| OUT2 | UART2 | PA23 | PA24 | 115200 | `OUT2_UART` |

- MCU：PA8=`UART1_TX`，PA9=`UART1_RX`。  
- 板丝印 `TRANS_TX`/`RX` 对外 **交叉** 接线。  
- OUT2 同理（如 MaixCAM）。  
- 遥控/视觉协议解析：ISR **只写邮箱**，主循环调 Chassis（见 plan §3.2）。

---

## 6. OLED（SPI1）

| 功能 | 引脚 | 宏 |
|------|------|----|
| SCLK / PICO | PB9 / PB8 | `GPIO_SPI_OLED_*` |
| CS / DC / RES | PB14 / PB11 / PB10 | `SPI_OLED_CTRL_*` |

SPI1 · 20 MHz · MOTO3；`direction=PICO`（只写屏，**不占用 PB21**）；CS 低有效，初始高。

---

## 6.1 板载按键 B21

| 丝印 | 引脚 | 宏 | 电平 |
|------|------|----|------|
| **B21** | **PB21** | `GPIO_KEY_PORT` / `GPIO_KEY_B21_PIN` | 上拉；未按=高，**按下=低** |

- 组名 `GPIO_KEY`（SysConfig）  
- 用途：演示程序启动/重跑开关（见 `main.c`）  
- 与 OLED MISO 互斥：工程已释放 SPI POCI，PB21 专供按键  
- 勿用 BSL(PA18) / RST 作普通键  

---

## 7. 系统

| 项 | 值 |
|----|-----|
| SWDIO / SWCLK | PA19 / PA20 |
| CPUCLK | 80 MHz |
| SysTick | 已使能（可用于 `dt_ms` 时间基，**勿占用 TIMG0**） |

---

## 8. 软件映射备忘

| 项目 | 值 |
|------|-----|
| 控制节拍 | 主循环 / SysTick，5–10 ms；非 TIMG0 |
| GROUP1 | 四路编码器共用；handler 内只计数 |
| 停车 | COAST/BRAKE 见 §2.1 与 `chassis_cfg.h` |

---

## 9. IMU / 陀螺仪（预留，未接线）

当前 **SysConfig 未配置 IMU**。软件预留 `imu.h`，默认 `IMU_ENABLED=0`。

| 候选 | 外设 | 注意 |
|------|------|------|
| UART 姿态模块（如 IMU601） | 独立 UART | car：UART0=DEBUG，UART1 PA8/9=**TRANS**，UART2 PA23/24=OUT2；接 IMU 需占用 OUT2/改线/另开 UART，并更新本表与 SysConfig |
| I2C 六轴 | 新建 I2C | 选定空闲脚后写入本表 |
| SPI 六轴 | SPI+CS | 避开 OLED SPI1 |

航向与 Chassis 一致（**+** 左转）；`IMU_YAW_SIGN` 适配安装。  
接入：置 `IMU_ENABLED`、实现 `imu.c`、更新本节实脚、`CHASSIS_HEADING_SOURCE=0|1`（**不要开未定义的融合=2**）、按 plan §7.4 航向标定。

---

## 10. 文档

| 文件 | 内容 |
|------|------|
| **pins.md** | 引脚与宏 |
| [plan.md](plan.md) | 架构（状态机 · odom · 阶段 · IMU） |
| [api.md](api.md) | API 契约 |
| [硬件接线表.html](../syscfg/硬件接线表.html) | 可视化接线 |
