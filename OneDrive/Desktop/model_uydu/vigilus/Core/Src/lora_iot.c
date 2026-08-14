/*
 * lora_iot.c
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */


#include "lora_iot.h"
#include "main.h"
#include <string.h>

extern UART_HandleTypeDef huart4;

void LoRa_Init(void)
{
    /* UART4 CubeMX tarafindan zaten init ediliyor (MX_UART4_Init), burada ekstra bir sey gerekmiyor. */
}

void LoRa_SendRhrhrh(const char *code)
{
    uint8_t buf[8];
    uint8_t len = 0;

    len = (uint8_t)strlen(code);
    if (len > 6) len = 6;

    memcpy(buf, code, len);
    buf[len]   = '\n';
    len += 1;

    HAL_UART_Transmit(&huart4, buf, len, 100);
}
