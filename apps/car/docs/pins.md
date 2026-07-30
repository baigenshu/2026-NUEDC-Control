# car 引脚

> MSPM0G3507 · LCKFB-TMX · 电赛板  
> 与 app.syscfg / ti_msp_dl_config.h 一致  
> 更新：2026-07-29 精简重写

## 轮位

| 电机 | 位置 | PWM | IN1/IN2 |
|------|------|-----|---------|
| A | 左后 | PA12 TIMG0 CCP0 | PB13 / PB15 |
| B | 左前 | PA21 TIMG6 CCP0 | PB4 / PB6 |
| C | 右前 | PA13 TIMG0 CCP1 | PB1 / PB2 |
| D | 右后 | PA22 TIMG6 CCP1 | PB3 / PB7 |
| STBY | - | - | PB16 |

编码器与电机同名 E1=A..E4=D（GPIOB 双边沿）。

## 灰度 G1-G8

PB19, PB17, PA16, PA14, PB20, PB25, PA25, PA27（上拉；GRAY_ACTIVE_LOW）

## OLED

SPI1 PB9 SCLK / PB8 PICO · CS PB14 · DC PB11 · RES PB10（SH1106，列偏移 2）

## 按键

B21 = PB21 上拉，按下=低 → 巡线开关

## 其它（板载，软件暂不用）

DEBUG UART0 PA10/11 · TRANS UART1 PA8/9 · OUT2 UART2 PA23/24
