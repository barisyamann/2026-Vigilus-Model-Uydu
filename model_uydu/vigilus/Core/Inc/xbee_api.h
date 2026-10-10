/*
 * xbee_api.h - XBee API modu (AP=1) cerceve sarma/cozme katmani.
 * DL/MY transparent modda oldugu gibi otomatik calismaz; API modunda hedef
 * adresi her TX cercevesinde biz belirtiriz.
 */

#ifndef XBEE_API_H
#define XBEE_API_H
  /* karsi (PC tarafi) modulun MY degeri */

#include <stdint.h>
#define XBEE_DEST_ADDR  0x0002
/* RF Data payload'i cikaran fonksiyonlar bu tipte bir callback alir */
typedef void (*XBeeAPI_RxCallback_t)(const uint8_t *payload, uint16_t len,
                                      uint16_t src_addr, int8_t rssi);

void    XBeeAPI_Init(XBeeAPI_RxCallback_t rx_callback);
void    XBeeAPI_FeedByte(uint8_t byte);           /* USART3 RX ISR'den cagir */
uint8_t XBeeAPI_Send(uint16_t dest_addr, const uint8_t *data, uint16_t len);

#endif
