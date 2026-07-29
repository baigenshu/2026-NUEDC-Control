/**
 * @file OLED.c
 * @brief OLED SPI 驱动（硬件 SPI1，软件 CS）
 *
 * 板载模组多为 SH1106 兼容（GDDRAM 132 列，可见 128 列从 col=2 起）。
 * 症状「方向对、但第 1 列像素跑到最右侧」= 未加列偏移 / 未清满 132 列。
 */
#include "OLED.h"
#include "OLED_Font.h"
#include "ti_msp_dl_config.h"

#define OLED_CS_H()  DL_GPIO_setPins(SPI_OLED_CTRL_PORT, SPI_OLED_CTRL_CS_PIN)
#define OLED_CS_L()  DL_GPIO_clearPins(SPI_OLED_CTRL_PORT, SPI_OLED_CTRL_CS_PIN)
#define OLED_DC_H()  DL_GPIO_setPins(SPI_OLED_CTRL_PORT, SPI_OLED_CTRL_DC_PIN)
#define OLED_DC_L()  DL_GPIO_clearPins(SPI_OLED_CTRL_PORT, SPI_OLED_CTRL_DC_PIN)
#define OLED_RES_H() DL_GPIO_setPins(SPI_OLED_CTRL_PORT, SPI_OLED_CTRL_RES_PIN)
#define OLED_RES_L() DL_GPIO_clearPins(SPI_OLED_CTRL_PORT, SPI_OLED_CTRL_RES_PIN)

/* 可见宽度 128；SH1106 内部 132。纯 SSD1306 可把 OFFSET 改为 0、GDRAM 改为 128 */
#ifndef OLED_COL_OFFSET
#define OLED_COL_OFFSET   (2u)
#endif
#ifndef OLED_GDRAM_COLS
#define OLED_GDRAM_COLS   (132u)
#endif
#define OLED_WIDTH        (128u)

static void delay_ms(uint32_t ms)
{
    volatile uint32_t i;
    while (ms--) {
        for (i = 0; i < 8000u; ++i)
            ;
    }
}

static void spi_write(uint8_t byte)
{
    /* SPI1 = PICO 单向（无 POCI），只等 TX，禁止等 RX */
    DL_SPI_transmitDataBlocking8(SPI_OLED_INST, byte);
    while (DL_SPI_isBusy(SPI_OLED_INST)) {
    }
}

static void write_cmd(uint8_t cmd)
{
    OLED_DC_L();
    OLED_CS_L();
    spi_write(cmd);
    OLED_CS_H();
}

/**
 * 页寻址：先页，再列低地址、列高地址（SH1106 常用顺序）。
 * @param x  可见区 X（0..127）；内部自动 +OLED_COL_OFFSET
 */
static void set_cursor(uint8_t page, uint8_t x)
{
    uint8_t col = (uint8_t)(x + OLED_COL_OFFSET);

    write_cmd((uint8_t)(0xB0u | (page & 0x07u)));
    write_cmd((uint8_t)(0x00u | (col & 0x0Fu)));          /* 列地址低 4 位 */
    write_cmd((uint8_t)(0x10u | ((col >> 4) & 0x0Fu)));    /* 列地址高 4 位 */
}

/** 从 GDDRAM 绝对列 0 起写（用于整页清 132 列，不做可见区偏移） */
static void set_cursor_abs(uint8_t page, uint8_t gdram_col)
{
    write_cmd((uint8_t)(0xB0u | (page & 0x07u)));
    write_cmd((uint8_t)(0x00u | (gdram_col & 0x0Fu)));
    write_cmd((uint8_t)(0x10u | ((gdram_col >> 4) & 0x0Fu)));
}

void OLED_Init(void)
{
    OLED_CS_H();
    OLED_DC_H();
    OLED_RES_H();

    OLED_RES_L();
    delay_ms(20);
    OLED_RES_H();
    delay_ms(20);

    write_cmd(0xAE); /* display off */
    write_cmd(0xD5);
    write_cmd(0x80); /* clock */
    write_cmd(0xA8);
    write_cmd(0x3F); /* multiplex 1/64 */
    write_cmd(0xD3);
    write_cmd(0x00); /* display offset */
    write_cmd(0x40); /* start line */
    /* 列偏移修好后若左右颠倒：A0↔A1；上下颠倒：C0↔C8 */
    write_cmd(0xA1); /* segment remap：列地址增大 → 屏右侧（纠正左右反） */
    write_cmd(0xC8);
    write_cmd(0xDA);
    write_cmd(0x12); /* COM pins */
    write_cmd(0x81);
    write_cmd(0xCF); /* contrast */
    write_cmd(0xD9);
    write_cmd(0xF1); /* precharge */
    write_cmd(0xDB);
    write_cmd(0x40); /* vcomh */
    write_cmd(0xA4); /* resume RAM */
    write_cmd(0xA6); /* normal */
    write_cmd(0x8D);
    write_cmd(0x14); /* charge pump on */
    /* 默认列指针落到可见区起点，避免上电后从 col0 写把首列甩到边缘 */
    write_cmd((uint8_t)(0x00u | (OLED_COL_OFFSET & 0x0Fu)));
    write_cmd((uint8_t)(0x10u | ((OLED_COL_OFFSET >> 4) & 0x0Fu)));
    write_cmd(0xAF); /* display on */

    OLED_Clear();
}

void OLED_Clear(void)
{
    uint8_t page, col;

    /* 必须清满 GDDRAM 132 列；只清 128 会在最右/最左留竖条 */
    for (page = 0; page < 8u; ++page) {
        set_cursor_abs(page, 0);
        OLED_DC_H();
        OLED_CS_L();
        for (col = 0; col < OLED_GDRAM_COLS; ++col)
            spi_write(0x00);
        OLED_CS_H();
    }
}

void OLED_ShowChar(uint8_t line, uint8_t column, char c)
{
    uint8_t i;
    uint8_t idx;
    uint8_t page;
    uint8_t x;

    if (line < 1u || line > 4u || column < 1u || column > 16u)
        return;
    if (c < ' ' || c > '~')
        c = '?';
    idx  = (uint8_t)(c - ' ');
    page = (uint8_t)((line - 1u) * 2u);
    x    = (uint8_t)((column - 1u) * 8u);

    set_cursor(page, x);
    OLED_DC_H();
    OLED_CS_L();
    for (i = 0; i < 8u; ++i)
        spi_write(OLED_F8x16[idx][i]);
    OLED_CS_H();

    set_cursor((uint8_t)(page + 1u), x);
    OLED_DC_H();
    OLED_CS_L();
    for (i = 0; i < 8u; ++i)
        spi_write(OLED_F8x16[idx][i + 8u]);
    OLED_CS_H();
}

void OLED_ShowString(uint8_t line, uint8_t column, const char *s)
{
    uint8_t col = column;

    if (!s)
        return;
    while (*s != '\0' && col <= 16u) {
        OLED_ShowChar(line, col, *s++);
        col++;
    }
}

static uint32_t pow10u(uint8_t n)
{
    uint32_t r = 1u;
    while (n--)
        r *= 10u;
    return r;
}

void OLED_ShowNum(uint8_t line, uint8_t column, uint32_t num, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < len; ++i) {
        uint32_t div = pow10u((uint8_t)(len - 1u - i));
        OLED_ShowChar(line, (uint8_t)(column + i),
                      (char)('0' + (num / div) % 10u));
    }
}

void OLED_ShowSignedNum(uint8_t line, uint8_t column, int32_t num, uint8_t len)
{
    if (num < 0) {
        OLED_ShowChar(line, column, '-');
        OLED_ShowNum(line, (uint8_t)(column + 1u), (uint32_t)(-num), len);
    } else {
        OLED_ShowChar(line, column, '+');
        OLED_ShowNum(line, (uint8_t)(column + 1u), (uint32_t)num, len);
    }
}

void OLED_ShowHexNum(uint8_t line, uint8_t column, uint32_t num, uint8_t len)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t i;

    for (i = 0; i < len; ++i) {
        uint8_t shift = (uint8_t)((len - 1u - i) * 4u);
        OLED_ShowChar(line, (uint8_t)(column + i),
                      hex[(num >> shift) & 0xFu]);
    }
}
