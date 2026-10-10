/*
 * xbee_api.c - XBee API modu (AP=1) cerceve sarma/cozme katmani.
 *
 * AP=1'de byte-stuffing (escape) YOK. Cerceveyi ayirmak icin 0x7E'yi degil
 * uzunluk alanini kullaniyoruz - FRAME_DATA icindeyken 0x7E gecmesi sorun
 * degil, sadece WAIT_START durumunda baslangic sayilir.
 */
#include "xbee_api.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart3;

#define XBEE_MAX_FRAME  200   /* telemetri CSV satirindan biraz buyuk */

typedef enum
{
    XBEE_WAIT_START,
    XBEE_LEN_MSB,
    XBEE_LEN_LSB,
    XBEE_FRAME_DATA,
    XBEE_CHECKSUM
} XBeeRxState_t;

static XBeeRxState_t rx_state = XBEE_WAIT_START;
static uint16_t rx_len = 0;
static uint16_t rx_idx = 0;
static uint8_t  rx_buf[XBEE_MAX_FRAME];
static XBeeAPI_RxCallback_t user_cb = NULL;

static uint8_t XBeeAPI_Checksum(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0;
    for (uint16_t i = 0; i < len; i++) sum += data[i];
    return (uint8_t)(0xFF - (sum & 0xFF));
}

void XBeeAPI_Init(XBeeAPI_RxCallback_t rx_callback)
{
    user_cb  = rx_callback;
    rx_state = XBEE_WAIT_START;
}

void XBeeAPI_FeedByte(uint8_t byte)
{
    switch (rx_state)
    {
        case XBEE_WAIT_START:
            if (byte == 0x7E) rx_state = XBEE_LEN_MSB;
            break;

        case XBEE_LEN_MSB:
            rx_len   = (uint16_t)byte << 8;
            rx_state = XBEE_LEN_LSB;
            break;

        case XBEE_LEN_LSB:
            rx_len |= byte;
            if (rx_len == 0 || rx_len > XBEE_MAX_FRAME)
            {
                rx_state = XBEE_WAIT_START;   /* gecersiz uzunluk */
                break;
            }
            rx_idx   = 0;
            rx_state = XBEE_FRAME_DATA;
            break;

        case XBEE_FRAME_DATA:
            rx_buf[rx_idx++] = byte;
            if (rx_idx >= rx_len) rx_state = XBEE_CHECKSUM;
            break;

        case XBEE_CHECKSUM:
        {
            uint8_t calc = XBeeAPI_Checksum(rx_buf, rx_len);
            if (calc == byte)
            {
                /* RX Packet, 16-bit adres (0x81): tip+kaynak(2)+rssi+opt = 5 bayt basli */
                if (rx_buf[0] == 0x81 && rx_len >= 5)
                {
                    uint16_t src  = ((uint16_t)rx_buf[1] << 8) | rx_buf[2];
                    int8_t   rssi = -(int8_t)rx_buf[3];
                    uint16_t plen = (uint16_t)(rx_len - 5);

                    if (user_cb) user_cb(&rx_buf[5], plen, src, rssi);
                }
                /* Diger cerceve tipleri (0x89 TX Status vb.) simdilik yoksayilir */
            }
            rx_state = XBEE_WAIT_START;
            break;
        }
    }
}

/*
 * TX Request (0x01), 16-bit hedef adres. data+len telemetri CSV satiridir.
 * Frame ID = 0 -> modulden TX Status cercevesi istemiyoruz.
 */
uint8_t XBeeAPI_Send(uint16_t dest_addr, const uint8_t *data, uint16_t len)
{
    uint8_t payload[XBEE_MAX_FRAME];
    uint16_t p = 0;

    if (len > (XBEE_MAX_FRAME - 5)) return 0;   /* siga sigmaz, reddet */

    payload[p++] = 0x01;                        /* TX Request, 16-bit */
    payload[p++] = 0x00;                        /* Frame ID = 0 */
    payload[p++] = (uint8_t)(dest_addr >> 8);
    payload[p++] = (uint8_t)(dest_addr & 0xFF);
    payload[p++] = 0x00;                        /* Options */
    memcpy(&payload[p], data, len);
    p += len;

    uint8_t checksum = XBeeAPI_Checksum(payload, p);

    uint8_t frame[XBEE_MAX_FRAME + 4];
    uint16_t idx = 0;
    frame[idx++] = 0x7E;
    frame[idx++] = (uint8_t)(p >> 8);
    frame[idx++] = (uint8_t)(p & 0xFF);
    memcpy(&frame[idx], payload, p);
    idx += p;
    frame[idx++] = checksum;

    HAL_UART_Transmit(&huart3, frame, idx, 200);
    printf("[XBEE TX] dest=0x%04X len=%u\r\n", dest_addr, len);
    return 1;
}
