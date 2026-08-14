/*
 * motor.h
 *
 *  LittleBee 30A ESC (PA1 / TIM5_CH2) - yer istasyonundan RPM_ARTIR /
 *  RPM_AZALT komutlariyla kademeli throttle kontrolu. Ucus yazilimina
 *  entegre degil, sadece motor testi icin.
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

#define MOTOR_MIN_US    1000u   /* arm / durma */
#define MOTOR_MAX_US    2000u   /* tam gaz */
#define MOTOR_STEP_US   50u     /* her RPM_ARTIR/AZALT adiminda degisim */

/* MANUEL_SIGMA komutuyla motorun donecegi sabit throttle degeri - buradan degistir */
#define MOTOR_SIGMA_US  1200u

/* ESC'yi arm eder (bloklayan ~3sn bekleme icerir, sadece baslangicta cagir) */
void Motor_Init(void);

void Motor_IncreaseRPM(void);
void Motor_DecreaseRPM(void);

/* MANUEL_SIGMA komutu: motoru MOTOR_SIGMA_US degerinde sabit dondurur */
void Motor_RunSigma(void);

#endif
