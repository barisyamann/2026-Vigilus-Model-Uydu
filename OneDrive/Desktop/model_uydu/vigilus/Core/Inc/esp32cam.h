/*
 * esp32cam.h
 *
 *  Created on: 6 Ağu 2026
 *      Author: Barış
 */

#ifndef ESP32CAM_H
#define ESP32CAM_H

#include <stdint.h>

void    ESP32CAM_Init(void);
void    ESP32CAM_FeedByte(uint8_t b);     // UART kesmesinden cagrilir

uint8_t ESP32CAM_Ping(void);              // 1 = PONG geldi
uint8_t ESP32CAM_SetActive(uint8_t on);   // 1 = ac, 0 = kapat
uint8_t ESP32CAM_Mark(const char *event); // olay isareti gonder
const char* ESP32CAM_GetStatus(void);     // "ST:cam=1,..." dizesi, NULL = hata

#endif
