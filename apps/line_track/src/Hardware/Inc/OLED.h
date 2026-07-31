/**
 * @file OLED.h
 * @brief 0.96" SSD1306 SPI OLED（128×64，8×16 字模）
 *
 * 总线：SPI1 · PB9 SCLK / PB8 PICO · CS/DC/RES 见 pins.md
 * 行 1..4 · 列 1..16
 */
#ifndef OLED_H
#define OLED_H

#include <stdint.h>

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t line, uint8_t column, char c);
void OLED_ShowString(uint8_t line, uint8_t column, const char *s);
void OLED_ShowNum(uint8_t line, uint8_t column, uint32_t num, uint8_t len);
void OLED_ShowSignedNum(uint8_t line, uint8_t column, int32_t num, uint8_t len);
void OLED_ShowHexNum(uint8_t line, uint8_t column, uint32_t num, uint8_t len);

#endif /* OLED_H */
