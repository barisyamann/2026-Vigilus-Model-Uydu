/*
 * safety.h
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */
#ifndef SAFETY_H
#define SAFETY_H

#include <stdint.h>
#include "SensorManager.h"

/*
 * PDR v4.0 (Kategori-SIGMA) kaynakli sabitler. Kaynakta acikca sayisal
 * verilmeyen degerler VARSAYIM olarak isaretlenmistir - takim onayi gerekir.
 */

/* Ayrilma (Separation): PDR Yapilan Isler madde 6 - "1000 (+/-10m) metre" */
#define SEPARATION_ALTITUDE_M       1000.0f
#define SEPARATION_ALT_TOLERANCE_M  10.0f    /* PDR'de KESIN deger, varsayim degil */
#define FREEFALL_MIN_VSPEED_MS      3.0f     /* VARSAYIM: serbest dusus alt siniri */

/* SIGMA hedef inis hizi: PDR madde 9 "8-10 m/s" (ayrilma sonrasi, aktif inis) */
#define SIGMA_TARGET_VSPEED_MIN_MS  8.0f
#define SIGMA_TARGET_VSPEED_MAX_MS  10.0f

/* Pasif inis (0-1000m) hiz araligi: PDR madde 5 "12-14 m/s" - sadece bilgi/
   sanity-check amacli, ayri bir tetikleme mantigi gerektirmiyor. */
#define PASSIVE_DESCENT_VSPEED_MIN_MS  12.0f
#define PASSIVE_DESCENT_VSPEED_MAX_MS  14.0f

/* APAM (Acil Parasut): PDR madde 10 "16 m/s ve uzeri, 10 saniye surerse,
   100 metre yuksekligin UZERINDE KESIN bir sekilde acilmali" */
#define APAM_TRIGGER_SPEED_MS       16.0f
#define APAM_TRIGGER_DURATION_MS    10000u
#define APAM_MIN_SAFE_ALTITUDE_M    100.0f   /* PDR'de KESIN deger */
#define APAM_ALTITUDE_MARGIN_M      30.0f    /* VARSAYIM: 100m'ye erken tepki icin
                                                 guvenlik payi - drop testinde
                                                 gercek degerle kalibre edilmeli */

/* Kill-switch siralamasi: PDR "saliseler once motor PWM %0'a iner" */
#define KILL_SWITCH_LEAD_MS         50u      /* VARSAYIM: "saliseler" icin 50 ms */

/* IMU/sensor watchdog: CDR s.17 "100 ms boyunca veri akmazsa" */
#define IMU_TIMEOUT_MS               100u

/* Inis (yere ulasma) tespiti: CDR "hiz=0, ani G-soku sonumlenmesi",
   PDR madde 27 "hasarsiz indikten sonra 10 saniye telemetri devam etmeli" */
#define LANDING_VSPEED_MAX_MS        0.5f    /* VARSAYIM: "sifir" icin tolerans */
#define LANDING_ACCEL_SETTLE_MIN_G   0.8f
#define LANDING_ACCEL_SETTLE_MAX_G   1.2f
#define LANDING_SETTLE_MS            500u    /* VARSAYIM: bu sure boyunca sabit kalmali */

/*
 * Ucus fazi kilidi: PDR "Hatalı Tetikleme Analizi" - APAM sadece ayrilma
 * SONRASI aktif olmali. Ayrilma gerceklesmeden APAM kontrolu hic calismamali.
 */
typedef enum
{
    FLIGHT_PHASE_PRE_SEPARATION = 0,   /* yukselme + 1000m'ye pasif inis */
    FLIGHT_PHASE_POST_SEPARATION       /* ayrilma sonrasi SIGMA/APAM aktif */
} FlightPhase_t;

void    Safety_Init(void);
void    Safety_MarkSeparated(void);              /* ayrilma gerceklesince cagir */
void    Safety_Update(const SensorData_t *s);    /* her dongude cagir */
void Safety_ManualTriggerAPAM(void);

uint8_t Safety_CanTriggerSeparation(const SensorData_t *s);
uint8_t Safety_ShouldTriggerAPAM(void);
uint8_t Safety_IsLanded(void);
uint8_t Safety_IsIMUHealthy(const SensorData_t *s, uint32_t now_ms);
uint8_t Safety_HasSeparated(void);

#endif
