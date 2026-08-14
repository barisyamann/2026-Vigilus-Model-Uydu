/*
 * servo.h
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */

#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

#define SEPARATION_CLOSED_US   1000u
#define SEPARATION_OPEN_US     2000u

void Servo_Init(void);
void Servo_Separation_Trigger(void);
void Servo_APAM_Trigger(void);

#endif
