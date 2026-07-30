/**
 * @file line_track.h
 * @brief 8 路灰度巡线 → Chassis_Arcade
 */
#ifndef LINE_TRACK_H
#define LINE_TRACK_H

#include <stdint.h>
#include <stdbool.h>

void    LineTrack_Init(void);
void    LineTrack_SetEnable(bool on);
bool    LineTrack_IsEnabled(void);
void    LineTrack_SetBaseSpeed(int16_t pct);
void    LineTrack_Reset(void);
/** 使能时读灰度、算误差、输出差速 */
void    LineTrack_Update(void);
int32_t LineTrack_GetError(void);
uint8_t LineTrack_GetMask(void);

#endif /* LINE_TRACK_H */
