/*
 * buzzer.c
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */


/*
 * buzzer.c - PDR madde 28 "yere indikten sonra bulunana kadar sesli ikaz"
 * icin fiziksel arama sinyali. ARAS (hata kodu alarm) ile karistirilmasin -
 * ARAS yer istasyonu tarafinda calisir, bu tamamen ayri bir gereklilik.
 *
 * Buzzer PASIF (PDR "5V Pasif Buzzer") - ic osilatoru yok, sadece GPIO'yu
 * HIGH tutmak ses cikarmaz. Bu yuzden Buzzer_Tone kare dalga uretiyor.
 */

#include "buzzer.h"
#include "main.h"

#define BUZZER_TONE_HALF_PERIOD_US  250   /* ~2 kHz ton */

void Buzzer_Init(void)
{
    Buzzer_Off();
}

void Buzzer_On(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
}

void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

/*
 * Bloklayan kare dalga - pasif buzzer icin gerekli. Kisa sureli (birkac
 * yuz ms) cagrılar icin sorun degil; ana donguyu uzun sure durdurmamak
 * icin cok uzun tutmayin (ornek: 200-500 ms).
 */
void Buzzer_Tone(uint32_t on_ms)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < on_ms)
    {
        HAL_GPIO_TogglePin(BUZZER_GPIO_Port, BUZZER_Pin);
        for (volatile uint32_t i = 0; i < 4200; i++) { __NOP(); }  /* ~250us gecikme, 84MHz'de kaba kalibrasyon */
    }
    Buzzer_Off();
}
