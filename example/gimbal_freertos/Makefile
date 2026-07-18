# gimbal_freertos — MSPM0G3507 + FreeRTOS (GCC)
# 原 example/gimbal 不修改；本目录独立编译。

TARGET := app
BUILD  ?= build

COMMON_DIR := $(abspath ../common)
REPO_ROOT  := $(abspath ..)
include $(REPO_ROOT)/toolpaths.mk

# Tools
CC      := "$(GCC_PATH)/bin/arm-none-eabi-gcc"
OBJCOPY := "$(GCC_PATH)/bin/arm-none-eabi-objcopy"
SIZE    := "$(GCC_PATH)/bin/arm-none-eabi-size"
JLINK_EXE := "$(JLINK_ROOT)/JLink.exe"

FREERTOS_SRC  := $(SDK)/kernel/freertos/Source
FREERTOS_PORT := $(FREERTOS_SRC)/portable/GCC/ARM_CM0

# ---- sources ----
APP_SRCS := \
	main.c \
	ti_msp_dl_config.c \
	Function/Src/delay.c \
	Function/Src/uart.c \
	Hardware/step_motor.c \
	FreeRTOS/StaticAllocs_freertos.c \
	FreeRTOS/AppHooks_freertos.c

RTOS_SRCS := \
	$(FREERTOS_SRC)/tasks.c \
	$(FREERTOS_SRC)/queue.c \
	$(FREERTOS_SRC)/list.c \
	$(FREERTOS_SRC)/timers.c \
	$(FREERTOS_SRC)/event_groups.c \
	$(FREERTOS_SRC)/stream_buffer.c \
	$(FREERTOS_SRC)/portable/MemMang/heap_4.c \
	$(FREERTOS_PORT)/port.c

CPUFLAGS := -mcpu=cortex-m0plus -march=armv6-m -mthumb -mfloat-abi=soft
CFLAGS   := $(CPUFLAGS) -std=c99 -O2 -g -gstrict-dwarf -Wall \
	-ffunction-sections -fdata-sections \
	-D__MSPM0G3507__ \
	-I. -IFunction/Inc -IHardware -IFreeRTOS \
	-I"$(FREERTOS_SRC)/include" \
	-I"$(FREERTOS_PORT)" \
	-I"$(SDK)/source" \
	-I"$(SDK)/source/third_party/CMSIS/Core/Include" \
	-I"$(GCC_PATH)/arm-none-eabi/include"

LDFLAGS  := $(CPUFLAGS) -nostartfiles -static -Wl,--gc-sections \
	-Wl,-Map,$(BUILD)/$(TARGET).map \
	-T"$(COMMON_DIR)/mspm0g3507.lds" \
	-L"$(SDK)/source" \
	-L"$(SDK)/source/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x" \
	-l:driverlib.a \
	--specs=nano.specs --specs=nosys.specs \
	-lgcc -lc -lm

APP_OBJS    := $(patsubst %.c,$(BUILD)/%.o,$(APP_SRCS))
STARTUP_OBJ := $(BUILD)/startup_mspm0g350x_gcc.o
RTOS_OBJS   := $(addprefix $(BUILD)/rtos/,$(notdir $(RTOS_SRCS:.c=.o)))
OBJS        := $(APP_OBJS) $(STARTUP_OBJ) $(RTOS_OBJS)

.PHONY: all clean size flash

all: $(BUILD)/$(TARGET).out $(BUILD)/$(TARGET).hex size

$(BUILD)/$(TARGET).out: $(OBJS) $(COMMON_DIR)/mspm0g3507.lds
	@echo Linking $@
	@$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).out
	@$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.o: %.c
	@mkdir -p "$(dir $@)"
	@echo CC $<
	@$(CC) $(CFLAGS) -c "$<" -o "$@"

$(STARTUP_OBJ): $(COMMON_DIR)/startup_mspm0g350x_gcc.c
	@mkdir -p "$(dir $@)"
	@echo CC $<
	@$(CC) $(CFLAGS) -c "$<" -o "$@"

# FreeRTOS kernel objects
$(BUILD)/rtos/tasks.o: $(FREERTOS_SRC)/tasks.c
	@mkdir -p "$(dir $@)"; echo CC $<; $(CC) $(CFLAGS) -c "$<" -o "$@"
$(BUILD)/rtos/queue.o: $(FREERTOS_SRC)/queue.c
	@mkdir -p "$(dir $@)"; echo CC $<; $(CC) $(CFLAGS) -c "$<" -o "$@"
$(BUILD)/rtos/list.o: $(FREERTOS_SRC)/list.c
	@mkdir -p "$(dir $@)"; echo CC $<; $(CC) $(CFLAGS) -c "$<" -o "$@"
$(BUILD)/rtos/timers.o: $(FREERTOS_SRC)/timers.c
	@mkdir -p "$(dir $@)"; echo CC $<; $(CC) $(CFLAGS) -c "$<" -o "$@"
$(BUILD)/rtos/event_groups.o: $(FREERTOS_SRC)/event_groups.c
	@mkdir -p "$(dir $@)"; echo CC $<; $(CC) $(CFLAGS) -c "$<" -o "$@"
$(BUILD)/rtos/stream_buffer.o: $(FREERTOS_SRC)/stream_buffer.c
	@mkdir -p "$(dir $@)"; echo CC $<; $(CC) $(CFLAGS) -c "$<" -o "$@"
$(BUILD)/rtos/heap_4.o: $(FREERTOS_SRC)/portable/MemMang/heap_4.c
	@mkdir -p "$(dir $@)"; echo CC $<; $(CC) $(CFLAGS) -c "$<" -o "$@"
$(BUILD)/rtos/port.o: $(FREERTOS_PORT)/port.c
	@mkdir -p "$(dir $@)"; echo CC $<; $(CC) $(CFLAGS) -c "$<" -o "$@"

size: $(BUILD)/$(TARGET).out
	@$(SIZE) "$<"

clean:
	@rm -rf "$(BUILD)"
	@echo Clean done.

flash: all
	@$(JLINK_EXE) -device MSPM0G3507 -if SWD -speed 4000 -autoconnect 1 -CommanderScript .vscode/flash.jlink
