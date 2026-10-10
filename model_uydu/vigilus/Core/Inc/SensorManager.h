#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>

/* Sensor saglik bayraklari (bit maskesi) */
#define SENSOR_OK_BMP388    (1 << 0)
#define SENSOR_OK_MPU9250   (1 << 1)
#define SENSOR_OK_GPS       (1 << 2)
#define SENSOR_OK_INA226    (1 << 3)

typedef struct
{
    /* --- BMP388 --- */
    float pressure_pa;
    float temperature_c;
    float altitude_m;          // kalkis noktasina gore

    /* --- Turetilmis --- */
    float vertical_speed_ms;   // inis hizi (asagi = pozitif)

    /* --- MPU9250 --- */
    float accel_x_g, accel_y_g, accel_z_g;
    float gyro_x_dps, gyro_y_dps, gyro_z_dps;
    float pitch_deg, roll_deg, yaw_deg;

    /* --- GPS --- */
    uint8_t gps_fix;
    uint8_t gps_satellites;    // fix icin kullanilan
    uint8_t gps_sats_view;     // gorunen
    double  gps_latitude;
    double  gps_longitude;
    float   gps_altitude_m;
    uint8_t gps_hour, gps_minute, gps_second;
    uint8_t gps_day, gps_month, gps_year;

    /* --- INA226 --- */
    float battery_voltage_v;
    float current_a;

    /* --- Durum --- */
    uint8_t  health_flags;
    uint32_t timestamp_ms;


} SensorData_t;

uint8_t SensorManager_Init(void);
void    SensorManager_Update(void);
void    SensorManager_FeedGpsByte(uint8_t byte);   // UART kesmesinden cagrilir
const SensorData_t* SensorManager_GetData(void);

#endif /* SENSOR_MANAGER_H */
