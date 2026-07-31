/**
 * @file tmc2209.c
 * @brief TMC2209 UART: IHOLD/IRUN + microstep (CHOPCONF MRES).
 */
#include "tmc2209.h"
#include "tmc2209_cfg.h"
#include "stepper_cfg.h"
#include "ti_msp_dl_config.h"

#if defined(TMC_UART_INST)

#ifndef TMC2209_SLAVE_ADDR
#define TMC2209_SLAVE_ADDR         (0u)
#endif

#define TMC2209_REG_GCONF          (0x00u)
#define TMC2209_REG_IHOLD_IRUN     (0x10u)
#define TMC2209_REG_CHOPCONF       (0x6Cu)
#define TMC2209_MRES_SHIFT         (24u)
#define TMC2209_MRES_MASK          (0x0Fu << TMC2209_MRES_SHIFT)

static uint8_t crc8(const uint8_t *data, unsigned len)
{
    uint8_t crc = 0;
    unsigned i;
    unsigned bit;
    uint8_t byte;

    for (i = 0; i < len; i++) {
        byte = data[i];
        for (bit = 0; bit < 8u; bit++) {
            if (((crc >> 7) ^ (byte & 0x01u)) != 0u)
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            else
                crc <<= 1;
            byte >>= 1;
        }
    }
    return crc;
}

static unsigned current_code_from_ma(unsigned current_ma)
{
    uint64_t denominator;
    uint64_t numerator;
    unsigned code;

    if (current_ma == 0u)
        return 0u;

    denominator = (uint64_t)TMC2209_VFS_MV * 1000000ull;
    numerator = (uint64_t)current_ma * 32ull *
        ((uint64_t)TMC2209_RSENSE_MOHM + 20ull) * 1414ull;
    code = (unsigned)((numerator + denominator / 2ull) / denominator);
    if (code > 0u)
        code--;
    if (code > 31u)
        code = 31u;
    return code;
}

static void uart_write_bytes(const uint8_t *data, unsigned len)
{
    unsigned i;

    for (i = 0; i < len; i++)
        DL_UART_Main_transmitDataBlocking(TMC_UART_INST, data[i]);
}

static bool write_register(uint8_t reg, uint32_t value)
{
    uint8_t frame[8];

    frame[0] = 0x05u;
    frame[1] = (uint8_t)TMC2209_SLAVE_ADDR;
    frame[2] = (uint8_t)(reg | 0x80u);
    frame[3] = (uint8_t)(value >> 24);
    frame[4] = (uint8_t)(value >> 16);
    frame[5] = (uint8_t)(value >> 8);
    frame[6] = (uint8_t)value;
    frame[7] = crc8(frame, 7u);

    uart_write_bytes(frame, sizeof(frame));
    return true;
}

bool TMC2209_SetCurrentCodes(unsigned ihold, unsigned irun)
{
    uint32_t ihold_irun;

    if (ihold > 31u || irun > 31u)
        return false;

    ihold_irun = (uint32_t)(ihold & 0x1Fu);
    ihold_irun |= (uint32_t)(irun & 0x1Fu) << 8;
    ihold_irun |= (uint32_t)(TMC2209_IHOLDDELAY_CODE & 0x0Fu) << 16;
    ihold_irun |= (uint32_t)(TMC2209_IRUNDELAY_CODE & 0x0Fu) << 24;

    return write_register(TMC2209_REG_IHOLD_IRUN, ihold_irun);
}

static unsigned mres_from_microsteps(unsigned microsteps)
{
    switch (microsteps) {
    case 256u: return 0u;
    case 128u: return 1u;
    case 64u:  return 2u;
    case 32u:  return 3u;
    case 16u:  return 4u;
    case 8u:   return 5u;
    case 4u:   return 6u;
    case 2u:   return 7u;
    case 1u:   return 8u;
    default:   return TMC2209_MRES_CODE & 0x0Fu;
    }
}

bool TMC2209_SetMicrosteps(unsigned microsteps)
{
    uint32_t chopconf;
    unsigned mres = mres_from_microsteps(microsteps);

    /* 先让 MRES 由寄存器决定（覆盖 MS 脚） */
    (void)write_register(TMC2209_REG_GCONF, TMC2209_GCONF_VALUE);

    chopconf = TMC2209_CHOPCONF_BASE & ~TMC2209_MRES_MASK;
    chopconf |= ((uint32_t)mres << TMC2209_MRES_SHIFT);
    return write_register(TMC2209_REG_CHOPCONF, chopconf);
}

void TMC2209_Init(void)
{
    /* 上电后给 TMC 内部时钟/UART 一点稳定时间 */
    delay_cycles(800000u); /* ~10 ms @ 80 MHz */

    /* UART 强制微步，覆盖 MS1/MS2 硬件脚 */
    (void)TMC2209_SetMicrosteps(STEPPER_MICROSTEPS);

    (void)TMC2209_SetCurrentCodes(
        current_code_from_ma(TMC2209_IHOLD_MA),
        current_code_from_ma(TMC2209_IRUN_MA));

    /* 再发一次，兼容上电时序偏慢的板子 */
    delay_cycles(160000u); /* ~2 ms */
    (void)TMC2209_SetMicrosteps(STEPPER_MICROSTEPS);
    (void)TMC2209_SetCurrentCodes(
        current_code_from_ma(TMC2209_IHOLD_MA),
        current_code_from_ma(TMC2209_IRUN_MA));
}

#else

bool TMC2209_SetCurrentCodes(unsigned ihold, unsigned irun)
{
    (void)ihold;
    (void)irun;
    return false;
}

bool TMC2209_SetMicrosteps(unsigned microsteps)
{
    (void)microsteps;
    return false;
}

void TMC2209_Init(void)
{
}

#endif
