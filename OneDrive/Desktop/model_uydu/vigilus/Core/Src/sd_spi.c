/*
 * sd_spi.c
 *
 *  Created on: 4 Ağu 2026
 *      Author: Barış
 */

#include "sd_spi.h"
#include "main.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;

#define SD_CS_LOW()   HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)
#define SD_CS_HIGH()  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)

#define CMD0    0
#define CMD8    8
#define CMD55  55
#define CMD41  41
#define CMD58  58

static SD_Type_t card_type = SD_TYPE_UNKNOWN;

static uint8_t SPI_TxRx(uint8_t data)
{
    uint8_t rx = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &data, &rx, 1, 100);
    return rx;
}

static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    SPI_TxRx(0xFF);

    SPI_TxRx(0x40 | cmd);
    SPI_TxRx((uint8_t)(arg >> 24));
    SPI_TxRx((uint8_t)(arg >> 16));
    SPI_TxRx((uint8_t)(arg >> 8));
    SPI_TxRx((uint8_t)arg);
    SPI_TxRx(crc);

    uint8_t r1 = 0xFF;
    for (uint8_t i = 0; i < 10; i++)
    {
        r1 = SPI_TxRx(0xFF);
        if ((r1 & 0x80) == 0) break;
    }
    return r1;
}

uint8_t SD_Init(void)
{
    card_type = SD_TYPE_UNKNOWN;

    SD_CS_HIGH();
    HAL_Delay(10);
    for (uint8_t i = 0; i < 10; i++) SPI_TxRx(0xFF);

    SD_CS_LOW();
    uint8_t r1 = 0xFF;
    for (uint8_t retry = 0; retry < 100; retry++)
    {
        r1 = SD_SendCmd(CMD0, 0, 0x95);
        if (r1 == 0x01) break;
        HAL_Delay(1);
    }
    printf("[SD] CMD0 -> 0x%02X\r\n", r1);

    if (r1 != 0x01)
    {
        SD_CS_HIGH();
        return 0;
    }

    /* --- TESHIS: CMD8'i 5 kez tekrarla, tutarli mi rastgele mi gor --- */
    for (uint8_t k = 0; k < 5; k++)
    {
        uint8_t rr = SD_SendCmd(CMD8, 0x000001AA, 0x87);
        uint8_t b[5];
        for (uint8_t i = 0; i < 5; i++) b[i] = SPI_TxRx(0xFF);
        printf("[SD] CMD8 #%u -> R1=0x%02X  R7= %02X %02X %02X %02X %02X\r\n",
               k, rr, b[0], b[1], b[2], b[3], b[4]);
        HAL_Delay(10);
    }

    SD_CS_HIGH();
    return 0;
}

SD_Type_t SD_GetType(void)
{
    return card_type;
}
