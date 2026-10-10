#include "mpu9250.h"
#include "main.h" // hi2c1 handle'ı buradan geliyor
extern I2C_HandleTypeDef hi2c1;
#define MPU9250_ADDR            (0x68 << 1)
#define MPU9250_WHO_AM_I_REG    0x75
#define MPU9250_PWR_MGMT_1      0x6B
#define MPU9250_CONFIG          0x1A
#define MPU9250_GYRO_CONFIG     0x1B
#define MPU9250_ACCEL_CONFIG    0x1C
#define MPU9250_ACCEL_XOUT_H    0x3B
static float gyro_bias_x = 0.0f;
static float gyro_bias_y = 0.0f;
static float gyro_bias_z = 0.0f;



uint8_t MPU9250_Init(void)
{
    uint8_t who_am_i = 0;
    HAL_I2C_Mem_Read(&hi2c1, MPU9250_ADDR, MPU9250_WHO_AM_I_REG,
                      I2C_MEMADD_SIZE_8BIT, &who_am_i, 1, 100);

    if (who_am_i != 0x70 && who_am_i != 0x71 && who_am_i != 0x73)
    {
        return 0; // beklenmeyen cihaz / I2C cevap yok
    }

    uint8_t data;

    data = 0x00; // SLEEP bitini temizle
    HAL_I2C_Mem_Write(&hi2c1, MPU9250_ADDR, MPU9250_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    HAL_Delay(100);

    data = 0x03; // DLPF ~44Hz
    HAL_I2C_Mem_Write(&hi2c1, MPU9250_ADDR, MPU9250_CONFIG, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);

    data = 0x00; // Gyro +/-250 dps
    HAL_I2C_Mem_Write(&hi2c1, MPU9250_ADDR, MPU9250_GYRO_CONFIG, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);

    data = 0x00; // Accel +/-2g
    HAL_I2C_Mem_Write(&hi2c1, MPU9250_ADDR, MPU9250_ACCEL_CONFIG, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);

    return 1;
}

uint8_t MPU9250_ReadData(MPU9250_Data_t *out)
{
    uint8_t buf[14];
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, MPU9250_ADDR, MPU9250_ACCEL_XOUT_H,
                                                  I2C_MEMADD_SIZE_8BIT, buf, 14, 100);
    if (status != HAL_OK)
    {
        return 0; // okuma basarisiz, out'a dokunma (eski deger korunur, cagiran taraf haberdar olur)
    }

    int16_t ax = (int16_t)(buf[0] << 8 | buf[1]);
    int16_t ay = (int16_t)(buf[2] << 8 | buf[3]);
    int16_t az = (int16_t)(buf[4] << 8 | buf[5]);
    int16_t gx = (int16_t)(buf[8] << 8 | buf[9]);
    int16_t gy = (int16_t)(buf[10] << 8 | buf[11]);
    int16_t gz = (int16_t)(buf[12] << 8 | buf[13]);

    out->accel_x_g  = ax / 16384.0f;
    out->accel_y_g  = ay / 16384.0f;
    out->accel_z_g  = az / 16384.0f;
    out->gyro_x_dps = (gx / 131.0f) - gyro_bias_x;
    out->gyro_y_dps = (gy / 131.0f) - gyro_bias_y;
    out->gyro_z_dps = (gz / 131.0f) - gyro_bias_z;


    return 1;
}
void MPU9250_CalibrateGyro(uint16_t sample_count)
{
    float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;
    uint16_t ok_count = 0;
    MPU9250_Data_t temp;

    for (uint16_t i = 0; i < sample_count; i++)
    {
        if (MPU9250_ReadData(&temp))   // bias henuz 0 oldugu icin ham deger doner
        {
            sum_x += temp.gyro_x_dps;
            sum_y += temp.gyro_y_dps;
            sum_z += temp.gyro_z_dps;
            ok_count++;
        }
        HAL_Delay(5);
    }

    if (ok_count > 0)
    {
        gyro_bias_x = sum_x / ok_count;
        gyro_bias_y = sum_y / ok_count;
        gyro_bias_z = sum_z / ok_count;
    }
}
