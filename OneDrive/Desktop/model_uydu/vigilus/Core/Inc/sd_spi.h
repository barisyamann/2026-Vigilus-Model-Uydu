/*
 * sd_spi.h
 *
 *  Created on: 4 Ağu 2026
 *      Author: Barış
 */

#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdint.h>

typedef enum
{
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_SDSC,      // standart kapasite, byte adresleme
    SD_TYPE_SDHC       // yuksek kapasite, blok adresleme
} SD_Type_t;

uint8_t   SD_Init(void);       // 1 = basarili
SD_Type_t SD_GetType(void);

#endif
