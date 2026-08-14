/*
 * motor.c
 *
 *  2x LittleBee 30A ESC kontrolu (PA1/TIM5_CH2 ve PB4/TIM3_CH1). Sadece
 *  PWM+GND baglantili, standart 1000-2000us / 50Hz ESC sinyali uretir.
 *  Iki motor da ayni throttle degerini paylasir, tum komutlar (arm,
 *  RPM_ARTIR/AZALT, SIGMA) ikisini birden ayni anda etkiler.
 */

#include "motor.h"
#include "main.h"
#include <stdio.h>

extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim3;

static uint16_t current_us = MOTOR_MIN_US;

static void SetThrottleUs(uint16_t us)
{
    if (us < MOTOR_MIN_US) us = MOTOR_MIN_US;
    if (us > MOTOR_MAX_US) us = MOTOR_MAX_US;
    current_us = us;
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_2, current_us);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, current_us);
}

void Motor_Init(void)
{
    HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

    /* ESC arm sirasi: minimum throttle'i bir sure sabit gonder */
    SetThrottleUs(MOTOR_MIN_US);
    HAL_Delay(3000);
    printf("[MOTOR] Her iki ESC arm edildi.\r\n");
}

void Motor_IncreaseRPM(void)
{
    SetThrottleUs(current_us + MOTOR_STEP_US);
    printf("[MOTOR] RPM artirildi -> %u us (iki motor)\r\n", current_us);
}

void Motor_DecreaseRPM(void)
{
    SetThrottleUs(current_us - MOTOR_STEP_US);
    printf("[MOTOR] RPM azaltildi -> %u us (iki motor)\r\n", current_us);
}

void Motor_RunSigma(void)
{
    SetThrottleUs(MOTOR_SIGMA_US);
    printf("[MOTOR] SIGMA - sabit %u us (iki motor)\r\n", current_us);
}
