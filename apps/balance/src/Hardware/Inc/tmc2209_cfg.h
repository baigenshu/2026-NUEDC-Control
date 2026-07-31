/**
 * @file tmc2209_cfg.h
 * @brief TMC2209 UART 电流 + 微步配置
 */
#ifndef TMC2209_CFG_H
#define TMC2209_CFG_H

#ifndef TMC2209_MOTOR_RATED_MA
#define TMC2209_MOTOR_RATED_MA       (1200u)
#endif
#ifndef TMC2209_IRUN_MA
#define TMC2209_IRUN_MA              (850u)
#endif
#ifndef TMC2209_IHOLD_MA
#define TMC2209_IHOLD_MA             (600u)
#endif

/* 板级采样电阻 / 内部参考，用于 mA → 寄存器码换算 */
#ifndef TMC2209_VFS_MV
#define TMC2209_VFS_MV               (325u)
#endif
#ifndef TMC2209_RSENSE_MOHM
#define TMC2209_RSENSE_MOHM          (110u)
#endif
#ifndef TMC2209_IHOLDDELAY_CODE
#define TMC2209_IHOLDDELAY_CODE      (6u)
#endif
#ifndef TMC2209_IRUNDELAY_CODE
#define TMC2209_IRUNDELAY_CODE       (0u)
#endif

/*
 * UART 微步（覆盖 MS1/MS2 引脚）
 * MRES: 0=1/256 … 4=1/16 … 5=1/8 … 8=整步
 * 与 STEPPER_MICROSTEPS 保持一致
 */
#ifndef TMC2209_MRES_CODE
#define TMC2209_MRES_CODE            (4u)   /* 1/16 */
#endif

/* GCONF: pdn_disable | mstep_reg_select | multistep_filt */
#ifndef TMC2209_GCONF_VALUE
#define TMC2209_GCONF_VALUE          (0x000001C0u)
#endif

/*
 * CHOPCONF 基底（不含 MRES）：共用默认可用 TOFF/HSTRT
 * 实际写入 = (BASE & ~MRES位) | (MRES<<24)
 */
#ifndef TMC2209_CHOPCONF_BASE
#define TMC2209_CHOPCONF_BASE        (0x10000053u)
#endif

#endif /* TMC2209_CFG_H */
