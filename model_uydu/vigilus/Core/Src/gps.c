#include "gps.h"
#include <string.h>
#include <stdlib.h>

#define GPS_LINE_BUF_SIZE 128

static char     line_buf[GPS_LINE_BUF_SIZE];
static uint16_t line_idx = 0;
static uint8_t  sats_view_gps = 0;
static uint8_t  sats_view_glonass = 0;

static uint8_t GPS_ChecksumOk(const char *line)
{
    const char *p = line;
    if (*p != '$') return 0;
    p++;

    uint8_t calc = 0;
    while (*p && *p != '*')
    {
        calc ^= (uint8_t)(*p);
        p++;
    }
    if (*p != '*') return 0;

    uint8_t recv = (uint8_t)strtol(p + 1, NULL, 16);
    return calc == recv;
}

/*
 * NMEA cumlesini virgullere gore alanlara boler.
 *
 * strtok KULLANMIYORUZ cunku strtok ardisik ayraclari tek ayrac sayar,
 * yani bos alanlari ("...,,...") atlar ve sonraki tum alan indekslerini
 * kaydirir. NMEA'da bos alan cok yaygin oldugu icin bu sessiz veri
 * bozulmasina yol acar. Bu fonksiyon bos alanlari "" olarak korur.
 */
static uint8_t GPS_SplitFields(char *line, char *field[], uint8_t max_fields)
{
    uint8_t n = 0;
    field[n++] = line;

    for (char *p = line; *p != '\0'; p++)
    {
        if (*p == '*')
        {
            *p = '\0';   // checksum kismini kes
            break;
        }
        if (*p == ',' && n < max_fields)
        {
            *p = '\0';
            field[n++] = p + 1;
        }
    }
    return n;
}

/* "4807.038" + 'N' -> 48.1173  (ddmm.mmmm formatindan ondalik dereceye) */
static double GPS_ToDecimalDegrees(const char *raw, char hemi, uint8_t deg_digits)
{
    if (raw[0] == '\0') return 0.0;

    char deg_str[4] = {0};
    memcpy(deg_str, raw, deg_digits);
    double degrees = (double)atoi(deg_str);
    double minutes = atof(raw + deg_digits);

    double value = degrees + (minutes / 60.0);
    if (hemi == 'S' || hemi == 'W') value = -value;
    return value;
}

static void GPS_ParseRMC(char *line, GPS_Data_t *out)
{
    // $GNRMC,zaman,durum,enlem,N/S,boylam,E/W,hiz,rota,tarih,...
    char *field[13] = {0};
    uint8_t n = GPS_SplitFields(line, field, 13);
    if (n < 7) return;

    out->fix_valid = (field[2][0] == 'A') ? 1 : 0;

    if (strlen(field[1]) >= 6)
    {
        out->hour   = (field[1][0] - '0') * 10 + (field[1][1] - '0');
        out->minute = (field[1][2] - '0') * 10 + (field[1][3] - '0');
        out->second = (field[1][4] - '0') * 10 + (field[1][5] - '0');
    }

    if (out->fix_valid)
    {
        out->latitude  = GPS_ToDecimalDegrees(field[3], field[4][0], 2);
        out->longitude = GPS_ToDecimalDegrees(field[5], field[6][0], 3);
    }
    if (strlen(field[9]) >= 6)
       {
           out->day   = (field[9][0] - '0') * 10 + (field[9][1] - '0');
           out->month = (field[9][2] - '0') * 10 + (field[9][3] - '0');
           out->year  = (field[9][4] - '0') * 10 + (field[9][5] - '0');
       }
}

static void GPS_ParseGGA(char *line, GPS_Data_t *out)
{
    // $GNGGA,zaman,enlem,N/S,boylam,E/W,kalite,uyduSayisi,hdop,irtifa,M,...
    char *field[15] = {0};
    uint8_t n = GPS_SplitFields(line, field, 15);
    if (n < 10) return;

    out->satellites = (uint8_t)atoi(field[7]);
    out->altitude_m = (float)atof(field[9]);
}

static void GPS_ParseGSV(char *line, GPS_Data_t *out, char talker)
{
    // $GPGSV,toplamMesaj,mesajNo,gorunenUyduSayisi,...
    char *field[5] = {0};
    uint8_t n = GPS_SplitFields(line, field, 5);
    if (n < 4) return;

    uint8_t count = (uint8_t)atoi(field[3]);
    if (talker == 'P')      sats_view_gps = count;
    else if (talker == 'L') sats_view_glonass = count;

    out->satellites_in_view = sats_view_gps + sats_view_glonass;
}

static void GPS_ParseLine(char *line, GPS_Data_t *out)
{
    if (!GPS_ChecksumOk(line)) return;

    // "$GNRMC" -> line[1..2] = talker (GN/GP/GL), line[3..5] = cumle tipi
    if (strncmp(&line[3], "RMC", 3) == 0)
    {
        GPS_ParseRMC(line, out);
    }
    else if (strncmp(&line[3], "GGA", 3) == 0)
    {
        GPS_ParseGGA(line, out);
    }
    else if (strncmp(&line[3], "GSV", 3) == 0)
    {
        GPS_ParseGSV(line, out, line[2]);
    }
}

void GPS_Init(void)
{
    line_idx = 0;
    memset(line_buf, 0, sizeof(line_buf));
    sats_view_gps = 0;
    sats_view_glonass = 0;
}

void GPS_ProcessByte(uint8_t byte, GPS_Data_t *out)
{
    if (byte == '\r') return;   // \r'yi yoksay, satiri sadece \n kapatir

    if (byte == '\n')
    {
        line_buf[line_idx] = '\0';
        if (line_idx > 0)
        {
            GPS_ParseLine(line_buf, out);
        }
        line_idx = 0;
        return;
    }

    if (line_idx < (GPS_LINE_BUF_SIZE - 1))
    {
        line_buf[line_idx++] = (char)byte;
    }
    else
    {
        line_idx = 0;   // tasma olursa satiri at, senkronu bozma
    }
}
