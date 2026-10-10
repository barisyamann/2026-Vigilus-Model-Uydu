#ifndef GPS_H
#define GPS_H

#include <stdint.h>

typedef struct
{
    uint8_t fix_valid;
    double  latitude;           // ondalik derece (+ Kuzey, - Guney)
    double  longitude;          // ondalik derece (+ Dogu,  - Bati)
    float   altitude_m;
    uint8_t satellites;         // fix icin KULLANILAN uydu sayisi (GGA)
    uint8_t satellites_in_view; // GORUNEN uydu sayisi (GSV)
    uint8_t hour, minute, second;
    uint8_t day, month, year;
} GPS_Data_t;

void GPS_Init(void);
void GPS_ProcessByte(uint8_t byte, GPS_Data_t *out);

#endif /* GPS_H */
