/*
 * esp32cam.c
 *
 *  Created on: 6 Ağu 2026
 *      Author: Barış
 */

#include "esp32cam.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart6;

#define RESP_BUF_SIZE   96
#define CMD_TIMEOUT_MS  500

static char     rx_buf[RESP_BUF_SIZE];
static uint16_t rx_idx = 0;
static char     last_response[RESP_BUF_SIZE];
static volatile uint8_t resp_ready = 0;

void ESP32CAM_Init(void)
{
    rx_idx = 0;
    resp_ready = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
    memset(last_response, 0, sizeof(last_response));
}

/* Kesme icinden cagrilir: satir tamamlaninca bayrak kaldiriyoruz */
void ESP32CAM_FeedByte(uint8_t b)
{
    if (b == '\r') return;

    if (b == '\n')
    {
        rx_buf[rx_idx] = '\0';
        if (rx_idx > 0)
        {
            strncpy(last_response, rx_buf, sizeof(last_response) - 1);
            last_response[sizeof(last_response) - 1] = '\0';
            resp_ready = 1;
        }
        rx_idx = 0;
        return;
    }

    if (rx_idx < (RESP_BUF_SIZE - 1)) rx_buf[rx_idx++] = (char)b;
    else                              rx_idx = 0;   // tasma - satiri at
}

/* Komut gonder, yanit bekle. Yanit geldiyse last_response'a yazilir. */
static uint8_t SendCommand(const char *cmd)
{
    // Komut göndermeden önce tamponu ve bayrağı sıfırla (Eski/yarım veriler gitsin)
    resp_ready = 0;
    rx_idx = 0;
    memset(rx_buf, 0, sizeof(rx_buf));

    // Komutu gönder
    HAL_UART_Transmit(&huart6, (uint8_t *)cmd, strlen(cmd), 200);

    // Timeout süresini 1000 ms yapıyoruz (ESP32 akış yaparken yavaşlayabilir)
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 1000)
    {
        if (resp_ready)
        {
            resp_ready = 0;
            return 1;
        }
    }
    return 0;   // Zaman aşımı
}

uint8_t ESP32CAM_Ping(void)
{
    if (!SendCommand("PING\n")) return 0;
    return (strcmp(last_response, "PONG") == 0);
}

uint8_t ESP32CAM_SetActive(uint8_t on)
{
    if (!SendCommand(on ? "CAM_ON\n" : "CAM_OFF\n")) return 0;
    return (strcmp(last_response, "OK") == 0);
}

uint8_t ESP32CAM_Mark(const char *event)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "MARK:%s\n", event);
    if (!SendCommand(cmd)) return 0;
    return (strcmp(last_response, "OK") == 0);
}

const char* ESP32CAM_GetStatus(void)
{
    if (!SendCommand("STATUS\n")) return NULL;
    return last_response;
}
