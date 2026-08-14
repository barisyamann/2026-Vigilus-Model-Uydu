#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include "SensorManager.h"

uint8_t Logger_Init(void);                          // 1 = basarili
uint8_t Logger_IsReady(void);                       // dosya acik mi
uint8_t Logger_Write_CSV(const SensorData_t *data); // 1 = satir yazildi
uint8_t Logger_Close(void);                         // kart cikarmadan ONCE
void    Logger_DumpHead(void);                      // dogrulama: dosyanin basini bas

#endif /* LOGGER_H */
