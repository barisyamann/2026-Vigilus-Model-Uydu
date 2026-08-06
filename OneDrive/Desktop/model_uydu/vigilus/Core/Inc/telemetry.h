/*
 * telemetry.h
 *
 *  Created on: 5 Ağu 2026
 *      Author: Barış
 */

/*
 * telemetry.h - TURKSAT Model Uydu telemetri paketi olusturma ve gonderme
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include "SensorManager.h"

#define TEAM_NUMBER   864773

void     Telemetry_Init(void);
void     Telemetry_Send(const SensorData_t *s);
uint32_t Telemetry_GetPacketCount(void);

/* Ucus yazilimi bu alanlari guncelleyecek (Faz 10, 16, 17) */
void Telemetry_SetStatus(uint8_t status);          // 0-5 arasi ucus asamasi
void Telemetry_SetErrorCode(const char *code);     // "00000" gibi 5 haneli
void Telemetry_SetRhrhrh(const char *code);        // "2R0G1B" gibi 6 haneli

#endif /* TELEMETRY_H */
