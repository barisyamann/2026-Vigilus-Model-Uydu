#ifndef MPU9250_H
#define MPU9250_H

#include <stdint.h>

typedef struct
{
    float accel_x_g, accel_y_g, accel_z_g;
    float gyro_x_dps, gyro_y_dps, gyro_z_dps;
} MPU9250_Data_t;

uint8_t MPU9250_Init(void);      // 1 = basarili, 0 = cihaz bulunamadi
uint8_t MPU9250_ReadData(MPU9250_Data_t *out);
void MPU9250_CalibrateGyro(uint16_t sample_count);

#endif
