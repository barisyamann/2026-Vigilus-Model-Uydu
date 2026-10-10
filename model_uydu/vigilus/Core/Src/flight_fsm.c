/*
 * flight_fsm.c
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */


/*
 * flight_fsm.c - PDR v4.0 "Uçuş Yazılımı Durum Diyagramı" (s.82) birebir
 * uygulanmasi. safety.c'nin ayrilma/APAM/inis tespitini burada statu
 * gecislerine baglar.
 */

#include "flight_fsm.h"
#include "safety.h"
#include "telemetry.h"
#include "main.h"
#include <stdio.h>
#include "buzzer.h"

/* PDR "Irtifa surekli artiyor mu?" / "Dususe gectik mi?" icin sayisal
   esik vermiyor - VARSAYIM, gercek ucus/drop testinde kalibre edilmeli. */
#define ASCENT_CHECK_WINDOW_MS   2000u
#define ASCENT_MIN_GAIN_M        3.0f
#define DESCENT_MIN_LOSS_M       3.0f

/* PDR madde 27: "hasarsiz indikten sonra 10 saniye telemetri devam etmeli",
   durum diyagraminda "1 dakika daha haberlesme sonra sesli ikaz". */
#define RECOVERY_COMMS_MS        60000u

static FlightState_t state = FSM_READY;
static float    trend_ref_altitude = 0.0f;
static uint32_t trend_ref_time = 0;
static uint32_t recovery_start_time = 0;
static uint8_t  buzzer_active = 0;

void FlightFSM_Init(void)
{
    state = FSM_READY;
    trend_ref_altitude = 0.0f;
    trend_ref_time = HAL_GetTick();
    buzzer_active = 0;
}

FlightState_t FlightFSM_GetState(void)
{
    return state;
}

/* ASCENT_CHECK_WINDOW_MS'de bir referans irtifayla simdiki farki doner */
static uint8_t CheckAltitudeTrend(const SensorData_t *s, float *out_diff)
{
    uint32_t now = HAL_GetTick();
    if (now - trend_ref_time >= ASCENT_CHECK_WINDOW_MS)
    {
        *out_diff = s->altitude_m - trend_ref_altitude;
        trend_ref_altitude = s->altitude_m;
        trend_ref_time = now;
        return 1;
    }
    return 0;
}

void FlightFSM_Update(void)
{
    const SensorData_t *s = SensorManager_GetData();
    float diff = 0.0f;
    uint8_t have_diff = CheckAltitudeTrend(s, &diff);

    /*
     * Manuel ayrilma komutu (MANUEL_AYRIL) hangi statude olursak olalim
     * gecerli olmali. QR bench testinde gercek yukselme/inis hic
     * yasanmayacagi icin FSM otonom olarak Statu 1/2'ye hic gecmez -
     * Safety_HasSeparated() true ise ve henuz Ayrilma statusune
     * gecmediysek dogrudan atlariz.
     */
    if (state < FSM_SEPARATION && Safety_HasSeparated())
    {
        state = FSM_SEPARATION;
        Telemetry_SetStatus(3);
        printf("[FSM] Statu 3: Ayrilma (manuel/harici tetikleme)\r\n");
    }

    switch (state)
    {
        case FSM_READY:
            if (have_diff && diff > ASCENT_MIN_GAIN_M)
            {
                state = FSM_ASCENT;
                Telemetry_SetStatus(1);
                printf("[FSM] Statu 1: Yukselme\r\n");
            }
            break;

        case FSM_ASCENT:
            if (have_diff && diff < -DESCENT_MIN_LOSS_M)
            {
                state = FSM_PASSIVE_DESCENT;
                Telemetry_SetStatus(2);
                printf("[FSM] Statu 2: Model Uydu Inis (pasif)\r\n");
            }
            break;

        case FSM_PASSIVE_DESCENT:
            if (Safety_CanTriggerSeparation(s))
            {
                /* TODO: motor gelince gercek ayrilma GPIO/PWM cagrisi buraya */
                Safety_MarkSeparated();
                state = FSM_SEPARATION;
                Telemetry_SetStatus(3);
                printf("[FSM] Statu 3: Ayrilma (otonom tetiklendi)\r\n");
            }
            break;

        case FSM_SEPARATION:
            state = FSM_PAYLOAD_DESCENT;
            Telemetry_SetStatus(4);
            printf("[FSM] Statu 4: Gorev Yuku Inis (SIGMA)\r\n");
            break;

        case FSM_PAYLOAD_DESCENT:
            if (Safety_ShouldTriggerAPAM())
            {
                /* TODO: motor gelince kill-switch + servo tetikleme buraya */
                printf("[FSM] APAM tetiklendi - PID durduruldu\r\n");
            }

            if (Safety_IsLanded())
            {
                state = FSM_RECOVERY;
                Telemetry_SetStatus(5);
                recovery_start_time = HAL_GetTick();
                printf("[FSM] Statu 5: Kurtarma\r\n");
            }
            break;

        case FSM_RECOVERY:
            if (!buzzer_active &&
                (HAL_GetTick() - recovery_start_time) >= RECOVERY_COMMS_MS)
            {
                buzzer_active = 1;
                Buzzer_Init();
                printf("[FSM] Sesli ikaz baslatiliyor\r\n");
            }

            if (buzzer_active)
            {
                static uint32_t last_beep_ms = 0;
                if (HAL_GetTick() - last_beep_ms >= 800u)   /* VARSAYIM: 0.8 saniyede bir beep - daha sik/duyulur */
                {
                    last_beep_ms = HAL_GetTick();
                    Buzzer_Tone(500);   /* 500ms ton */
                }
            }
            break;

    }
}
