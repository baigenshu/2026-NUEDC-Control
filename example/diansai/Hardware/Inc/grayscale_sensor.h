#ifndef __GRAYSCALE_SENSOR_H
#define __GRAYSCALE_SENSOR_H

#include <stdint.h>

// 读取所有传感器状态
// 深色(黑线)=1，浅色=0
// 返回值：bit0=S1, bit1=S2, ..., bit7=S8
uint8_t Read_Sensors(void);

// 根据8路传感器值计算位置偏差（供PID控制器使用）
// 返回偏差值，范围约 -3500 ~ +3500
// 负值 = 偏左（黑线在左侧），正值 = 偏右（黑线在右侧）
// 丢线时返回 0
int16_t Get_Tracking_Error(uint8_t sensor_val);

// 判断是否丢线
// 返回值：0=正常循迹，1=丢线
uint8_t Is_Tracking_Lost(uint8_t sensor_val);

#endif /* __GRAYSCALE_SENSOR_H */