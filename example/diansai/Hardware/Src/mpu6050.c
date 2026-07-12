#include "bsp_iic.h"
#include "bsp_systick.h"
#include "mpu6050.h"
#include "math.h"

#define MPU6050_ADDR		0x68		//MPU6050的7位I2C从机地址

/* 灵敏度常量 */
#define GYRO_SCALE  (16.4f)     // ±2000°/s => 16.4 LSB/°/s
#define ACCEL_SCALE (2048.0f)   // ±16g => 2048 LSB/g

/* 互补滤波系数 */
#define FILTER_ALPHA    (0.90f)

/* 内部浮点欧拉角（保留精度用于互补滤波计算） */
static float roll_f  = 0.0f;
static float pitch_f = 0.0f;
static float yaw_f   = 0.0f;

/* 对外输出的整数欧拉角（与头文件 extern 声明匹配） */
volatile int16_t roll  = 0;
volatile int16_t pitch = 0;
volatile int16_t yaw   = 0;

/* 陀螺仪零偏值（校准后使用） */
static int16_t gx_offset = 0, gy_offset = 0, gz_offset = 0;

/* 上一次调用时间戳（用SysTick，每1ms+1） */
static uint32_t last_tick = 0;

/* 数据就绪中断标志（由 GPIOB_IRQHandler 置位） */
volatile uint8_t MPU6050_DataReady = 0;

/**
  * 函    数：MPU6050写寄存器
  */
void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data)
{
	User_sIICDev.write_reg(MPU6050_ADDR << 1, RegAddress, &Data, 1, 100);
}

/**
  * 函    数：MPU6050读寄存器
  */
uint8_t MPU6050_ReadReg(uint8_t RegAddress)
{
	uint8_t data = 0;
	User_sIICDev.read_reg(MPU6050_ADDR << 1, RegAddress, &data, 1, 100);
	return data;
}

/**
  * 函    数：MPU6050初始化
  */
void MPU6050_Init(void)
{
	MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);
	MPU6050_WriteReg(MPU6050_PWR_MGMT_2, 0x00);
	MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x04);
	MPU6050_WriteReg(MPU6050_CONFIG, 0x06);
	MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x18);	// ±2000°/s
	MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x18);	// ±16g
}

/**
  * 函    数：MPU6050中断引脚初始化
  * 说    明：配置 INT 引脚为数据就绪模式
  *         寄存器 INT_PIN_CFG(0x37) bit4=1 → 读取清除
  *         寄存器 INT_ENABLE(0x38) bit0=1 → 数据就绪中断使能
  */
void MPU6050_INT_Init(void)
{
	/* INT_PIN_CFG: bit6=0(推挽), bit5=0(低电平), bit4=1(读取清除), bit3=1(电平保持) */
	MPU6050_WriteReg(0x37, 0x18);
	/* INT_ENABLE: bit0=1 数据就绪中断 */
	MPU6050_WriteReg(0x38, 0x01);
}

/**
  * 函    数：MPU6050获取ID号
  */
uint8_t MPU6050_GetID(void)
{
	return MPU6050_ReadReg(MPU6050_WHO_AM_I);
}

/**
  * 函    数：MPU6050获取数据
  */
void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ, 
						int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ)
{
	uint8_t buf[14];

	User_sIICDev.read_reg(MPU6050_ADDR << 1, MPU6050_ACCEL_XOUT_H, buf, 14, 100);

	*AccX = (int16_t)((buf[0] << 8) | buf[1]);
	*AccY = (int16_t)((buf[2] << 8) | buf[3]);
	*AccZ = (int16_t)((buf[4] << 8) | buf[5]);
	*GyroX = (int16_t)((buf[8] << 8) | buf[9]);
	*GyroY = (int16_t)((buf[10] << 8) | buf[11]);
	*GyroZ = (int16_t)((buf[12] << 8) | buf[13]);
}

/**
  * 函    数：陀螺仪零偏校准
  * 说    明：在MPU6050完全静止时调用，采集100次取平均作为零偏
  */
void MPU6050_Calibrate(void)
{
	int16_t ax, ay, az, gx, gy, gz;
	int32_t sum_gx = 0, sum_gy = 0, sum_gz = 0;
	uint8_t i;

	for (i = 0; i < 100; i++)
	{
		MPU6050_GetData(&ax, &ay, &az, &gx, &gy, &gz);
		sum_gx += gx;
		sum_gy += gy;
		sum_gz += gz;
		delay_ms(5);
	}

	gx_offset = (int16_t)(sum_gx / 100);
	gy_offset = (int16_t)(sum_gy / 100);
	gz_offset = (int16_t)(sum_gz / 100);

	/* 复位欧拉角 */
	roll_f = 0.0f;
	pitch_f = 0.0f;
	yaw_f = 0.0f;
	last_tick = 0;
}

/**
  * 函    数：MPU6050计算欧拉角（互补滤波）
  * 说    明：需要在main循环中反复调用，调用间隔约5ms
  */
void MPU6050_Calculate(void)
{
	int16_t ax, ay, az, gx, gy, gz;
	float gx_dps, gy_dps, gz_dps;   // DPS = °/s
	float ax_g, ay_g, az_g;          // g为单位
	float dt;
	uint32_t now;
	float roll_a, pitch_a;

	/* 读取原始数据 */
	MPU6050_GetData(&ax, &ay, &az, &gx, &gy, &gz);

	/* ----- 计算dt (单位:秒) ----- */
	now = Systick_getTick();
	if (last_tick == 0) last_tick = now;
	dt = (float)(now - last_tick) / 1000.0f;
	last_tick = now;

	/* 限制dt范围，防止异常值 */
	if (dt <= 0.0f) dt = 0.005f;
	if (dt > 0.1f)  dt = 0.005f;

	/* ----- 陀螺仪积分 (已减去零偏, 换算为°/s) ----- */
	gx_dps = (float)(gx - gx_offset) / GYRO_SCALE;
	gy_dps = (float)(gy - gy_offset) / GYRO_SCALE;
	gz_dps = (float)(gz - gz_offset) / GYRO_SCALE;

	roll_f  += gx_dps * dt;
	pitch_f += gy_dps * dt;
	yaw_f   += gz_dps * dt;

	/* ----- 加速度计解算姿态 (单位:度) ----- */
	ax_g = (float)ax / ACCEL_SCALE;
	ay_g = (float)ay / ACCEL_SCALE;
	az_g = (float)az / ACCEL_SCALE;

	roll_a  = atan2f(ay_g, az_g) * 180.0f / 3.14159265f;
	pitch_a = atan2f(-ax_g, sqrtf(ay_g * ay_g + az_g * az_g)) * 180.0f / 3.14159265f;

	/* ----- 一阶互补滤波 ----- */
	roll_f  = FILTER_ALPHA * roll_f  + (1.0f - FILTER_ALPHA) * roll_a;
	pitch_f = FILTER_ALPHA * pitch_f + (1.0f - FILTER_ALPHA) * pitch_a;
	/* yaw 没有加速度计修正，仅靠陀螺仪，会缓慢漂移 */

	/* 浮点结果取整输出给外部使用 */
	roll  = (int16_t)roll_f;
	pitch = (int16_t)pitch_f;
	yaw   = (int16_t)yaw_f;
}
