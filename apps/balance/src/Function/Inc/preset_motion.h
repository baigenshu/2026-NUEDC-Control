#ifndef PRESET_MOTION_H
#define PRESET_MOTION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PRESET_MOTION_IDLE = 0,
    PRESET_MOTION_ARMING_ZERO,
    PRESET_MOTION_POSITIVE_PUSH,
    PRESET_MOTION_POSITIVE_BRAKE,
    PRESET_MOTION_NEGATIVE_PUSH,
    PRESET_MOTION_NEGATIVE_BRAKE,
    PRESET_MOTION_FINAL_SETTLE,
    PRESET_MOTION_COMPLETE,
    PRESET_MOTION_TIMEOUT,
} preset_motion_state_t;

void PresetMotion_Init(void);
void PresetMotion_Start(uint32_t now_ms);
void PresetMotion_Cancel(void);
void PresetMotion_Update(uint32_t now_ms);

preset_motion_state_t PresetMotion_GetState(void);
uint32_t PresetMotion_GetElapsedMs(uint32_t now_ms);
bool PresetMotion_IsActive(void);
bool PresetMotion_IsFinished(void);

#endif /* PRESET_MOTION_H */