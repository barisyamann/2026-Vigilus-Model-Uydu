/*
 * buzzer.h
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */
#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);

/* Pasif buzzer icin kare dalga uretir - "on_ms" boyunca duyulabilir ton */
void Buzzer_Tone(uint32_t on_ms);

#endif
