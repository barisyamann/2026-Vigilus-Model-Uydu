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

#include "SensorManager.h"
#include <stdint.h>

/* Kendi takim numaranizi buraya yazin */
#define TEAM_NUMBER 864773

/* ==========================================
 * XBEE ICIN SIKISTIRILMIS BINARY PAKET YAPISI
 * ========================================== */
#pragma pack(push, 1) // Hafiza hizalama bosluklarini (padding) kapatir
typedef struct {
    uint32_t takim_no;          // 4 byte
    uint32_t paket_sayaci;      // 4 byte

    // Tarih ve Zaman
    uint8_t gun, ay, yil;       // 3 byte
    uint8_t saat, dakika, saniye; // 3 byte

    // Durumlar
    uint8_t uydu_statusu;       // 1 byte
    uint8_t hata_kodu_num;      // 1 byte (String'den sayiya cevrilmis)

    // Cevresel ve Inis Verileri
    float basinc;               // 4 byte
    float yukseklik;            // 4 byte
    float inis_hizi;            // 4 byte
    float sicaklik;             // 4 byte
    float pil_gerilimi;         // 4 byte

    // GPS Verileri
    float gps_enlem;            // 4 byte (double'dan float'a cast edilecek)
    float gps_boylam;           // 4 byte (double'dan float'a cast edilecek)
    float gps_yukseklik;        // 4 byte

    // IMU (Yonelim) Verileri
    float pitch;                // 4 byte
    float roll;                 // 4 byte
    float yaw;                  // 4 byte

    char rhrhrh[6];    // 6 byte
} XBeeTelemetriPaketi_t;
#pragma pack(pop)
// TOPLAM BOYUT: 61 Byte


void Telemetry_Init(void);
void Telemetry_SetStatus(uint8_t status);
void Telemetry_SetErrorCode(const char *code);
void Telemetry_SetRhrhrh(const char *code);
uint32_t Telemetry_GetPacketCount(void);

// Sensor verilerini alip hem SD karta CSV yazar hem de XBee'den Binary gonderir
void Telemetry_Send(const SensorData_t *s);

#endif /* TELEMETRY_H */
