


#ifndef INA226_H
#define INA226_H

#include <stdint.h>

typedef struct
{
    float bus_voltage_v;      // yuk uzerindeki gerilim (V)
    float shunt_voltage_mv;   // sont direnc uzerindeki gerilim (mV)
    float current_a;          // hattan gecen akim (A)
    float power_w;            // hesaplanan guc (W)
} INA226_Data_t;

uint8_t INA226_Init(void);                    // 1 = basarili, 0 = cihaz bulunamadi
uint8_t INA226_ReadData(INA226_Data_t *out);  // 1 = okuma basarili, 0 = I2C hatasi

#endif
