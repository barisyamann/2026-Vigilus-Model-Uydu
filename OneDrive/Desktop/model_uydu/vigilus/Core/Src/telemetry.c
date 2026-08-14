/*
 * telemetry.c
 *
 *  Created on: 5 Ağu 2026
 *      Author: Barış
 */
/*
 * telemetry.c
 *
 * Sartnamedeki 17 alanli telemetri paketini CSV formatinda olusturur ve
 * XBee (USART3) uzerinden yer istasyonuna gonderir.
 *
 * Alan sirasi (rapordaki tablo ile birebir):
 *   1  Paket Numarasi      10 GPS Latitude
 *   2  Uydu Statusu        11 GPS Longitude
 *   3  Hata Kodu           12 GPS Altitude
 *   4  Gonderme Saati      13 Pitch
 *   5  Basinc              14 Roll
 *   6  Yukseklik           15 Yaw
 *   7  Inis Hizi           16 RHRHRH
 *   8  Sicaklik            17 Takim No
 *   9  Pil Gerilimi
 */

/*
 * telemetry.c
 *
 * Guncellenmis Versiyon:
 * 1. SD Kart/Debug icin 140+ byte CSV formati uretir.
 * 2. XBee icin 59 byte'lik SIKISTIRILMIS BINARY formati uretir ve gonderir.
 */

#include "telemetry.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "xbee_api.h"

extern UART_HandleTypeDef huart3;

static uint32_t packet_counter = 0;
static uint8_t  satellite_status = 0;
static char     error_code_str[8]  = "00000";
static char     rhrhrh_code_str[8] = "000000";

/*
 * String olarak gelen "00001", "10010" gibi binary kodlari
 * 1 byte'lik sayiya (0-255) cevirir.
 */
static uint8_t StringHataKodunuSayiyaCevir(const char* kod) {
    // 2 rakami, stringin ikilik (binary) tabanda oldugunu belirtir.
    return (uint8_t)strtol(kod, NULL, 2);
}

void Telemetry_Init(void)
{
    packet_counter = 0;
    satellite_status = 0;
    strcpy(error_code_str, "00000");
    strcpy(rhrhrh_code_str, "000000");
}

void Telemetry_SetStatus(uint8_t status)
{
    satellite_status = status;
}

void Telemetry_SetErrorCode(const char *code)
{
    strncpy(error_code_str, code, sizeof(error_code_str) - 1);
    error_code_str[sizeof(error_code_str) - 1] = '\0';
}

void Telemetry_SetRhrhrh(const char *code)
{
    strncpy(rhrhrh_code_str, code, sizeof(rhrhrh_code_str) - 1);
    rhrhrh_code_str[sizeof(rhrhrh_code_str) - 1] = '\0';
}

uint32_t Telemetry_GetPacketCount(void)
{
    return packet_counter;
}

void Telemetry_Send(const SensorData_t *s)
{
    packet_counter++;

    /* ==============================================================
     * 1. BOLUM: SD KART VE DEBUG ICIN CSV STRING OLUSTURMA
     * ============================================================== */
    char packet_csv[256];
    int len_csv = snprintf(packet_csv, sizeof(packet_csv),
        "%lu,%u,%s,%02u/%02u/20%02u %02u:%02u:%02u,"
        "%.1f,%.1f,%.2f,%.1f,%.2f,"
        "%.6f,%.6f,%.1f,"
        "%.1f,%.1f,%.1f,"
        "%s,%lu\r\n",
        packet_counter,
        satellite_status,
        error_code_str,
        s->gps_day, s->gps_month, s->gps_year,
        s->gps_hour, s->gps_minute, s->gps_second,
        s->pressure_pa,
        s->altitude_m,
        s->vertical_speed_ms,
        s->temperature_c,
        s->battery_voltage_v,
        s->gps_latitude,
        s->gps_longitude,
        s->gps_altitude_m,
        s->pitch_deg,
        s->roll_deg,
        s->yaw_deg,
        rhrhrh_code_str,
        (unsigned long)TEAM_NUMBER);

    if (len_csv > 0)
    {
        // Debug portundan eski formati gorebilmek icin (Istege Bagli):
        // printf("%s", packet_csv);

        // TODO: SD Karta yazma fonksiyonunuzu buraya ekleyebilirsiniz:
        // Ornek: SD_Card_Write(packet_csv, len_csv);
    }

    /* ==============================================================
     * 2. BOLUM: XBEE UZERINDEN SIKISTIRILMIS BINARY (59 Byte) GONDERIM
     * ============================================================== */
    XBeeTelemetriPaketi_t binary_paket;

    binary_paket.takim_no = (uint32_t)TEAM_NUMBER;
    binary_paket.paket_sayaci = packet_counter;

    binary_paket.gun = s->gps_day;
    binary_paket.ay = s->gps_month;
    binary_paket.yil = s->gps_year;
    binary_paket.saat = s->gps_hour;
    binary_paket.dakika = s->gps_minute;
    binary_paket.saniye = s->gps_second;

    binary_paket.uydu_statusu = satellite_status;

    // 5 haneli String hata kodlarini ("10100" gibi) 1 Byte'lik sayiya ceviriyoruz
    binary_paket.hata_kodu_num = StringHataKodunuSayiyaCevir(error_code_str);
    memcpy(binary_paket.rhrhrh, rhrhrh_code_str, 6);

    binary_paket.basinc = s->pressure_pa;
    binary_paket.yukseklik = s->altitude_m;
    binary_paket.inis_hizi = s->vertical_speed_ms;
    binary_paket.sicaklik = s->temperature_c;
    binary_paket.pil_gerilimi = s->battery_voltage_v;

    // GPS double verilerini float (4 byte) olarak kestirip atiyoruz
    binary_paket.gps_enlem = (float)s->gps_latitude;
    binary_paket.gps_boylam = (float)s->gps_longitude;
    binary_paket.gps_yukseklik = s->gps_altitude_m;

    binary_paket.pitch = s->pitch_deg;
    binary_paket.roll = s->roll_deg;
    binary_paket.yaw = s->yaw_deg;

    // 59 Byte'lik ham (binary) structi XBee fonksiyonuna gonderiyoruz
    XBeeAPI_Send(XBEE_DEST_ADDR, (uint8_t *)&binary_paket, sizeof(XBeeTelemetriPaketi_t));
}
