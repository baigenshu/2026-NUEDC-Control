#include "drv_spi.h"

void drv_spi_init(void)
{
}

uint8_t drv_spi_read_write_byte(uint8_t TxByte)
{
    uint16_t timeOut = 1000;

    while (DL_SPI_isBusy(SPI_INST)) {
        if (timeOut-- == 0) {
            break;
        }
    }

    DL_SPI_transmitData8(SPI_INST, TxByte);

    timeOut = 1000;
    while (DL_SPI_isRXFIFOEmpty(SPI_INST)) {
        if (timeOut-- == 0) {
            return 0xFF;
        }
    }

    return DL_SPI_receiveData8(SPI_INST);
}

void drv_spi_read_write_string(uint8_t *ReadBuffer, uint8_t *WriteBuffer, uint16_t Length)
{
    spi_set_nss_low();
    while (Length--) {
        *ReadBuffer = drv_spi_read_write_byte(*WriteBuffer);
        ReadBuffer++;
        WriteBuffer++;
    }
    spi_set_nss_high();
}
