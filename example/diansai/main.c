#include "ti_msp_dl_config.h"
#include "board.h"

/*
 * IMU601 欧拉角 OLED 显示测试
 *
 * IMU601 (UART1, 115200) 在接收中断中自动解析姿态帧并更新全局变量
 * IMU601_Attitude (yaw / pitch / roll, 单位 °)。主循环读取该变量，
 * 格式化为字符串后显示到 OLED。
 */

/* 将浮点角度格式化为固定 6 字符宽字符串，形如 " 12.3" / "-45.6" / "359.9"
 * 固定宽度避免数字变短时 OLED 上残留旧字符。范围 -999.9 ~ 999.9。 */
static void angle_to_str(float val, char *buf)
{
    char     tmp[8];
    int      i = 0, j = 0, k;
    int32_t  scaled;
    uint32_t int_part, frac_part;
    uint8_t  neg = 0;

    if (val < 0.0f) { neg = 1; val = -val; }
    scaled    = (int32_t)(val * 10.0f + 0.5f);   /* 放大 10 倍并四舍五入 */
    int_part  = (uint32_t)(scaled / 10);
    frac_part = (uint32_t)(scaled % 10);

    /* 整数部分倒序暂存（不含符号） */
    if (int_part == 0) tmp[j++] = '0';
    while (int_part) { tmp[j++] = (char)('0' + int_part % 10); int_part /= 10; }

    /* 右对齐到 4 字符宽（符号 + 整数），再补 ".d" 一位小数 */
    for (k = j + (neg ? 1 : 0); k < 4; k++) buf[i++] = ' ';
    if (neg) buf[i++] = '-';
    for (k = j - 1; k >= 0; k--) buf[i++] = tmp[k];
    buf[i++] = '.';
    buf[i++] = (char)('0' + frac_part);
    buf[i]   = '\0';
}

int main(void)
{
    char buf[16];

    SYSCFG_DL_init();
    OLED_Init();
    OLED_Clear();
    IMU601_Init();   /* 模块复位+校准后默认即输出姿态帧, 无需额外设速率 */

    OLED_ShowString(1, 1, "IMU601 Euler");
    OLED_ShowString(2, 1, "Yaw  :");
    OLED_ShowString(3, 1, "Pitch:");
    OLED_ShowString(4, 1, "Roll :");

    while (1)
    {
        angle_to_str(IMU601_Attitude.yaw,   buf);  OLED_ShowString(2, 7, buf);
        angle_to_str(IMU601_Attitude.pitch, buf);  OLED_ShowString(3, 7, buf);
        angle_to_str(IMU601_Attitude.roll,  buf);  OLED_ShowString(4, 7, buf);

        delay_ms(50);
    }
}
