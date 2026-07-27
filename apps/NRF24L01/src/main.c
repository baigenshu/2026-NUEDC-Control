#include "board.h"
#include <stdio.h>
#include "NRF24L01.h"
#include "drv_spi.h"

/* set by IRQ when a packet is received */
extern volatile uint8_t g_rx_ready;
extern uint8_t g_RF24L01RxBuffer[33];

int main(void)
{
    board_init();
    /* PA7 = module VCC */
    delay_ms(50);
    drv_spi_init();

    NRF24L01_Gpio_Init_receive();
    NRF24L01_check();
    RF24L01_Init();
    RF24L01_Set_Mode(MODE_RX);
    RF24L01_SET_CE_HIGH();

    printf("NRF24L01 RX ready, UART0 115200 (PA28 TX)\r\n");

    while (1) {
        if (g_rx_ready) {
            g_rx_ready = 0;
            printf("RX: %s\r\n", (char *)g_RF24L01RxBuffer);
            Buff_Clear();
        }

        /* poll STATUS if IRQ missed */
        if (RF24L01_GET_IRQ_STATUS() == 0) {
            uint8_t st = NRF24L01_Read_Reg(STATUS);
            if (st & RX_OK) {
                uint8_t n = NRF24L01_RxPacket(g_RF24L01RxBuffer);
                if (n > 0) {
                    if (n > 32) {
                        n = 32;
                    }
                    g_RF24L01RxBuffer[n] = 0;
                    printf("RX: %s\r\n", (char *)g_RF24L01RxBuffer);
                    Buff_Clear();
                }
                RF24L01_SET_CE_HIGH();
            }
        }

        delay_ms(10);
    }
}
