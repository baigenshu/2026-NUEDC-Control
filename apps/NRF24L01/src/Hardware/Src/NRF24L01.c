#include "NRF24L01.h"
#include <stdio.h>

static void drv_delay_500Ms(unsigned int ms)
{
    while (ms--) {
        delay_ms(500);
    }
}

uint8_t NRF24L01_Read_Reg(uint8_t RegAddr)
{
    uint8_t btmp;

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(NRF_READ_REG | RegAddr);
    btmp = drv_spi_read_write_byte(0xFF);
    RF24L01_SET_CS_HIGH();

    return btmp;
}

void NRF24L01_Read_Buf(uint8_t RegAddr, uint8_t *pBuf, uint8_t len)
{
    uint8_t btmp;

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(NRF_READ_REG | RegAddr);
    for (btmp = 0; btmp < len; btmp++) {
        *(pBuf + btmp) = drv_spi_read_write_byte(0xFF);
    }
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Write_Reg(uint8_t RegAddr, uint8_t Value)
{
    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(NRF_WRITE_REG | RegAddr);
    drv_spi_read_write_byte(Value);
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Write_Buf(uint8_t RegAddr, uint8_t *pBuf, uint8_t len)
{
    uint8_t i;

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(NRF_WRITE_REG | RegAddr);
    for (i = 0; i < len; i++) {
        drv_spi_read_write_byte(*(pBuf + i));
    }
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Flush_Tx_Fifo(void)
{
    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(FLUSH_TX);
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Flush_Rx_Fifo(void)
{
    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(FLUSH_RX);
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Reuse_Tx_Payload(void)
{
    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(REUSE_TX_PL);
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Nop(void)
{
    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(NOP);
    RF24L01_SET_CS_HIGH();
}

uint8_t NRF24L01_Read_Status_Register(void)
{
    uint8_t Status;

    RF24L01_SET_CS_LOW();
    Status = drv_spi_read_write_byte(NRF_READ_REG + STATUS);
    RF24L01_SET_CS_HIGH();

    return Status;
}

uint8_t NRF24L01_Clear_IRQ_Flag(uint8_t IRQ_Source)
{
    uint8_t btmp = 0;

    IRQ_Source &= (1 << RX_DR) | (1 << TX_DS) | (1 << MAX_RT);
    btmp = NRF24L01_Read_Status_Register();

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(NRF_WRITE_REG + STATUS);
    drv_spi_read_write_byte(IRQ_Source | btmp);
    RF24L01_SET_CS_HIGH();

    return NRF24L01_Read_Status_Register();
}

uint8_t RF24L01_Read_IRQ_Status(void)
{
    return (NRF24L01_Read_Status_Register() &
            ((1 << RX_DR) | (1 << TX_DS) | (1 << MAX_RT)));
}

uint8_t NRF24L01_Read_Top_Fifo_Width(void)
{
    uint8_t btmp;

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(R_RX_PL_WID);
    btmp = drv_spi_read_write_byte(0xFF);
    RF24L01_SET_CS_HIGH();

    return btmp;
}

uint8_t NRF24L01_Read_Rx_Payload(uint8_t *pRxBuf)
{
    uint8_t Width, PipeNum;

    PipeNum = (NRF24L01_Read_Reg(STATUS) >> 1) & 0x07;
    Width = NRF24L01_Read_Top_Fifo_Width();

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(RD_RX_PLOAD);
    for (PipeNum = 0; PipeNum < Width; PipeNum++) {
        *(pRxBuf + PipeNum) = drv_spi_read_write_byte(0xFF);
    }
    RF24L01_SET_CS_HIGH();
    NRF24L01_Flush_Rx_Fifo();

    return Width;
}

void NRF24L01_Write_Tx_Payload_Ack(uint8_t *pTxBuf, uint8_t len)
{
    uint8_t btmp;
    uint8_t length = (len > 32) ? 32 : len;

    NRF24L01_Flush_Tx_Fifo();

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(WR_TX_PLOAD);
    for (btmp = 0; btmp < length; btmp++) {
        drv_spi_read_write_byte(*(pTxBuf + btmp));
    }
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Write_Tx_Payload_NoAck(uint8_t *pTxBuf, uint8_t len)
{
    if (len > 32 || len == 0) {
        return;
    }

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(WR_TX_PLOAD_NACK);
    while (len--) {
        drv_spi_read_write_byte(*pTxBuf);
        pTxBuf++;
    }
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Write_Tx_Payload_InAck(uint8_t *pData, uint8_t len)
{
    uint8_t btmp;

    len = (len > 32) ? 32 : len;

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(W_ACK_PLOAD);
    for (btmp = 0; btmp < len; btmp++) {
        drv_spi_read_write_byte(*(pData + btmp));
    }
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Set_TxAddr(uint8_t *pAddr, uint8_t len)
{
    len = (len > 5) ? 5 : len;
    NRF24L01_Write_Buf(TX_ADDR, pAddr, len);
}

void NRF24L01_Set_RxAddr(uint8_t PipeNum, uint8_t *pAddr, uint8_t Len)
{
    Len = (Len > 5) ? 5 : Len;
    PipeNum = (PipeNum > 5) ? 5 : PipeNum;
    NRF24L01_Write_Buf(RX_ADDR_P0 + PipeNum, pAddr, Len);
}

void NRF24L01_Set_Speed(nRf24l01SpeedType Speed)
{
    uint8_t btmp = 0;

    btmp = NRF24L01_Read_Reg(RF_SETUP);
    btmp &= ~((1 << 5) | (1 << 3));

    if (Speed == SPEED_250K) {
        btmp |= (1 << 5);
    } else if (Speed == SPEED_1M) {
        btmp &= ~((1 << 5) | (1 << 3));
    } else if (Speed == SPEED_2M) {
        btmp |= (1 << 3);
    }

    NRF24L01_Write_Reg(RF_SETUP, btmp);
}

void NRF24L01_Set_Power(nRf24l01PowerType Power)
{
    uint8_t btmp;

    btmp = NRF24L01_Read_Reg(RF_SETUP) & ~0x07;
    switch (Power) {
    case POWER_F18DBM:
        btmp |= PWR_18DB;
        break;
    case POWER_F12DBM:
        btmp |= PWR_12DB;
        break;
    case POWER_F6DBM:
        btmp |= PWR_6DB;
        break;
    case POWER_0DBM:
        btmp |= PWR_0DB;
        break;
    default:
        break;
    }
    NRF24L01_Write_Reg(RF_SETUP, btmp);
}

void RF24LL01_Write_Hopping_Point(uint8_t FreqPoint)
{
    NRF24L01_Write_Reg(RF_CH, FreqPoint & 0x7F);
}

void NRF24L01_check(void)
{
    uint8_t i;
    uint8_t error = 0;
    uint8_t buf[5] = {0xA5, 0xA5, 0xA5, 0xA5, 0xA5};
    uint8_t read_buf[5] = {0};

    while (1) {
        NRF24L01_Write_Buf(TX_ADDR, buf, 5);
        NRF24L01_Read_Buf(TX_ADDR, read_buf, 5);
        for (i = 0; i < 5; i++) {
            if (buf[i] != read_buf[i]) {
                break;
            }
        }

        if (5 == i) {
            break;
        } else {
            error++;
            if (error >= 3) {
                break;
            }
            printf("NRF24L01 ERROR  FILE:NRF24L01.C  LINE = %d\r\n", __LINE__);
        }
        drv_delay_500Ms(2);
    }
    printf("Successful configuration\r\n");
}

void RF24L01_Set_Mode(nRf24l01ModeType Mode)
{
    uint8_t controlreg = 0;
    controlreg = NRF24L01_Read_Reg(CONFIG);

    if (Mode == MODE_TX) {
        controlreg &= ~(1 << PRIM_RX);
    } else if (Mode == MODE_RX) {
        controlreg |= (1 << PRIM_RX);
    }

    NRF24L01_Write_Reg(CONFIG, controlreg);
}

uint8_t NRF24L01_TxPacket(uint8_t *txbuf, uint8_t Length)
{
    uint8_t l_Status = 0;
    uint16_t l_MsTimes = 0;

    RF24L01_SET_CS_LOW();
    drv_spi_read_write_byte(FLUSH_TX);
    RF24L01_SET_CS_HIGH();

    RF24L01_SET_CE_LOW();
    NRF24L01_Write_Buf(WR_TX_PLOAD, txbuf, Length);
    RF24L01_SET_CE_HIGH();
    while (0 != RF24L01_GET_IRQ_STATUS()) {
        delay_ms(5);
        if (500 == l_MsTimes++) {
            NRF24L01_Gpio_Init_transmit();
            RF24L01_Init();
            RF24L01_Set_Mode(MODE_TX);
            break;
        }
    }
    l_Status = NRF24L01_Read_Reg(STATUS);
    NRF24L01_Write_Reg(STATUS, l_Status);

    if (l_Status & MAX_TX) {
        NRF24L01_Write_Reg(FLUSH_TX, 0xff);
        return MAX_TX;
    }
    if (l_Status & TX_OK) {
        return TX_OK;
    }

    return 0xFF;
}

void NRF24L01_Gpio_Init_receive(void)
{
    NVIC_EnableIRQ(GPIO_INT_IRQN);
    RF24L01_SET_CE_LOW();
    RF24L01_SET_CS_HIGH();
}

void NRF24L01_Gpio_Init_transmit(void)
{
    NVIC_EnableIRQ(GPIO_INT_IRQN);
    RF24L01_SET_CE_LOW();
    RF24L01_SET_CS_HIGH();
}

uint8_t g_RF24L01RxBuffer[33];
volatile uint8_t g_rx_ready = 0;

void Buff_Clear(void)
{
    int i;
    for (i = 0; i < 33; i++) {
        g_RF24L01RxBuffer[i] = 0;
    }
    g_rx_ready = 0;
}

void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
    case GPIO_INT_IIDX:
        if (RF24L01_GET_IRQ_STATUS() == 0) {
            uint8_t n = NRF24L01_RxPacket(g_RF24L01RxBuffer);
            if (n > 0) {
                if (n > 32) {
                    n = 32;
                }
                g_RF24L01RxBuffer[n] = 0;
                g_rx_ready = 1;
            }
            NRF24L01_Flush_Rx_Fifo();
            RF24L01_SET_CE_HIGH();
        }
        DL_GPIO_clearInterruptStatus(GPIO_PORT, GPIO_IRQ_PIN);
        break;
    default:
        break;
    }
}

uint8_t NRF24L01_RxPacket(uint8_t *rxbuf)
{
    uint8_t l_Status = 0, l_RxLength = 0;

    l_Status = NRF24L01_Read_Reg(STATUS);
    NRF24L01_Write_Reg(STATUS, l_Status);
    if (l_Status & RX_OK) {
        l_RxLength = NRF24L01_Read_Top_Fifo_Width();
        if (l_RxLength > 32) {
            l_RxLength = 32;
        }
        NRF24L01_Read_Buf(RD_RX_PLOAD, rxbuf, l_RxLength);
        NRF24L01_Flush_Rx_Fifo();
        return l_RxLength;
    }
    return 0;
}

void RF24L01_Init(void)
{
    uint8_t addr[5] = {INIT_ADDR};

    RF24L01_SET_CE_HIGH();
    NRF24L01_Clear_IRQ_Flag(IRQ_ALL);
#if DYNAMIC_PACKET == 1
    NRF24L01_Write_Reg(DYNPD, (1 << 0));
    NRF24L01_Write_Reg(FEATRUE, 0x07);
    NRF24L01_Read_Reg(DYNPD);
    NRF24L01_Read_Reg(FEATRUE);
#elif DYNAMIC_PACKET == 0
    NRF24L01_Write_Reg(RX_PW_P0, FIXED_PACKET_LEN);
#endif

    NRF24L01_Write_Reg(CONFIG, (1 << EN_CRC) | (1 << PWR_UP));
    NRF24L01_Write_Reg(EN_AA, (1 << ENAA_P0));
    NRF24L01_Write_Reg(EN_RXADDR, (1 << ERX_P0));
    NRF24L01_Write_Reg(SETUP_AW, AW_5BYTES);
    NRF24L01_Write_Reg(SETUP_RETR, ARD_4000US | (REPEAT_CNT & 0x0F));
    NRF24L01_Write_Reg(RF_CH, 0);
    NRF24L01_Write_Reg(RF_SETUP, 0x26);

    NRF24L01_Set_TxAddr(&addr[0], 5);
    NRF24L01_Set_RxAddr(0, &addr[0], 5);
}
