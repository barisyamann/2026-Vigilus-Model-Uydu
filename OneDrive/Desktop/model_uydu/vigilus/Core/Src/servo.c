/*
 * servo.c
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */

#include "servo.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;

/*
 * Aciliste CCR bilerek 0'da birakiliyor (gecersiz sinyal - servo tork
 * uygulamiyor). Boylece servolar guc verilmeden onceki fiziksel konumlarinda
 * kalir, ilk komuta kadar hicbir yone zipmazlar.
 */
void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
}

/* Demo/sunum icin: her cagrida acik<->kapali arasinda gidip gelir (tek seferlik degil) */
void Servo_Separation_Trigger(void)
{
    static uint8_t open = 0;
    open = !open;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, open ? SEPARATION_OPEN_US : SEPARATION_CLOSED_US);
}

void Servo_APAM_Trigger(void)
{
    static uint8_t open = 0;
    open = !open;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, open ? SEPARATION_OPEN_US : SEPARATION_CLOSED_US);
}
