/*
 * flight_fsm.h
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */
#ifndef FLIGHT_FSM_H
#define FLIGHT_FSM_H

#include <stdint.h>
#include "SensorManager.h"

typedef enum
{
    FSM_READY = 0,        /* Ucusa Hazir */
    FSM_ASCENT,            /* Yukselme */
    FSM_PASSIVE_DESCENT,   /* Model Uydu Inis - 1000m'ye pasif inis */
    FSM_SEPARATION,        /* Ayrilma */
    FSM_PAYLOAD_DESCENT,   /* Gorev Yuku Inis - SIGMA aktif */
    FSM_RECOVERY           /* Kurtarma */
} FlightState_t;

void          FlightFSM_Init(void);
void          FlightFSM_Update(void);
FlightState_t FlightFSM_GetState(void);

#endif
