/*
 * SensorManager.c
 *
 *  Created on: 4 Ağu 2026
 *      Author: Barış
 */

#include "SensorManager.h"
#include "main.h"
#include "bmp388.h"
#include "mpu9250.h"
#include "ina226.h"
#include "gps.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define RAD_TO_DEG          57.29577951f

/* Her sensorun okuma periyodu (ms) - bloklamadan, kendi hizinda */
#define PERIOD_IMU_MS       20     // 50 Hz
#define PERIOD_BARO_MS      50     // 20 Hz
#define PERIOD_POWER_MS     500    // 2 Hz (batarya yavas degisir)

/* Dikey hiz alcak gecirgen filtre katsayisi (0-1, kucuk = daha cok filtre) */
#define VSPEED_FILTER_ALPHA 0.3f
/* Ivme tabanli dikey hiz tahmini icin agirlik - VARSAYIM, PDR test
   planindaki "Terminal Hiz ve Inis Testi" (drone ile serbest birakma)
   sirasinda gercek deger ile kalibre edilmeli */
#define VSPEED_COMPLEMENTARY_ALPHA 0.98f
#define GRAVITY_MS2 9.81f


/* Complementary filter agirligi: jiroskopa ne kadar guvenilecek (0-1).
   Yuksek deger = jiroskop agirlikli (hizli tepki, uzun vadede az kayma
   duzeltmesi); dusuk deger = ivmeolcer agirlikli (yavas ama mutlak referans). */
#define COMPLEMENTARY_ALPHA 0.98f

/* Bu sure boyunca GPS'ten byte gelmezse modul ariza sayilir */
#define GPS_TIMEOUT_MS      2000

static SensorData_t data;
static volatile GPS_Data_t gps_raw;   // kesme icinden yazildigi icin volatile

static uint32_t last_imu_ms   = 0;
static uint32_t last_baro_ms  = 0;
static uint32_t last_power_ms = 0;

static float    prev_altitude_m  = 0.0f;
static uint32_t prev_alt_time_ms = 0;

static volatile uint32_t last_gps_byte_ms = 0;

uint8_t SensorManager_Init(void)
{
    memset(&data, 0, sizeof(data));
    memset((void *)&gps_raw, 0, sizeof(gps_raw));

    GPS_Init();

    if (BMP388_Init())
    {
        data.health_flags |= SENSOR_OK_BMP388;
        printf("[SM] BMP388 hazir\r\n");
        BMP388_SetGroundReference(40);
    }
    else
    {
        printf("[SM] BMP388 BASARISIZ\r\n");
    }

    if (MPU9250_Init())
    {
        data.health_flags |= SENSOR_OK_MPU9250;
        printf("[SM] MPU9250 hazir, kalibrasyon (sabit tutun)...\r\n");
        MPU9250_CalibrateGyro(500);
    }
    else
    {
        printf("[SM] MPU9250 BASARISIZ\r\n");
    }

    if (INA226_Init())
    {
        data.health_flags |= SENSOR_OK_INA226;
        printf("[SM] INA226 hazir\r\n");
    }
    else
    {
        printf("[SM] INA226 BASARISIZ\r\n");
    }

    prev_alt_time_ms = HAL_GetTick();
    return data.health_flags;
}

void SensorManager_FeedGpsByte(uint8_t byte)
{
    last_gps_byte_ms = HAL_GetTick();
    GPS_ProcessByte(byte, (GPS_Data_t *)&gps_raw);
}

static void UpdateAttitude(void)
{
    /*
     * Ivmeolcerden ani (anlik) aci tahmini - dogru ama titresim/ivme altinda
     * gurultulu. Jiroskoptan integre edilen aci - kisa vadede duzgun ama
     * zamanla kayar. Complementary filter ikisini birlestirir: jiroskopun
     * hizli tepkisini kullanir, ivmeolcerle uzun vadeli referansa cekilir.
     */
    float ax = data.accel_x_g, ay = data.accel_y_g, az = data.accel_z_g;

    float accel_roll  = atan2f(ay, az) * RAD_TO_DEG;
    float accel_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;

    static uint32_t last_att_ms = 0;
    uint32_t now_att = HAL_GetTick();

    if (last_att_ms == 0)
    {
        /* Ilk calisma: integrasyon icin onceki deger yok, dogrudan ivmeolcerle basla */
        data.roll_deg  = accel_roll;
        data.pitch_deg = accel_pitch;
    }
    else
    {
        float dt = (now_att - last_att_ms) / 1000.0f;

        float gyro_roll  = data.roll_deg  + data.gyro_x_dps * dt;
        float gyro_pitch = data.pitch_deg + data.gyro_y_dps * dt;

        data.roll_deg  = COMPLEMENTARY_ALPHA * gyro_roll
                        + (1.0f - COMPLEMENTARY_ALPHA) * accel_roll;
        data.pitch_deg = COMPLEMENTARY_ALPHA * gyro_pitch
                        + (1.0f - COMPLEMENTARY_ALPHA) * accel_pitch;
    }
    last_att_ms = now_att;

    /*
     * Yaw: manyetometre olmadigi icin sadece jiroskop Z ekseni integrali.
     * Zamanla KAYAR (drift), mutlak yon bilgisi degildir. Gorev icin kritik
     * degil - yaw sonumleme diferansiyel RPM ile yapiliyor.
     */
    static uint32_t last_yaw_ms = 0;
    uint32_t now_yaw = HAL_GetTick();
    if (last_yaw_ms != 0)
    {
        float dt = (now_yaw - last_yaw_ms) / 1000.0f;
        data.yaw_deg += data.gyro_z_dps * dt;

        data.yaw_deg = fmodf(data.yaw_deg, 360.0f);
        if (data.yaw_deg < 0.0f) data.yaw_deg += 360.0f;
    }
    last_yaw_ms = now_yaw;
}

static void UpdateVerticalSpeed(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t dt_ms = now - prev_alt_time_ms;

    if (dt_ms < 100) return;   // cok kisa araliklarda turev gurultuyu buyutur

    float dt_s = dt_ms / 1000.0f;
    float raw_baro_vspeed = (prev_altitude_m - data.altitude_m) / dt_s;  // alcalma = pozitif

    /*
     * Ivmeolcer tabanli tahmin: govde eksenindeki ivmeyi pitch/roll ile
     * dunya (dikey) eksenine projekte ediyoruz, durgun haldeki +1g
     * referansini cikarip gercek ivmeyi buluyoruz, sonra entegre
     * ediyoruz. Kisa vadede duzgun (dusuk gurultulu) ama zamanla kayar -
     * barometre ile complementary filter'a sokuluyor, UpdateAttitude'daki
     * pitch/roll fuzyonuyla ayni mimari.
     *
     * VARSAYIM: rotasyon formulunun isareti bu projenin ozel eksen
     * duzeltmesiyle (X ekseni 180 derece ters montaj) dogrulanmadi.
     * Asagidaki [VSPEED-TANI] ciktisiyla elle kontrol et: kartı hafifce
     * YUKARI kaldirinca "fused" gecici olarak NEGATIF, AŞAĞI indirince
     * gecici olarak POZITIF olmali (alcalma=pozitif kuralimiza gore).
     */
    float pitch_rad = data.pitch_deg / RAD_TO_DEG;
    float roll_rad  = data.roll_deg  / RAD_TO_DEG;

    float accel_world_up_g =
          (-sinf(pitch_rad) * data.accel_x_g)
        + ( sinf(roll_rad) * cosf(pitch_rad) * data.accel_y_g)
        + ( cosf(roll_rad) * cosf(pitch_rad) * data.accel_z_g);

    float accel_true_ms2 = (accel_world_up_g - 1.0f) * GRAVITY_MS2;

    static float accel_vspeed_ms = 0.0f;
    accel_vspeed_ms -= accel_true_ms2 * dt_s;   // yukari ivme -> azalan (dusme) hiz

    float fused = VSPEED_COMPLEMENTARY_ALPHA * accel_vspeed_ms
                + (1.0f - VSPEED_COMPLEMENTARY_ALPHA) * raw_baro_vspeed;

    accel_vspeed_ms = fused;   // entegrasyonu barometreye dogru cek (drift duzeltmesi)

    /* GECICI TANI: raw barometre ile fuzyonu yan yana bas - elle
       yukari/asagi hareket ettirip isareti dogrula. Is bitince silinecek. */


    data.vertical_speed_ms = VSPEED_FILTER_ALPHA * fused
                             + (1.0f - VSPEED_FILTER_ALPHA) * data.vertical_speed_ms;

    prev_altitude_m  = data.altitude_m;
    prev_alt_time_ms = now;
}


void SensorManager_Update(void)
{
    uint32_t now = HAL_GetTick();

    /* --- IMU --- */
    /* --- IMU --- */
    if ((now - last_imu_ms) >= PERIOD_IMU_MS)
    {
        last_imu_ms = now;
        MPU9250_Data_t imu;
        if (MPU9250_ReadData(&imu))
        {
            /*
             * Kart breadboard'a X ekseni etrafinda 180 derece ters monte
             * edilmis: X ayni kalir, Y ve Z isaret degistirir. Duzeltmeyi
             * TEK YERDE, ham veriyi SensorData_t'ye yazarken yapiyoruz ki
             * UpdateAttitude ve ileride complementary filter hep dogru
             * eksenlerle calissin.
             */
            data.accel_x_g  =  imu.accel_x_g;
            data.accel_y_g  = -imu.accel_y_g;
            data.accel_z_g  = -imu.accel_z_g;
            data.gyro_x_dps =  imu.gyro_x_dps;
            data.gyro_y_dps = -imu.gyro_y_dps;
            data.gyro_z_dps = -imu.gyro_z_dps;

            UpdateAttitude();
            data.health_flags |= SENSOR_OK_MPU9250;
        }
        else
        {
            data.health_flags &= ~SENSOR_OK_MPU9250;
        }
    }

    /* --- Barometre --- */
    if ((now - last_baro_ms) >= PERIOD_BARO_MS)
    {
        last_baro_ms = now;
        BMP388_Data_t baro;
        BMP388_ReadData(&baro);

        if (baro.pressure_pa > 30000.0f && baro.pressure_pa < 120000.0f)
        {
            data.pressure_pa   = baro.pressure_pa;
            data.temperature_c = baro.temperature_c;
            data.altitude_m    = baro.altitude_m;
            UpdateVerticalSpeed();
            data.health_flags |= SENSOR_OK_BMP388;
        }
        else
        {
            data.health_flags &= ~SENSOR_OK_BMP388;
        }
    }

    /* --- Guc --- */
    if ((now - last_power_ms) >= PERIOD_POWER_MS)
    {
        last_power_ms = now;
        INA226_Data_t pwr;
        if (INA226_ReadData(&pwr))
        {
            data.battery_voltage_v = pwr.bus_voltage_v;
            data.current_a         = pwr.current_a;
            data.health_flags |= SENSOR_OK_INA226;
            printf("[INA226 DEBUG] bus=%.3fV shunt=%.3fmV current=%.3fA power=%.3fW\r\n",
                   pwr.bus_voltage_v, pwr.shunt_voltage_mv, pwr.current_a, pwr.power_w);
        }
        else
        {
            data.health_flags &= ~SENSOR_OK_INA226;
        }
    }

    /*
     * --- GPS ---
     * Veri UART kesmesi icinden dolduruluyor. Kopyalama sirasinda kesme
     * gelirse yarim guncellenmis veri okunabilir (yaris durumu), bu yuzden
     * kisa bir kritik bolumde atomik snapshot aliyoruz.
     */
    GPS_Data_t gps_snapshot;
    uint32_t   gps_last_byte;

    __disable_irq();
    gps_snapshot  = *(GPS_Data_t *)&gps_raw;
    gps_last_byte = last_gps_byte_ms;
    __enable_irq();

    data.gps_fix        = gps_snapshot.fix_valid;
    data.gps_satellites = gps_snapshot.satellites;
    data.gps_sats_view  = gps_snapshot.satellites_in_view;
    data.gps_latitude   = gps_snapshot.latitude;
    data.gps_longitude  = gps_snapshot.longitude;
    data.gps_altitude_m = gps_snapshot.altitude_m;
    data.gps_hour       = gps_snapshot.hour;
    data.gps_minute     = gps_snapshot.minute;
    data.gps_second     = gps_snapshot.second;
    data.gps_month 		= gps_snapshot.month;
    data.gps_day		= gps_snapshot.day;
    data.gps_year		= gps_snapshot.year;

    if (gps_last_byte != 0 && (now - gps_last_byte) < GPS_TIMEOUT_MS)
        data.health_flags |= SENSOR_OK_GPS;
    else
        data.health_flags &= ~SENSOR_OK_GPS;

    data.timestamp_ms = now;
}

const SensorData_t* SensorManager_GetData(void)
{
    return &data;
}
