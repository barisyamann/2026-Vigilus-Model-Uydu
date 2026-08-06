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

#include "telemetry.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart3;

static uint32_t packet_counter = 0;
static uint8_t  satellite_status = 0;
static char     error_code[8]  = "00000";
static char     rhrhrh_code[8] = "000000";

void Telemetry_Init(void)
{
    packet_counter = 0;
    satellite_status = 0;
    strcpy(error_code, "00000");
    strcpy(rhrhrh_code, "000000");
}

void Telemetry_SetStatus(uint8_t status)
{
    satellite_status = status;
}

void Telemetry_SetErrorCode(const char *code)
{
    strncpy(error_code, code, sizeof(error_code) - 1);
    error_code[sizeof(error_code) - 1] = '\0';
}

void Telemetry_SetRhrhrh(const char *code)
{
    strncpy(rhrhrh_code, code, sizeof(rhrhrh_code) - 1);
    rhrhrh_code[sizeof(rhrhrh_code) - 1] = '\0';
}

uint32_t Telemetry_GetPacketCount(void)
{
    return packet_counter;
}

void Telemetry_Send(const SensorData_t *s)
{
    char packet[256];

    packet_counter++;

    /*
     * Tarih-saat: GPS fix varsa gercek UTC zamani, yoksa placeholder.
     * NOT: Sartname, islemci yeniden baslasa bile zaman verisinin
     * korunmasini istiyor (istek 17). GPS zamani bu sarti dogal olarak
     * saglar cunku disaridan gelir - ama fix yokken bosluk olusur.
     * Faz 10'da RTC yedek register'lariyla bu bosluk kapatilacak.
     */
    int len = snprintf(packet, sizeof(packet),
        "%lu,%u,%s,%02u/%02u/20%02u %02u:%02u:%02u,"
        "%.1f,%.1f,%.2f,%.1f,%.2f,"
        "%.6f,%.6f,%.1f,"
        "%.1f,%.1f,%.1f,"
        "%s,%lu\r\n",
        packet_counter,
        satellite_status,
        error_code,
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
        rhrhrh_code,
        (unsigned long)TEAM_NUMBER);

    if (len > 0)
    {
        HAL_UART_Transmit(&huart3, (uint8_t *)packet, (uint16_t)len, 1000);
    }
}
