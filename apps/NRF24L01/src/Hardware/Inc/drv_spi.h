#ifndef DRV_SPI_H
#define DRV_SPI_H

#include "board.h"

#define spi_set_nss_high() DL_GPIO_setPins(GPIO_PORT, GPIO_CS_PIN)
#define spi_set_nss_low()  DL_GPIO_clearPins(GPIO_PORT, GPIO_CS_PIN)

void drv_spi_init(void);
uint8_t drv_spi_read_write_byte(uint8_t TxByte);
void drv_spi_read_write_string(uint8_t *ReadBuffer, uint8_t *WriteBuffer, uint16_t Length);

#endif /* DRV_SPI_H */
