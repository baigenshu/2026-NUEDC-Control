# MSPM0G3507 shared GCC rules (Keil + Makefile coexistence)
# Each project Makefile must set:
#   TARGET, SRCS, INCLUDES
# then: include ../common/mspm0_gcc.mk
#
# Paths: edit repo-root toolpaths.mk, then: mingw32-make apply-paths

BUILD ?= build

# Locate repo root / common relative to this file
COMMON_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT  := $(patsubst %/,%,$(dir $(COMMON_DIR)))

include $(REPO_ROOT)/toolpaths.mk

# Tool invocations — quote paths that may contain spaces
SYSCONFIG_CLI := "$(SYSCONFIG_ROOT)/sysconfig_cli.bat"
SYSCONFIG_GUI := "$(SYSCONFIG_ROOT)/sysconfig_gui.bat"
CC            := "$(GCC_PATH)/bin/arm-none-eabi-gcc"
OBJCOPY       := "$(GCC_PATH)/bin/arm-none-eabi-objcopy"
SIZE          := "$(GCC_PATH)/bin/arm-none-eabi-size"
JLINK_EXE     := "$(JLINK_ROOT)/JLink.exe"

CPUFLAGS := -mcpu=cortex-m0plus -march=armv6-m -mthumb -mfloat-abi=soft
CFLAGS   := $(CPUFLAGS) -std=c99 -O2 -g -gstrict-dwarf -Wall \
            -ffunction-sections -fdata-sections \
            -D__MSPM0G3507__ \
            $(INCLUDES) \
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

# Project objects under build/ (mirror source paths)
PROJ_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
STARTUP_OBJ := $(BUILD)/startup_mspm0g350x_gcc.o
OBJS := $(PROJ_OBJS) $(STARTUP_OBJ)

.PHONY: all clean size syscfg syscfg-gui flash apply-paths

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

size: $(BUILD)/$(TARGET).out
	@$(SIZE) "$<"

# SysConfig: empty.syscfg + generated files stay at project root (shared with Keil)
syscfg:
	@echo Generating SysConfig files into project root...
	@$(SYSCONFIG_CLI) --compiler gcc --product "$(SDK)/.metadata/product.json" --output . empty.syscfg

syscfg-gui:
	@echo Opening SysConfig GUI...
	@start "" $(SYSCONFIG_GUI) --product "$(SDK)/.metadata/product.json" --compiler gcc --output . empty.syscfg

clean:
	@rm -rf "$(BUILD)"
	@echo Clean done.

flash: all
	@$(JLINK_EXE) -device MSPM0G3507 -if SWD -speed 4000 -autoconnect 1 -CommanderScript .vscode/flash.jlink

# Regenerate .vscode/* from repo-root toolpaths.mk
apply-paths:
	@powershell -NoProfile -ExecutionPolicy Bypass -File "$(REPO_ROOT)/common/scripts/apply_toolpaths.ps1" -ProjectRoot "$(CURDIR)" -ToolpathsFile "$(REPO_ROOT)/toolpaths.mk"
