/**
 * @file lap_task.h
 * @brief H 题第 2 项：A 点一圈巡线任务
 *
 * 置于 A 点 → B21 启动计时 → 顺时针黑线一圈 → 停车基准线停 → 冻结总时间
 */
#ifndef LAP_TASK_H
#define LAP_TASK_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LAP_STATE_WAIT = 0,
    LAP_STATE_RUN,
    LAP_STATE_DONE,
    LAP_STATE_TIMEOUT,
    LAP_STATE_ABORTED,
} lap_state_t;

void        LapTask_Init(void);
void        LapTask_Start(uint32_t now_ms);
void        LapTask_Abort(uint32_t now_ms);
void        LapTask_Update(uint32_t now_ms);

lap_state_t LapTask_GetState(void);
bool        LapTask_IsActive(void);
uint32_t    LapTask_GetElapsedMs(void);
uint8_t     LapTask_GetMask(void);
int32_t     LapTask_GetError(void);

#endif /* LAP_TASK_H */
