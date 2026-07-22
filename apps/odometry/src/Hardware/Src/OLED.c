/* OLED removed on odometry LQFP-48 board — use UART0 debug instead. */
#include "OLED.h"

void OLED_Init(void) {}
void OLED_Clear(void) {}
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    (void)Line; (void)Column; (void)Char;
}
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    (void)Line; (void)Column; (void)String;
}
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    (void)Line; (void)Column; (void)Number; (void)Length;
}
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    (void)Line; (void)Column; (void)Number; (void)Length;
}
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    (void)Line; (void)Column; (void)Number; (void)Length;
}
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    (void)Line; (void)Column; (void)Number; (void)Length;
}
