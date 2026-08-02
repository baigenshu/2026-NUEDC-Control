/**
 * @file ir4.h
 * @brief 四路红外循迹（G1–G4 · 高有效黑线）
 *
 * 左→右 p1..p4：PB19, PB17, PA16, PA14
 */
#ifndef IR4_H
#define IR4_H

#include <stdint.h>

#define IR4_CH_COUNT  (4u)

void Ir4_Init(void);

/** bit0=p1 … bit3=p4，黑线=1 */
uint8_t Ir4_ReadMask(void);

/** s[0..3] = p1..p4，黑线=1 */
void Ir4_ReadRaw(uint8_t s[IR4_CH_COUNT]);

#endif /* IR4_H */
