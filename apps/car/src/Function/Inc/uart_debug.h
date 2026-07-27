#ifndef __UART_DEBUG_H
#define __UART_DEBUG_H

#include <stdint.h>

/* UART0 PA10 TX / PA11 RX @115200
 * 文本: Printf / FireWater(ch0,ch1,...\n)
 * 波形: JustFloat → VOFA+ 选 JustFloat 协议
 */
void UartDebug_Init(void);
void UartDebug_Write(const char *s);
void UartDebug_WriteBuf(const uint8_t *p, uint16_t n);
void UartDebug_Printf(const char *fmt, ...);

/* VOFA+ JustFloat: n 个 float 小端 + 帧尾 00 00 80 7F，n 建议 ≤16 */
void UartDebug_JustFloat(const float *data, uint8_t n);

/* VOFA+ FireWater: 逗号分隔 + \n，适合少量通道 */
void UartDebug_FireWater(const float *data, uint8_t n);

#endif
