#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdint.h>

#define SD_BLOCK_SIZE 512

typedef enum
{
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_SDSC,      // standart kapasite, bayt adresleme
    SD_TYPE_SDHC       // yuksek kapasite, blok adresleme
} SD_Type_t;

uint8_t   SD_Init(void);
SD_Type_t SD_GetType(void);

uint8_t   SD_ReadBlock(uint32_t block_addr, uint8_t *buf);
uint8_t   SD_WriteBlock(uint32_t block_addr, const uint8_t *buf);

uint8_t   SD_TestReadMBR(void);
uint8_t   SD_TestWriteReadBack(uint32_t block_addr);

#endif
