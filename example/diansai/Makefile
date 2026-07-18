# diansai — MSPM0G3507 GCC build (Keil project under keil/ unchanged)
TARGET   := app
SRCS     := main.c \
            ti_msp_dl_config.c \
            Function/Src/bsp_iic.c \
            Function/Src/bsp_systick.c \
            Hardware/Src/grayscale_sensor.c \
            Hardware/Src/motor.c \
            Hardware/Src/OLED.c \
            Hardware/Src/encoder.c \
            Hardware/Src/step_motor.c \
            Hardware/Src/imu601.c
INCLUDES := -I. -IFunction/Inc -IHardware/Inc

include ../common/mspm0_gcc.mk
