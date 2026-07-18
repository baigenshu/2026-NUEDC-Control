# gimbal_freertos
#
# FreeRTOS 版云台工程（从 example/gimbal 拷贝驱动，不修改原工程）
#
# 硬件接线与 gimbal 相同:
#   M1: PA0 STEP / PA1 DIR / PA7 DCY / PA8 SLP / PA9 RST  (1:4 齿轮 Yaw)
#   M2: PA12 STEP / PA13 DIR / PA14 DCY / PA15 SLP / PA16 RST
#   PRINT UART0: PA28 TX / PA31 RX  115200
#   DEBUG UART1: PB6 TX / PB7 RX    115200
#
# 编译 (GCC):
#   cd example/gimbal_freertos
#   mingw32-make -j8
#   mingw32-make flash   # 需 .vscode/flash.jlink
#
# 任务:
#   motion_task — 双步进开环 90° 往返
#   comm_task   — 串口心跳
#
# FreeRTOS 源码来自 MSPM0 SDK: kernel/freertos/Source (+ GCC ARM_CM0 port)
