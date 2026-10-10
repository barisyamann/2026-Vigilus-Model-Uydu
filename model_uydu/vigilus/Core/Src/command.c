/*
 * command.c
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */


/*
 * command.c - yer istasyonu komut ayristirici.
 * XBee_OnPacket'ten cozulmus RF Data payload'i buraya gelir.
 */

#include "command.h"
#include "safety.h"
#include "telemetry.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lora_iot.h"
#include "servo.h"
#include "motor.h"


#define CMD_MAX_LEN 32

/* PDR ornegi: "2R0G1B" -> rakam,harf,rakam,harf,rakam,harf */
static uint8_t IsRhrhrhFormat(const char *s, uint16_t len)
{
    if (len != 6) return 0;

    for (uint8_t i = 0; i < 6; i += 2)
    {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    for (uint8_t i = 1; i < 6; i += 2)
    {
        if (!isalpha((unsigned char)s[i])) return 0;
    }
    return 1;
}

void Command_Parse(const uint8_t *payload, uint16_t len)
{
    char buf[CMD_MAX_LEN];
    uint16_t n = (len < CMD_MAX_LEN - 1) ? len : (CMD_MAX_LEN - 1);

    memcpy(buf, payload, n);
    buf[n] = '\0';

    /* Sondaki \r\n ve bosluklari temizle */
    while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n' || buf[n - 1] == ' '))
    {
        buf[--n] = '\0';
    }

    if (n == 0) return;

    printf("[CMD] alinan: \"%s\"\r\n", buf);

    if (IsRhrhrhFormat(buf, n))
    {
        /* Bonus-2 S2D-IoT sifresi: telemetriye eklenir, LoRa modulu
           takilinca gercek yonlendirme eklenecek (TODO). */
        Telemetry_SetRhrhrh(buf);

        LoRa_SendRhrhrh(buf);   /* Bonus-2: S2D-IoT istasyonuna 868/470MHz LoRa uzerinden ilet */

        printf("[CMD] RHRHRH sifresi kaydedildi: %s\r\n", buf);
        return;
    }

    if (strcmp(buf, "MANUEL_AYRIL") == 0)
    {
        printf("[CMD] MANUEL_AYRIL - kosulsuz ayrilma tetikleniyor\r\n");
        Servo_Separation_Trigger();
        Safety_MarkSeparated();
        return;
    }


    if (strcmp(buf, "MANUEL_APAM") == 0)
    {
        printf("[CMD] MANUEL_APAM - kosulsuz parasut tetikleniyor\r\n");
        Servo_APAM_Trigger();
        Safety_ManualTriggerAPAM();
        return;
    }

    if (strcmp(buf, "RPM_ARTIR") == 0)
    {
        printf("[CMD] RPM_ARTIR\r\n");
        Motor_IncreaseRPM();
        return;
    }

    if (strcmp(buf, "RPM_AZALT") == 0)
    {
        printf("[CMD] RPM_AZALT\r\n");
        Motor_DecreaseRPM();
        return;
    }

    if (strcmp(buf, "MANUEL_SIGMA") == 0)
    {
        printf("[CMD] MANUEL_SIGMA\r\n");
        Motor_RunSigma();
        return;
    }

    printf("[CMD] TANINMAYAN KOMUT: \"%s\"\r\n", buf);
}
