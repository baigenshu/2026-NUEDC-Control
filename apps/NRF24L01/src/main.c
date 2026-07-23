#include "board.h"
#include <stdio.h>
#include "NRF24L01.h"
#include "drv_spi.h"

/* 1: RX mode  0: TX mode */
#define RECEIVING_MODE 1

extern uint8_t g_RF24L01RxBuffer[30];

int main(void)
{
    board_init();
    drv_spi_init();

#if RECEIVING_MODE
    NRF24L01_Gpio_Init_receive();
    NRF24L01_check();
    RF24L01_Init();
    RF24L01_Set_Mode(MODE_RX);
    printf("MODE_RX\r\n");

    while (1) {
        if (0 != g_RF24L01RxBuffer[0]) {
            printf("Data = %s\r\n", g_RF24L01RxBuffer);
            Buff_Clear();
        }
        delay_ms(50);
    }
#else
    NRF24L01_Gpio_Init_transmit();
    NRF24L01_check();
    RF24L01_Init();
    RF24L01_Set_Mode(MODE_TX);
    printf("MODE_TX\r\n");

    while (1) {
        NRF24L01_TxPacket((uint8_t *)"hello LCKFB!\r\n", 13);
        printf("Send\r\n");
        delay_ms(100);
    }
#endif
}
