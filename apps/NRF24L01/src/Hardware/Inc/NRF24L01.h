#ifndef NRF24L01_H
#define NRF24L01_H

#include "drv_spi.h"

#define DYNAMIC_PACKET   1
#define FIXED_PACKET_LEN 32
#define REPEAT_CNT       15
#define INIT_ADDR        0x34, 0x43, 0x10, 0x10, 0x01

#define RF24L01_SET_CE_HIGH() DL_GPIO_setPins(GPIO_PORT, GPIO_CE_PIN)
#define RF24L01_SET_CE_LOW()  DL_GPIO_clearPins(GPIO_PORT, GPIO_CE_PIN)

#define RF24L01_SET_CS_HIGH() spi_set_nss_high()
#define RF24L01_SET_CS_LOW()  spi_set_nss_low()

/* nRF24L01 IRQ is active-low: 0 = asserted */
#define RF24L01_GET_IRQ_STATUS() \
    (((DL_GPIO_readPins(GPIO_PORT, GPIO_IRQ_PIN) & GPIO_IRQ_PIN) > 0) ? 1 : 0)

typedef enum ModeType {
    MODE_TX = 0,
    MODE_RX
} nRf24l01ModeType;

typedef enum SpeedType {
    SPEED_250K = 0,
    SPEED_1M,
    SPEED_2M
} nRf24l01SpeedType;

typedef enum PowerType {
    POWER_F18DBM = 0,
    POWER_F12DBM,
    POWER_F6DBM,
    POWER_0DBM
} nRf24l01PowerType;

#define NRF_READ_REG     0x00
#define NRF_WRITE_REG    0x20
#define RD_RX_PLOAD      0x61
#define WR_TX_PLOAD      0xA0
#define FLUSH_TX         0xE1
#define FLUSH_RX         0xE2
#define REUSE_TX_PL      0xE3
#define R_RX_PL_WID      0x60
#define NOP              0xFF
#define W_ACK_PLOAD      0xA8
#define WR_TX_PLOAD_NACK 0xB0

#define CONFIG           0x00
#define EN_AA            0x01
#define EN_RXADDR        0x02
#define SETUP_AW         0x03
#define SETUP_RETR       0x04
#define RF_CH            0x05
#define RF_SETUP         0x06
#define STATUS           0x07
#define MAX_TX           0x10
#define TX_OK            0x20
#define RX_OK            0x40
#define OBSERVE_TX       0x08
#define CD               0x09
#define RX_ADDR_P0       0x0A
#define RX_ADDR_P1       0x0B
#define RX_ADDR_P2       0x0C
#define RX_ADDR_P3       0x0D
#define RX_ADDR_P4       0x0E
#define RX_ADDR_P5       0x0F
#define TX_ADDR          0x10
#define RX_PW_P0         0x11
#define RX_PW_P1         0x12
#define RX_PW_P2         0x13
#define RX_PW_P3         0x14
#define RX_PW_P4         0x15
#define RX_PW_P5         0x16
#define NRF_FIFO_STATUS  0x17
#define DYNPD            0x1C
#define FEATRUE          0x1D

#define MASK_RX_DR  6
#define MASK_TX_DS  5
#define MASK_MAX_RT 4
#define EN_CRC      3
#define CRCO        2
#define PWR_UP      1
#define PRIM_RX     0

#define ENAA_P5 5
#define ENAA_P4 4
#define ENAA_P3 3
#define ENAA_P2 2
#define ENAA_P1 1
#define ENAA_P0 0

#define ERX_P5 5
#define ERX_P4 4
#define ERX_P3 3
#define ERX_P2 2
#define ERX_P1 1
#define ERX_P0 0

#define AW_RERSERVED 0x0
#define AW_3BYTES    0x1
#define AW_4BYTES    0x2
#define AW_5BYTES    0x3

#define ARD_250US  (0x00 << 4)
#define ARD_500US  (0x01 << 4)
#define ARD_750US  (0x02 << 4)
#define ARD_1000US (0x03 << 4)
#define ARD_2000US (0x07 << 4)
#define ARD_4000US (0x0F << 4)
#define ARC_DISABLE 0x00
#define ARC_15      0x0F

#define PWR_18DB (0x00 << 1)
#define PWR_12DB (0x01 << 1)
#define PWR_6DB  (0x02 << 1)
#define PWR_0DB  (0x03 << 1)

#define RX_DR  6
#define TX_DS  5
#define MAX_RT 4

#define IRQ_ALL ((1 << RX_DR) | (1 << TX_DS) | (1 << MAX_RT))

uint8_t NRF24L01_Read_Reg(uint8_t RegAddr);
void NRF24L01_Read_Buf(uint8_t RegAddr, uint8_t *pBuf, uint8_t len);
void NRF24L01_Write_Reg(uint8_t RegAddr, uint8_t Value);
void NRF24L01_Write_Buf(uint8_t RegAddr, uint8_t *pBuf, uint8_t len);
void NRF24L01_Flush_Tx_Fifo(void);
void NRF24L01_Flush_Rx_Fifo(void);
void NRF24L01_Reuse_Tx_Payload(void);
void NRF24L01_Nop(void);
uint8_t NRF24L01_Read_Status_Register(void);
uint8_t NRF24L01_Clear_IRQ_Flag(uint8_t IRQ_Source);
uint8_t RF24L01_Read_IRQ_Status(void);
uint8_t NRF24L01_Read_Top_Fifo_Width(void);
uint8_t NRF24L01_Read_Rx_Payload(uint8_t *pRxBuf);
void NRF24L01_Write_Tx_Payload_Ack(uint8_t *pTxBuf, uint8_t len);
void NRF24L01_Write_Tx_Payload_NoAck(uint8_t *pTxBuf, uint8_t len);
void NRF24L01_Write_Tx_Payload_InAck(uint8_t *pData, uint8_t len);
void NRF24L01_Set_TxAddr(uint8_t *pAddr, uint8_t len);
void NRF24L01_Set_RxAddr(uint8_t PipeNum, uint8_t *pAddr, uint8_t Len);
void NRF24L01_Set_Speed(nRf24l01SpeedType Speed);
void NRF24L01_Set_Power(nRf24l01PowerType Power);
void RF24LL01_Write_Hopping_Point(uint8_t FreqPoint);
void RF24L01_Set_Mode(nRf24l01ModeType Mode);
void NRF24L01_check(void);
uint8_t NRF24L01_TxPacket(uint8_t *txbuf, uint8_t Length);
uint8_t NRF24L01_RxPacket(uint8_t *rxbuf);
void NRF24L01_Gpio_Init_transmit(void);
void NRF24L01_Gpio_Init_receive(void);
void RF24L01_Init(void);
void Buff_Clear(void);

#endif /* NRF24L01_H */
