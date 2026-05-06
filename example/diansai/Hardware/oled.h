#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define OLED_CMD  0
#define OLED_DATA 1

#define OLED_SSD1306_SCL_PORT     GPIO_OLED_PORT
#define OLED_SSD1306_SCL_PIN_NUM  GPIO_OLED_SCL_OLED_PIN
#define OLED_SSD1306_SCL_IOMUX    GPIO_OLED_SCL_OLED_IOMUX

#define OLED_SSD1306_SDA_PORT     GPIO_OLED_PORT
#define OLED_SSD1306_SDA_PIN_NUM  GPIO_OLED_SDA_OLED_PIN
#define OLED_SSD1306_SDA_IOMUX    GPIO_OLED_SDA_OLED_IOMUX

#define OLED_SSD1306_RES_PORT     GPIO_OLED_PORT
#define OLED_SSD1306_RES_PIN_NUM  GPIO_OLED_RST_PIN
#define OLED_SSD1306_RES_IOMUX    GPIO_OLED_RST_IOMUX

#define OLED_SSD1306_DC_PORT      GPIO_OLED_PORT
#define OLED_SSD1306_DC_PIN_NUM   GPIO_OLED_DC_PIN
#define OLED_SSD1306_DC_IOMUX     GPIO_OLED_DC_IOMUX

#define OLED_SSD1306_CS_PORT      GPIO_OLED_PORT
#define OLED_SSD1306_CS_PIN_NUM   GPIO_OLED_CS_PIN
#define OLED_SSD1306_CS_IOMUX     GPIO_OLED_CS_IOMUX

#define OLED_SSD1306_SCL_IO_INIT  (DL_GPIO_initDigitalOutput(OLED_SSD1306_SCL_IOMUX))
#define OLED_SCL_Set()            (DL_GPIO_setPins(OLED_SSD1306_SCL_PORT, OLED_SSD1306_SCL_PIN_NUM))
#define OLED_SCL_Clr()            (DL_GPIO_clearPins(OLED_SSD1306_SCL_PORT, OLED_SSD1306_SCL_PIN_NUM))

#define OLED_SSD1306_SDA_IO_INIT  (DL_GPIO_initDigitalOutput(OLED_SSD1306_SDA_IOMUX))
#define OLED_SDA_Set()            (DL_GPIO_setPins(OLED_SSD1306_SDA_PORT, OLED_SSD1306_SDA_PIN_NUM))
#define OLED_SDA_Clr()            (DL_GPIO_clearPins(OLED_SSD1306_SDA_PORT, OLED_SSD1306_SDA_PIN_NUM))

#define OLED_SSD1306_RES_IO_INIT  (DL_GPIO_initDigitalOutput(OLED_SSD1306_RES_IOMUX))
#define OLED_RES_Set()            (DL_GPIO_setPins(OLED_SSD1306_RES_PORT, OLED_SSD1306_RES_PIN_NUM))
#define OLED_RES_Clr()            (DL_GPIO_clearPins(OLED_SSD1306_RES_PORT, OLED_SSD1306_RES_PIN_NUM))

#define OLED_SSD1306_DC_IO_INIT   (DL_GPIO_initDigitalOutput(OLED_SSD1306_DC_IOMUX))
#define OLED_DC_Set()             (DL_GPIO_setPins(OLED_SSD1306_DC_PORT, OLED_SSD1306_DC_PIN_NUM))
#define OLED_DC_Clr()             (DL_GPIO_clearPins(OLED_SSD1306_DC_PORT, OLED_SSD1306_DC_PIN_NUM))

#define OLED_SSD1306_CS_IO_INIT   (DL_GPIO_initDigitalOutput(OLED_SSD1306_CS_IOMUX))
#define OLED_CS_Set()             (DL_GPIO_setPins(OLED_SSD1306_CS_PORT, OLED_SSD1306_CS_PIN_NUM))
#define OLED_CS_Clr()             (DL_GPIO_clearPins(OLED_SSD1306_CS_PORT, OLED_SSD1306_CS_PIN_NUM))

void delay_ms(unsigned int ms);
void OLED_ColorTurn(u8 i);
void OLED_DisplayTurn(u8 i);
void OLED_WR_Byte(u8 dat, u8 cmd);
void OLED_Set_Pos(u8 x, u8 y);
void OLED_Display_On(void);
void OLED_Display_Off(void);
void OLED_Clear(void);
void OLED_ShowChar(u8 x, u8 y, u8 chr, u8 sizey);
u32 oled_pow(u8 m, u8 n);
void OLED_ShowNum(u8 x, u8 y, u32 num, u8 len, u8 sizey);
void OLED_ShowString(u8 x, u8 y, u8 *chr, u8 sizey);
void OLED_ShowChinese(u8 x, u8 y, u8 no, u8 sizey);
void OLED_DrawBMP(u8 x, u8 y, u8 sizex, u8 sizey, u8 BMP[]);
void OLED_Init(void);

#endif




