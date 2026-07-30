# car 架构（重写）

> 范围：**四轮差速** · **8 路灰度巡线** · **OLED** · **H 题第 2 项一圈任务**  
> 引脚 [pins.md](pins.md) · API [api.md](api.md)  
> 更新：2026-07-30 彻底简化重写

## 题目对应

H 题第 2 项：小车置于 A 点，按键启动后沿黑线顺时针行驶一圈并停到 A 点，计时停止并显示总时间（≤20 s，停车偏差 ≤2 cm）。

## 目录

```text
apps/car/src/
├── main.c                 # 时基 / B21 / OLED / 主循环
├── Hardware/
│   ├── Inc/  motor.h encoder.h gray.h OLED.h OLED_Font.h
│   └── Src/  motor.c encoder.c gray.c OLED.c
└── Function/
    ├── Inc/  chassis_cfg.h chassis.h line_track.h lap_task.h
    └── Src/  chassis.c line_track.c lap_task.c
```

## 数据流

```text
B21 ──► LapTask 状态机
Gray ──► LineTrack PD ──► Chassis_Arcade ──► Motor A/B/C/D
Encoder ──► Chassis_Update(odom) ──► LapTask 里程锁
main ──► OLED：状态 / 时间 / 灰度 / 里程·误差
```

## 轮位

```text
        前
   B(左前)   C(右前)
   A(左后)   D(右后)
        后
左 = A+B · 右 = C+D
```

## 一圈任务

1. `WAIT`：停在 A 点附近，车头朝 B（顺时针）
2. B21 按下 → 清 odom、开巡线、开始计时
3. 前 300 ms 低速起步，随后巡航
4. 离开起点横线后，里程 ≥ `LAP_MIN_DISTANCE_CM` 才允许认终点
5. 终点前约 0.8 m 降速
6. **唯一正常停车条件**：≥4 路灰度同时黑（垂直停车基准线），连续确认 3 拍
7. 刹车，冻结总时间，OLED 显示 `LAP:DONE`

## 原则

1. 配置只进 `chassis_cfg.h`
2. 硬件驱动不写业务；电机只经 `Chassis`
3. 竞赛流程只进 `lap_task.*`；`main` 只做时基/按键/显示
4. 节拍用 SysTick 软 ms，**禁止 TIMG0**（PWMA）
5. 标定顺序：`POL_*` → `ENC_SIGN_*` → TRIM → 巡线 PD → 停车线阈值
