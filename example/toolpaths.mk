# =============================================================================
# MSPM0 tool paths - SINGLE SOURCE OF TRUTH (repo root)
# Edit this file only, then in each project run:
#   mingw32-make apply-paths
# Use forward slashes. No spaces around '='.
# =============================================================================

# Arm GNU Toolchain root (contains bin/arm-none-eabi-gcc.exe)
GCC_PATH=D:/arm/GNU Toolchain mingw-w64-x86_64-arm-none-eabi

# MSPM0 SDK root
SDK=D:/TI/mspm0_sdk_2_05_01_00

# SysConfig install root (contains sysconfig_cli.bat / sysconfig_gui.bat)
SYSCONFIG_ROOT=D:/TI/sysconfig_1.24.1

# J-Link install root (contains JLink.exe / JLinkGDBServerCL.exe)
JLINK_ROOT=D:/app/Jlink

# Directory that contains mingw32-make.exe (or make.exe)
MAKE_BIN=D:/app/MinGW/bin

# Optional: cmake / ninja / openocd bin dirs (for terminal PATH only)
CMAKE_BIN=D:/Arm/cmake/bin
NINJA_BIN=D:/Arm/Ninja
OPENOCD_BIN=D:/Arm/OpenOCD/bin

# Cortex-Debug SVD file (after installing ti-development-tools.cortex-debug-dp-mspm0)
# Leave empty to auto-detect under %USERPROFILE%/.vscode/extensions
SVD_FILE=
