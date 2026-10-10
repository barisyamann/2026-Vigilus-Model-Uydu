/*
 * safety.c
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */

/*
 * safety.c - Yanlis tetiklenme koruma mantigi (PDR v4.0 + CDR v3.2 temelli).
 *
 * Bu dosya SAF MANTIK katmanidir - hicbir GPIO/PWM cagrisi yapmaz. Motor
 * suruculeri geldiginde, buradaki fonksiyonlar "true" dondugunde gercek
 * donanim tetiklemesi eklenecek. Simdilik sadece karar veriyor.
 */

#include "safety.h"
#include "main.h"
#include <math.h>

typedef enum { APAM_ARMED = 0, APAM_COUNTING, APAM_TRIGGERED } ApamState_t;

static ApamState_t   apam_state   = APAM_ARMED;
static uint32_t      apam_start_ms = 0;
static FlightPhase_t flight_phase  = FLIGHT_PHASE_PRE_SEPARATION;

static uint32_t landing_settle_start_ms = 0;
static uint8_t  landing_confirmed = 0;

static uint32_t last_imu_ok_ms = 0;

void Safety_Init(void)
{
    apam_state   = APAM_ARMED;
    apam_start_ms = 0;
    flight_phase  = FLIGHT_PHASE_PRE_SEPARATION;
    landing_settle_start_ms = 0;
    landing_confirmed = 0;
    last_imu_ok_ms = 0;
}

void Safety_MarkSeparated(void)
{
    flight_phase = FLIGHT_PHASE_POST_SEPARATION;
}

/*
 * PDR "Yapilan Isler" md.6: "1000 (+/-10m) metre yukseklikte... otonom
 * olarak ayrilmali". Ikinci dogrulama kaynagi olarak GPS irtifasini
 * kullaniyoruz; GPS fix yoksa cift dogrulama saglanamadigi icin
 * tetiklemiyoruz - tek sensore guvenmek PDR'nin acikca yasakladigi sey
 * (Hatali Tetikleme Analizi md.1: "sensor fuzyonu ile capraz dogrulama").
 */
uint8_t Safety_CanTriggerSeparation(const SensorData_t *s)
{
    if (flight_phase != FLIGHT_PHASE_PRE_SEPARATION) return 0;  /* zaten ayrildi */

    if (!(s->health_flags & SENSOR_OK_BMP388)) return 0;

    float alt_diff = fabsf(s->altitude_m - SEPARATION_ALTITUDE_M);
    if (alt_diff > SEPARATION_ALT_TOLERANCE_M) return 0;

    uint8_t baro_freefall = (s->vertical_speed_ms > FREEFALL_MIN_VSPEED_MS);
    if (!baro_freefall) return 0;

    if (!s->gps_fix) return 0;

    float gps_diff = fabsf(s->gps_altitude_m - SEPARATION_ALTITUDE_M);
    if (gps_diff > SEPARATION_ALT_TOLERANCE_M * 2.0f)   /* GPS irtifasi daha az hassas */
        return 0;

    return 1;
}

void Safety_Update(const SensorData_t *s)
{
    uint32_t now = HAL_GetTick();

    /*
     * PDR "Hatalı Tetikleme Analizi" md.2 - State Machine Durum Kilidi:
     * APAM mantigi ayrilma GERCEKLESMEDEN kesinlikle degerlendirilmez.
     * Yukselis veya 1000m'ye pasif inis sirasinda 16 m/s'yi asan bir
     * olcum (sensor gurultusu, kalkis ivmesi vb.) yanlislikla paraşütü
     * acmaya calismasin diye tum APAM sayaci burada dondurulur.
     */
    if (flight_phase != FLIGHT_PHASE_POST_SEPARATION)
    {
        apam_state = APAM_ARMED;
        return;
    }

    /* --- APAM sayaci: PDR md.10 "16 m/s uzerinde 10 saniye surekli" --- */
    if (s->vertical_speed_ms >= APAM_TRIGGER_SPEED_MS)
    {
        if (apam_state == APAM_ARMED)
        {
            apam_state = APAM_COUNTING;
            apam_start_ms = now;
        }
        else if (apam_state == APAM_COUNTING &&
                 (now - apam_start_ms) >= APAM_TRIGGER_DURATION_MS)
        {
            apam_state = APAM_TRIGGERED;
        }
    }
    else
    {
        /* PDR "9. saniyedeki hizin 15.3 m/s'e dusmesinde bile sayac
           ANINDA sifirlanir" - erken düşüş yanlış tetiklemeyi önler. */
        if (apam_state == APAM_COUNTING)
        {
            apam_state = APAM_ARMED;
        }
    }

    /*
     * PDR md.10: "100 metre yuksekligin UZERINDE KESIN bir sekilde
     * acilmalidir." 10 saniyelik sayacin tam dolmasini beklemek, dusuk
     * irtifada baslayan bir arizada zemine carpmadan once acilmayi
     * garanti etmeyebilir. Bu yuzden irtifa 100m + guvenlik payina
     * dustugunde ve hala tehlikeli hizda inilirken, sayaç dolmamis olsa
     * bile ACIL tetikleme yapiyoruz - bu bagimsiz bir ikinci yol.
     * NOT: APAM_ALTITUDE_MARGIN_M drop testinde kalibre edilmeli.
     */
    if (apam_state != APAM_TRIGGERED &&
        s->altitude_m <= (APAM_MIN_SAFE_ALTITUDE_M + APAM_ALTITUDE_MARGIN_M) &&
        s->vertical_speed_ms >= APAM_TRIGGER_SPEED_MS)
    {
        apam_state = APAM_TRIGGERED;
    }

    /* --- Inis tespiti: CDR "hiz=0, ani G-soku sonumlenmesi" --- */
    float accel_mag = sqrtf(s->accel_x_g * s->accel_x_g +
                             s->accel_y_g * s->accel_y_g +
                             s->accel_z_g * s->accel_z_g);

    uint8_t vspeed_ok = (fabsf(s->vertical_speed_ms) <= LANDING_VSPEED_MAX_MS);
    uint8_t accel_ok  = (accel_mag >= LANDING_ACCEL_SETTLE_MIN_G &&
                         accel_mag <= LANDING_ACCEL_SETTLE_MAX_G);

    if (!landing_confirmed)
    {
        if (vspeed_ok && accel_ok)
        {
            if (landing_settle_start_ms == 0)
            {
                landing_settle_start_ms = now;
            }
            else if ((now - landing_settle_start_ms) >= LANDING_SETTLE_MS)
            {
                landing_confirmed = 1;
            }
        }
        else
        {
            landing_settle_start_ms = 0;   /* kosul bozuldu, yeniden basla */
        }
    }

    /* --- IMU tazelik takibi --- */
    if (s->health_flags & SENSOR_OK_MPU9250)
    {
        last_imu_ok_ms = now;
    }
}

uint8_t Safety_ShouldTriggerAPAM(void)
{
    return (apam_state == APAM_TRIGGERED);
}

uint8_t Safety_IsLanded(void)
{
    return landing_confirmed;
}

uint8_t Safety_IsIMUHealthy(const SensorData_t *s, uint32_t now_ms)
{
    (void)s;
    if (last_imu_ok_ms == 0) return 0;
    return ((now_ms - last_imu_ok_ms) < IMU_TIMEOUT_MS);
}
/*
 * Yer istasyonundan MANUEL_APAM komutu: PDR APAM bolumu "Otonom sensor
 * sartlari aranmaksizin... mekanizmanin aninda acilabilmesi" gerektiriyor.
 * Bu yuzden flight_phase kontrolu YAPILMAZ - koşulsuz tetikler.
 */
void Safety_ManualTriggerAPAM(void)
{
    apam_state = APAM_TRIGGERED;
}
uint8_t Safety_HasSeparated(void)
{
    return (flight_phase == FLIGHT_PHASE_POST_SEPARATION);
}

