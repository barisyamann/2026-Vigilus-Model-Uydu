/*
 * logger.c - SD karta CSV ucus kaydi
 *
 * Tasarim notlari:
 *  - f_write her satirda cagrilir; FatFs zaten kendi 512 baytlik tamponunda
 *    biriktirir, karta ancak tampon dolunca gider.
 *  - f_sync PAHALI ve BLOKE EDICI bir islemdir (ucuz kartlarda 100 ms+).
 *    Her satirda cagirmak ana donguyu kilitler ve bozulma penceresini
 *    buyutur. Saniyede bir yetiyor.
 *  - Kart CIKARILMADAN once Logger_Close cagrilmak ZORUNDA. Aksi halde
 *    FAT ve dizin girdisi yarim kalir, PC dosya sistemini bozuk gorur.
 */

#include "logger.h"
#include "fatfs.h"
#include <stdio.h>
#include <string.h>

static FATFS fs;
static FIL   logFile;
static char  logBuffer[256];
static UINT  bytesWrote;

static uint8_t logger_ready = 0;   /* dosya acik ve yazilabilir mi */

uint8_t Logger_Init(void)
{
    logger_ready = 0;

    FRESULT res = f_mount(&fs, "", 1);
    if (res != FR_OK)
    {
        printf("[LOGGER] Mount hatasi (FRESULT=%d)\r\n", res);
        return 0;
    }

    res = f_open(&logFile, "VIGILUS.csv", FA_OPEN_APPEND | FA_WRITE);
    if (res != FR_OK)
    {
        printf("[LOGGER] Dosya acilamadi (FRESULT=%d)\r\n", res);
        f_mount(NULL, "", 0);
        return 0;
    }

    if (f_size(&logFile) == 0)
    {
        const char *header = "Time_ms,Pres_Pa,Temp_C,Alt_m,VS_ms,"
                             "AccX,AccY,AccZ,GyroX,GyroY,GyroZ,Pitch,Roll,Yaw,"
                             "Fix,Sat_Use,Sat_View,Lat,Lon,GpsAlt,"
                             "GpsDate,GpsTime,V_Bat,I_Bat,Health\n";

        UINT len = (UINT)strlen(header);
        if (f_write(&logFile, header, len, &bytesWrote) != FR_OK || bytesWrote != len)
        {
            printf("[LOGGER] Baslik yazilamadi\r\n");
            f_close(&logFile);
            f_mount(NULL, "", 0);
            return 0;
        }
        f_sync(&logFile);
    }

    logger_ready = 1;
    printf("[LOGGER] VIGILUS.csv hazir (mevcut boyut %lu bayt)\r\n",
           (unsigned long)f_size(&logFile));
    return 1;
}

uint8_t Logger_IsReady(void)
{
    return logger_ready;
}

uint8_t Logger_Write_CSV(const SensorData_t *data)
{
    if (!logger_ready) return 0;

    char dateStr[16], timeStr[16];

    snprintf(dateStr, sizeof(dateStr), "%02u/%02u/20%02u",
             data->gps_day, data->gps_month, data->gps_year);
    snprintf(timeStr, sizeof(timeStr), "%02u:%02u:%02u",
             data->gps_hour, data->gps_minute, data->gps_second);

    int written = snprintf(logBuffer, sizeof(logBuffer),
             "%lu,%.2f,%.2f,%.2f,%.2f,"
             "%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"
             "%u,%u,%u,%.6f,%.6f,%.1f,"
             "%s,%s,%.2f,%.3f,0x%02X\n",
             (unsigned long)data->timestamp_ms,
             data->pressure_pa, data->temperature_c,
             data->altitude_m, data->vertical_speed_ms,
             data->accel_x_g, data->accel_y_g, data->accel_z_g,
             data->gyro_x_dps, data->gyro_y_dps, data->gyro_z_dps,
             data->pitch_deg, data->roll_deg, data->yaw_deg,
             data->gps_fix, data->gps_satellites, data->gps_sats_view,
             data->gps_latitude, data->gps_longitude, data->gps_altitude_m,
             dateStr, timeStr,
             data->battery_voltage_v, data->current_a,
             data->health_flags);

    /* snprintf tampona sigmadiysa satir yarim kalmistir - yazma */
    if (written < 0 || (size_t)written >= sizeof(logBuffer))
    {
        printf("[LOGGER] Satir tampona sigmadi (%d bayt)\r\n", written);
        return 0;
    }

    UINT len = (UINT)written;

    if (f_write(&logFile, logBuffer, len, &bytesWrote) != FR_OK)
    {
        printf("[LOGGER] f_write hatasi\r\n");
        return 0;
    }

    if (bytesWrote != len)
    {
        /* Kisa yazim: kart dolu olabilir. Sessizce gecersek CSV kayar. */
        printf("[LOGGER] Kisa yazim %u/%u - loglama durduruldu\r\n",
               (unsigned)bytesWrote, (unsigned)len);
        logger_ready = 0;
        return 0;
    }

    /* Sync pahali: saniyede bir yeter */
    static uint32_t last_sync = 0;
    if (HAL_GetTick() - last_sync >= 1000)
    {
        last_sync = HAL_GetTick();
        if (f_sync(&logFile) != FR_OK)
        {
            printf("[LOGGER] f_sync hatasi\r\n");
            return 0;
        }
    }

    return 1;
}

uint8_t Logger_Close(void)
{
    if (!logger_ready) return 0;

    logger_ready = 0;   /* once kapat ki paralel yazim olmasin */

    if (f_close(&logFile) != FR_OK)
    {
        printf("[LOGGER] f_close hatasi\r\n");
        return 0;
    }

    f_mount(NULL, "", 0);   /* birimi ayir - kart artik cikarilabilir */
    printf("[LOGGER] Dosya kapatildi, kart guvenle cikarilabilir\r\n");
    return 1;
}

void Logger_DumpHead(void)
{
    FIL  f;
    char line[128];
    UINT n = 0;

    if (f_open(&f, "VIGILUS.csv", FA_READ) != FR_OK)
    {
        printf("[LOGGER] Dogrulama: dosya acilamadi\r\n");
        return;
    }

    printf("[LOGGER] Dogrulama: boyut = %lu bayt\r\n", (unsigned long)f_size(&f));

    if (f_read(&f, line, sizeof(line) - 1, &n) == FR_OK)
    {
        line[n] = '\0';
        printf("[LOGGER] Ilk %u bayt:\r\n%s\r\n", (unsigned)n, line);
    }

    f_close(&f);
}
