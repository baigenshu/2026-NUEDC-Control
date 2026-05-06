#include "grayscale_sensor.h"
#include "ti_msp_dl_config.h"

// 定义传感器引脚（使用 ti_msp_dl_config.h 中的宏）
#define SENSOR1_PIN  GPIO_GRAY_PIN_0_PIN   // GPIOB PIN_19
#define SENSOR2_PIN  GPIO_GRAY_PIN_1_PIN   // GPIOB PIN_17
#define SENSOR3_PIN  GPIO_GRAY_PIN_2_PIN   // GPIOA PIN_16
#define SENSOR4_PIN  GPIO_GRAY_PIN_3_PIN   // GPIOA PIN_14
#define SENSOR5_PIN  GPIO_GRAY_PIN_4_PIN   // GPIOB PIN_20
#define SENSOR6_PIN  GPIO_GRAY_PIN_5_PIN   // GPIOB PIN_25
#define SENSOR7_PIN  GPIO_GRAY_PIN_6_PIN   // GPIOA PIN_25
#define SENSOR8_PIN  GPIO_GRAY_PIN_7_PIN   // GPIOA PIN_27

#define SENSOR_PORT_A  GPIOA
#define SENSOR_PORT_B  GPIOB

// 传感器端口分组定义（GPIOA上：S3,S4,S7,S8；GPIOB上：S1,S2,S5,S6）
#define SENSOR_GPIOA_PINS (SENSOR3_PIN | SENSOR4_PIN | SENSOR7_PIN | SENSOR8_PIN)
#define SENSOR_GPIOB_PINS (SENSOR1_PIN | SENSOR2_PIN | SENSOR5_PIN | SENSOR6_PIN)

// 每个传感器的坐标权重（左负右正，用于计算位置偏差）
// 传感器布局：S1(左) --- S8(右)
#define SENSOR1_WEIGHT   -3500
#define SENSOR2_WEIGHT   -2500
#define SENSOR3_WEIGHT   -1500
#define SENSOR4_WEIGHT    -500
#define SENSOR5_WEIGHT     500
#define SENSOR6_WEIGHT    1500
#define SENSOR7_WEIGHT    2500
#define SENSOR8_WEIGHT    3500

// 读取所有传感器状态
// 深色(黑线)=1，浅色=0
// 返回值：bit0=S1, bit1=S2, ..., bit7=S8
uint8_t Read_Sensors(void)
{
    uint8_t sensor_val = 0;
    
    // 读取GPIOB上的传感器 (S1, S2, S5, S6)
    uint32_t portb_val = DL_GPIO_readPins(SENSOR_PORT_B, SENSOR_GPIOB_PINS);
    if (portb_val & SENSOR1_PIN) sensor_val |= 0x01;  // bit0 = S1
    if (portb_val & SENSOR2_PIN) sensor_val |= 0x02;  // bit1 = S2
    if (portb_val & SENSOR5_PIN) sensor_val |= 0x10;  // bit4 = S5
    if (portb_val & SENSOR6_PIN) sensor_val |= 0x20;  // bit5 = S6
    
    // 读取GPIOA上的传感器 (S3, S4, S7, S8)
    uint32_t porta_val = DL_GPIO_readPins(SENSOR_PORT_A, SENSOR_GPIOA_PINS);
    if (porta_val & SENSOR3_PIN) sensor_val |= 0x04;  // bit2 = S3
    if (porta_val & SENSOR4_PIN) sensor_val |= 0x08;  // bit3 = S4
    if (porta_val & SENSOR7_PIN) sensor_val |= 0x40;  // bit6 = S7
    if (porta_val & SENSOR8_PIN) sensor_val |= 0x80;  // bit7 = S8
    
    return sensor_val;
}

// 根据8路传感器值计算位置偏差（供PID控制器使用）
// 返回偏差值，范围约 -3500 ~ +3500
// 负值 = 偏左（黑线在左侧），正值 = 偏右（黑线在右侧）
// 丢线时返回 0
int16_t Get_Tracking_Error(uint8_t sensor_val)
{
    int32_t sum_weight = 0;     // 加权和
    uint8_t active_count = 0;   // 检测到黑线的传感器数量
    
    // 计算加权平均
    if (sensor_val & 0x01) { sum_weight += SENSOR1_WEIGHT; active_count++; }
    if (sensor_val & 0x02) { sum_weight += SENSOR2_WEIGHT; active_count++; }
    if (sensor_val & 0x04) { sum_weight += SENSOR3_WEIGHT; active_count++; }
    if (sensor_val & 0x08) { sum_weight += SENSOR4_WEIGHT; active_count++; }
    if (sensor_val & 0x10) { sum_weight += SENSOR5_WEIGHT; active_count++; }
    if (sensor_val & 0x20) { sum_weight += SENSOR6_WEIGHT; active_count++; }
    if (sensor_val & 0x40) { sum_weight += SENSOR7_WEIGHT; active_count++; }
    if (sensor_val & 0x80) { sum_weight += SENSOR8_WEIGHT; active_count++; }
    
    // 没有检测到黑线（丢线），返回0
    if (active_count == 0) return 0;
    
    return (int16_t)(sum_weight / active_count);
}

// 判断是否丢线
// 返回值：0=正常循迹，1=丢线
uint8_t Is_Tracking_Lost(uint8_t sensor_val)
{
    // 全0（全白）或全1（全黑）视为丢线
    if (sensor_val == 0x00 || sensor_val == 0xFF) {
        return 1;
    }
    return 0;
}