/*
 * aras.c
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */


/*
 * aras.c - ARAS (Arayuz Alarm Sistemi) icin STM32 tarafi: 5 haneli hata
 * kodunu uretir, telemetriye yazar. Alarm gorsellestirmesi/sesi PC
 * tarafindaki yer istasyonu GUI'sinde (winsound) yapilir - bu dosya
 * sadece dogru kodu uretmekle sorumlu.
 *
 * NOT: PDR'nin ARAS bolumu "4 haneli" diyor, Telemetri Formati tablosu
 * "5 haneli" diyor (rapor ici tutarsizlik). telemetry.c zaten 5 karakterlik
 * alan kullaniyor, o yuzden 5 hane ile devam ediliyor. Ilk 4 hane PDR'nin
 * ARAS ekran goruntusundeki sirayla (Inis Hizi, Konum, Ayrilma, Parasut)
 * eslesir, 5. hane genel sensor sagligi icin eklendi.
 *
 * Her hane: '1' = alarm aktif, '0' = normal.
 */

#include "aras.h"
#include "safety.h"
#include "flight_fsm.h"
#include "telemetry.h"
#include "SensorManager.h"

/* VARSAYIM: ayrilma icin "cok gec kalindi" esigi - takim onayi gerekir */
#define SEPARATION_LATE_MARGIN_M   50.0f
/* VARSAYIM: SIGMA hedef bandi disinda ne kadar tolerans - takim onayi gerekir */
#define VSPEED_ALARM_MARGIN_MS     2.0f

void ARAS_Update(void)
{
    const SensorData_t *s = SensorManager_GetData();
    FlightState_t fsm_state = FlightFSM_GetState();
    char code[6];

    /* Hane 0: Inis Hizi - SIGMA fazinda hedef bandin (8-10 m/s) disinda */
    uint8_t d0 = 0;
    if (fsm_state == FSM_PAYLOAD_DESCENT)
    {
        if (s->vertical_speed_ms < SIGMA_TARGET_VSPEED_MIN_MS - VSPEED_ALARM_MARGIN_MS ||
            s->vertical_speed_ms > SIGMA_TARGET_VSPEED_MAX_MS + VSPEED_ALARM_MARGIN_MS)
        {
            d0 = 1;
        }
    }

    /* Hane 1: Konum - GPS verisi kayip */
    uint8_t d1 = !(s->health_flags & SENSOR_OK_GPS);

    /* Hane 2: Ayrilma - hedef irtifayi cok gecmisiz ama hala ayrilmadik */
    uint8_t d2 = 0;
    if (fsm_state == FSM_PASSIVE_DESCENT &&
        s->altitude_m < (SEPARATION_ALTITUDE_M - SEPARATION_LATE_MARGIN_M))
    {
        d2 = 1;
    }

    /* Hane 3: Parasut - APAM (acil durum) tetiklendi */
    uint8_t d3 = Safety_ShouldTriggerAPAM();

    /* Hane 4: Genel sensor sagligi - cekirdek sensorlerden biri down */
    uint8_t d4 = !((s->health_flags & SENSOR_OK_BMP388) &&
                   (s->health_flags & SENSOR_OK_MPU9250));

    code[0] = (char)('0' + d0);
    code[1] = (char)('0' + d1);
    code[2] = (char)('0' + d2);
    code[3] = (char)('0' + d3);
    code[4] = (char)('0' + d4);
    code[5] = '\0';

    Telemetry_SetErrorCode(code);
}
