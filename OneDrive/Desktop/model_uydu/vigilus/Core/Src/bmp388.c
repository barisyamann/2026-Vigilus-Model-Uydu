/*
 * bmp388.c
 *
 *  Created on: 3 Ağu 2026
 *      Author: Barış
 */

#include "bmp388.h"
#include "main.h"
#include <stdio.h>
#include <math.h>

extern I2C_HandleTypeDef hi2c1;

#define BMP388_ADDR             (0x76 << 1)
#define BMP388_CHIP_ID_REG      0x00
#define BMP388_ERR_REG          0x02
#define BMP388_STATUS_REG       0x03
#define BMP388_DATA_START_REG   0x04
#define BMP388_PWR_CTRL_REG     0x1B
#define BMP388_OSR_REG          0x1C
#define BMP388_ODR_REG          0x1D
#define BMP388_CALIB_START_REG  0x31

static BMP388_CalibData_t calib;
static float ground_pressure_pa = 101325.0f;

static void BMP388_ReadCalibration(void)
{
    uint8_t raw[21], raw2[21];

    HAL_StatusTypeDef s1 = HAL_I2C_Mem_Read(&hi2c1, BMP388_ADDR, BMP388_CALIB_START_REG,
                                              I2C_MEMADD_SIZE_8BIT, raw, 21, 100);
    HAL_Delay(10);
    HAL_StatusTypeDef s2 = HAL_I2C_Mem_Read(&hi2c1, BMP388_ADDR, BMP388_CALIB_START_REG,
                                              I2C_MEMADD_SIZE_8BIT, raw2, 21, 100);

    uint8_t mismatch = 0;
    for (int i = 0; i < 21; i++)
    {
        if (raw[i] != raw2[i]) mismatch++;
    }
    printf("[BMP388] calib read: s1=%d s2=%d mismatch=%d byte\r\n", s1, s2, mismatch);

    calib.par_t1 = (uint16_t)(raw[1] << 8 | raw[0]);
    calib.par_t2 = (uint16_t)(raw[3] << 8 | raw[2]);
    calib.par_t3 = (int8_t)raw[4];

    calib.par_p1 = (int16_t)(raw[6] << 8 | raw[5]);
    calib.par_p2 = (int16_t)(raw[8] << 8 | raw[7]);
    calib.par_p3 = (int8_t)raw[9];
    calib.par_p4 = (int8_t)raw[10];
    calib.par_p5 = (uint16_t)(raw[12] << 8 | raw[11]);
    calib.par_p6 = (uint16_t)(raw[14] << 8 | raw[13]);
    calib.par_p7 = (int8_t)raw[15];
    calib.par_p8 = (int8_t)raw[16];
    calib.par_p9 = (int16_t)(raw[18] << 8 | raw[17]);
    calib.par_p10 = (int8_t)raw[19];
    calib.par_p11 = (int8_t)raw[20];

    printf("[BMP388] t1=%u t2=%u t3=%d p1=%d p2=%d p3=%d p4=%d p5=%u p6=%u p7=%d p8=%d p9=%d p10=%d p11=%d\r\n",
           calib.par_t1, calib.par_t2, calib.par_t3,
           calib.par_p1, calib.par_p2, calib.par_p3, calib.par_p4,
           calib.par_p5, calib.par_p6, calib.par_p7, calib.par_p8,
           calib.par_p9, calib.par_p10, calib.par_p11);
}

uint8_t BMP388_Init(void)
{
    uint8_t chip_id = 0;
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, BMP388_ADDR, BMP388_CHIP_ID_REG,
                                                  I2C_MEMADD_SIZE_8BIT, &chip_id, 1, 100);
    printf("[BMP388] init read: status=%d chip_id=0x%02X\r\n", status, chip_id);

    if (status != HAL_OK || chip_id != 0x50)
    {
        return 0;
    }

    BMP388_ReadCalibration();

    uint8_t osr = 0x03; // osr_p=x8, osr_t=x1
    HAL_I2C_Mem_Write(&hi2c1, BMP388_ADDR, BMP388_OSR_REG, I2C_MEMADD_SIZE_8BIT, &osr, 1, 100);

    uint8_t odr = 0x02; // 50 Hz (20ms periyot) - x8 osr icin yeterli sure
    HAL_I2C_Mem_Write(&hi2c1, BMP388_ADDR, BMP388_ODR_REG, I2C_MEMADD_SIZE_8BIT, &odr, 1, 100);

    uint8_t pwr_ctrl = 0x33; // press_en=1, temp_en=1, mode=normal(11)
    HAL_I2C_Mem_Write(&hi2c1, BMP388_ADDR, BMP388_PWR_CTRL_REG, I2C_MEMADD_SIZE_8BIT, &pwr_ctrl, 1, 100);

    HAL_Delay(100); // ilk olcumun tamamlanmasi icin bekle

    uint8_t err_reg = 0, status_reg = 0;
    HAL_I2C_Mem_Read(&hi2c1, BMP388_ADDR, BMP388_ERR_REG, I2C_MEMADD_SIZE_8BIT, &err_reg, 1, 100);
    HAL_I2C_Mem_Read(&hi2c1, BMP388_ADDR, BMP388_STATUS_REG, I2C_MEMADD_SIZE_8BIT, &status_reg, 1, 100);
    printf("[BMP388] err_reg=0x%02X status_reg=0x%02X\r\n", err_reg, status_reg);

    return 1;
}

static float compensate_temperature(int32_t uncomp_temp)
{
    float par_t1 = calib.par_t1 * 256.0f;
    float par_t2 = calib.par_t2 / 1073741824.0f;      // 2^30
    float par_t3 = calib.par_t3 / 281474976710656.0f; // 2^48

    float partial1 = (float)uncomp_temp - par_t1;
    float partial2 = partial1 * par_t2;
    return partial2 + (partial1 * partial1) * par_t3;
}

static float compensate_pressure(int32_t uncomp_press, float comp_temp)
{
    float par_p1  = (calib.par_p1  - 16384.0f) / 1048576.0f;    // 2^20
    float par_p2  = (calib.par_p2  - 16384.0f) / 536870912.0f;  // 2^29
    float par_p3  = calib.par_p3  / 4294967296.0f;              // 2^32
    float par_p4  = calib.par_p4  / 137438953472.0f;            // 2^37
    float par_p5  = calib.par_p5  * 8.0f;
    float par_p6  = calib.par_p6  / 64.0f;
    float par_p7  = calib.par_p7  / 256.0f;
    float par_p8  = calib.par_p8  / 32768.0f;
    float par_p9  = calib.par_p9  / 281474976710656.0f;         // 2^48
    float par_p10 = calib.par_p10 / 281474976710656.0f;         // 2^48
    float par_p11 = calib.par_p11 / 36893488147419103232.0f;    // 2^65

    float out1 = par_p5 + (par_p6 * comp_temp) + (par_p7 * comp_temp * comp_temp)
                 + (par_p8 * comp_temp * comp_temp * comp_temp);

    float out2 = (float)uncomp_press * (par_p1 + (par_p2 * comp_temp)
                 + (par_p3 * comp_temp * comp_temp) + (par_p4 * comp_temp * comp_temp * comp_temp));

    float press2 = (float)uncomp_press * (float)uncomp_press;
    float out3 = press2 * (par_p9 + par_p10 * comp_temp);
    float out4 = press2 * (float)uncomp_press * par_p11;

    return out1 + out2 + out3 + out4;
}

void BMP388_ReadData(BMP388_Data_t *out)
{
    uint8_t raw[6];
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, BMP388_ADDR, BMP388_DATA_START_REG,
                                                  I2C_MEMADD_SIZE_8BIT, raw, 6, 100);
    if (status != HAL_OK)
    {
        return; // okuma basarisiz, eski degerleri koru
    }

    int32_t uncomp_press = (int32_t)(raw[2] << 16 | raw[1] << 8 | raw[0]);
    int32_t uncomp_temp  = (int32_t)(raw[5] << 16 | raw[4] << 8 | raw[3]);

    out->temperature_c = compensate_temperature(uncomp_temp);
    out->pressure_pa = compensate_pressure(uncomp_press, out->temperature_c);
    out->altitude_m = 44330.0f * (1.0f - powf(out->pressure_pa / ground_pressure_pa, 0.1903f));
}
void BMP388_SetGroundReference(uint16_t sample_count)
{
    float sum = 0.0f;
    uint16_t ok_count = 0;
    BMP388_Data_t temp;

    for (uint16_t i = 0; i < sample_count; i++)
    {
        BMP388_ReadData(&temp);
        if (temp.pressure_pa > 30000.0f && temp.pressure_pa < 120000.0f) // makul aralik
        {
            sum += temp.pressure_pa;
            ok_count++;
        }
        HAL_Delay(25); // 50Hz ODR icin yeni olcum bekle
    }

    if (ok_count > 0)
    {
        ground_pressure_pa = sum / ok_count;
    }
    printf("[BMP388] zemin referansi: %.2f Pa (%u ornek)\r\n", ground_pressure_pa, ok_count);
}
