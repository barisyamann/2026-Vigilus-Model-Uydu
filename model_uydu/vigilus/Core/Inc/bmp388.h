/*
 * bmp388.h
 *
 *  Created on: 3 Ağu 2026
 *      Author: Barış
 */

#ifndef BMP388_H
#define BMP388_H

#include <stdint.h>

typedef struct
{
    uint16_t par_t1;
    uint16_t par_t2;
    int8_t   par_t3;
    int16_t  par_p1;
    int16_t  par_p2;
    int8_t   par_p3;
    int8_t   par_p4;
    uint16_t par_p5;
    uint16_t par_p6;
    int8_t   par_p7;
    int8_t   par_p8;
    int16_t  par_p9;
    int8_t   par_p10;
    int8_t   par_p11;
} BMP388_CalibData_t;


typedef struct
{
    float temperature_c;
    float pressure_pa;
    float altitude_m;      // kalkis noktasina gore yukseklik
} BMP388_Data_t;

void BMP388_SetGroundReference(uint16_t sample_count);

void BMP388_ReadData(BMP388_Data_t *out);
uint8_t BMP388_Init(void);

#endif
