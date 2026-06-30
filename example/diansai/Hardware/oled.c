#include "ti_msp_dl_config.h"
#include "OLED_Font.h"

/* ---- GPIO pin definitions (PB14=CS, PB11=DC, PB10=RES) ---- */
#define OLED_CS_PORT    GPIOB
#define OLED_CS_PIN     DL_GPIO_PIN_14
#define OLED_DC_PORT    GPIOB
#define OLED_DC_PIN     DL_GPIO_PIN_11
#define OLED_RES_PORT   GPIOB
#define OLED_RES_PIN    DL_GPIO_PIN_10

#define OLED_CS_IOMUX   (IOMUX_PINCM31)
#define OLED_DC_IOMUX   (IOMUX_PINCM28)
#define OLED_RES_IOMUX  (IOMUX_PINCM27)

/* ---- Pin control macros ---- */
#define OLED_CS_LOW()   DL_GPIO_clearPins(OLED_CS_PORT, OLED_CS_PIN)
#define OLED_CS_HIGH()  DL_GPIO_setPins(OLED_CS_PORT, OLED_CS_PIN)
#define OLED_DC_LOW()   DL_GPIO_clearPins(OLED_DC_PORT, OLED_DC_PIN)
#define OLED_DC_HIGH()  DL_GPIO_setPins(OLED_DC_PORT, OLED_DC_PIN)
#define OLED_RES_LOW()  DL_GPIO_clearPins(OLED_RES_PORT, OLED_RES_PIN)
#define OLED_RES_HIGH() DL_GPIO_setPins(OLED_RES_PORT, OLED_RES_PIN)

/* ---- Send one byte via SPI ---- */
static void OLED_SPI_WriteByte(uint8_t data)
{
    DL_SPI_transmitDataBlocking8(SPI_OLED_INST, data);
}

static void OLED_WriteCommand(uint8_t cmd)
{
    OLED_DC_LOW();
    OLED_CS_LOW();
    OLED_SPI_WriteByte(cmd);
    OLED_CS_HIGH();
}

static void OLED_WriteData(uint8_t data)
{
    OLED_DC_HIGH();
    OLED_CS_LOW();
    OLED_SPI_WriteByte(data);
    OLED_CS_HIGH();
}

/* ---- Display functions (same API as before) ---- */

void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    X += 2;  /* offset 2 pixels right to avoid edge noise */
    OLED_WriteCommand(0xB0 | Y);
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (X & 0x0F));
}

void OLED_Clear(void)
{
    uint8_t i, n;
    for (i = 0; i < 8; i++)
    {
        /* Match manufacturer: start from column 0x02, write 128 bytes
         * (wraps columns 2..127→0..1, covering all 128 physical columns) */
        OLED_WriteCommand(0xB0 + i);
        OLED_WriteCommand(0x02);  /* lower column = 0x02 */
        OLED_WriteCommand(0x10);  /* higher column = 0x10 */
        for (n = 0; n < 128; n++)
            OLED_WriteData(0x00);
    }
}

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i]);
    }
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
    }
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--)
    {
        Result *= X;
    }
    return Result;
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i,
            Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    uint8_t i;
    uint32_t Number1;
    if (Number >= 0)
    {
        OLED_ShowChar(Line, Column, '+');
        Number1 = Number;
    }
    else
    {
        OLED_ShowChar(Line, Column, '-');
        Number1 = -Number;
    }
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i + 1,
            Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i, SingleNumber;
    for (i = 0; i < Length; i++)
    {
        SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
        if (SingleNumber < 10)
        {
            OLED_ShowChar(Line, Column + i, SingleNumber + '0');
        }
        else
        {
            OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
        }
    }
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i,
            Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
    }
}

void OLED_Init(void)
{
    uint32_t i, j;

    /* Hardware reset (GPIO init done by SYSCFG_DL_GPIO_init,
     * SPI init done by SYSCFG_DL_SPI_OLED_init) */
    OLED_RES_HIGH();
    for (i = 0; i < 1000; i++)
        for (j = 0; j < 1000; j++);
    OLED_RES_LOW();
    for (i = 0; i < 1000; i++)
        for (j = 0; j < 1000; j++);
    OLED_RES_HIGH();
    for (i = 0; i < 1000; i++)
        for (j = 0; j < 1000; j++);

    /* SSD1306 init sequence — matched to ZJY (中景园) 1.3" panel */
    OLED_WriteCommand(0xAE);   /* display off */
    OLED_WriteCommand(0x02);   /* set lower column address = 0x02 */
    OLED_WriteCommand(0x10);   /* set higher column address = 0x10 */
    OLED_WriteCommand(0x40);   /* set display start line */
    OLED_WriteCommand(0xB0);   /* set page address */
    OLED_WriteCommand(0x81);   /* contrast control */
    OLED_WriteCommand(0xCF);   /* 128 */
    OLED_WriteCommand(0xA1);   /* segment remap */
    OLED_WriteCommand(0xA6);   /* normal display (not inverse) */
    OLED_WriteCommand(0xA8);   /* multiplex ratio */
    OLED_WriteCommand(0x3F);   /* duty = 1/64 */
    OLED_WriteCommand(0xAD);   /* DC-DC control */
    OLED_WriteCommand(0x8B);   /* DC-DC ON, internal VCC */
    OLED_WriteCommand(0x33);   /* VPP = 9V */
    OLED_WriteCommand(0xC8);   /* COM scan direction */
    OLED_WriteCommand(0xD3);   /* display offset */
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0xD5);   /* oscillator division */
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xD9);   /* pre-charge period */
    OLED_WriteCommand(0x1F);   /* Phase1=1, Phase2=15 */
    OLED_WriteCommand(0xDA);   /* COM pins configuration */
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0xDB);   /* VCOMH deselect level */
    OLED_WriteCommand(0x40);

    OLED_Clear();
    OLED_WriteCommand(0xAF);   /* display ON */
}