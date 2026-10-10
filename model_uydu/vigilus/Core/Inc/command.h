/*
 * command.h
 *
 *  Created on: 8 Ağu 2026
 *      Author: Barış
 */
/*
 * command.h - yer istasyonundan XBee uzerinden gelen metin komutlarini
 * ayristirir (MANUEL_AYRIL, MANUEL_APAM, RHRHRH sifresi vb.)
 */

#ifndef COMMAND_H
#define COMMAND_H

#include <stdint.h>

void Command_Parse(const uint8_t *payload, uint16_t len);

#endif
