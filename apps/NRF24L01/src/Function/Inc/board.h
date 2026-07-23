#ifndef BOARD_H
#define BOARD_H

#include "ti_msp_dl_config.h"
#include "delay.h"
#include "uart.h"

static inline void board_init(void)
{
    SYSCFG_DL_init();
}

#endif /* BOARD_H */
