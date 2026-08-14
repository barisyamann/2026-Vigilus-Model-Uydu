
#include "ina226.h"
#include "main.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

#define INA226_ADDR             (0x44 << 1)   // A0/A1 pinleri GND ise 0x40

/* Register adresleri */
#define INA226_CONFIG_REG       0x00
#define INA226_SHUNT_V_REG      0x01
#define INA226_BUS_V_REG        0x02
#define INA226_POWER_REG        0x03
#define INA226_CURRENT_REG      0x04
#define INA226_CALIB_REG        0x05
#define INA226_MANUF_ID_REG     0xFE
#define INA226_DIE_ID_REG       0xFF

#define INA226_MANUF_ID_EXPECTED  0x5449   // ASCII "TI" (Texas Instruments)
#define INA226_DIE_ID_EXPECTED    0x2260

/*
 * Konfigurasyon: 0x4527
 *   bit15    = 0    -> reset yok
 *   bit14-12 = 100  -> sabit (datasheet)
 *   bit11-9  = 010  -> AVG = 16 ornek ortalamasi (gurultuyu azaltir)
 *   bit8-6   = 100  -> VBUSCT = 1.1 ms bus gerilim donusum suresi
 *   bit5-3   = 100  -> VSHCT  = 1.1 ms sont gerilim donusum suresi
 *   bit2-0   = 111  -> mod: sont + bus, surekli olcum
 */
#define INA226_CONFIG_VALUE     0x4527

/*
 * Sont direnc ve akim cozunurlugu:
 *
 * Cogu hazir INA226 modulunde 0.1 ohm (uzerinde "R100" yazar) sont vardir.
 * Kendi sont degerini kullaniyorsan asagidaki degeri guncelle.
 *
 * INA226'nin sont gerilim olcum siniri +/-81.92 mV'dur. Yani 0.1 ohm ile
 * olculebilecek MAKSIMUM akim:  81.92 mV / 0.1 ohm = 0.819 A
 *
 * DIKKAT (proje notu): Itki hatti 40 A civari akim cekecek. 0.1 ohm sont
 * hem bu akimi olcemez hem de 0.1*40^2 = 160 W isi uretip yanar. Ana batarya
 * hattini olcmek icin cok daha kucuk bir sont (ornegin 0.002 ohm, 4 W'lik)
 * gerekir. Mevcut modul, sadece aviyonik hattini (mA seviyesi) olcmek icin
 * uygundur.
 */
#define INA226_SHUNT_OHM        0.1f

/*
 * Current_LSB: akim register'inin her biriminin kac Amper ettigi.
 * Sectigimiz deger 0.1 mA -> hem cozunurluk yeterli hem kalibrasyon tam sayi.
 *
 * Kalibrasyon formulu (datasheet):
 *   CAL = 0.00512 / (Current_LSB * R_shunt)
 *   CAL = 0.00512 / (0.0001 * 0.1) = 512
 */
#define INA226_CURRENT_LSB_A    0.0001f
#define INA226_CALIB_VALUE      512

/* Sabit olcek katsayilari (datasheet) */
#define INA226_BUS_V_LSB        0.00125f   // 1.25 mV
#define INA226_SHUNT_V_LSB_MV   0.0025f    // 2.5 uV = 0.0025 mV

static uint8_t INA226_ReadReg16(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, INA226_ADDR, reg,
                                                  I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
    if (status != HAL_OK)
    {
        return 0;
    }
    *value = (uint16_t)(buf[0] << 8 | buf[1]);  // MSB once
    return 1;
}

static uint8_t INA226_WriteReg16(uint8_t reg, uint16_t value)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);    // MSB once
    buf[1] = (uint8_t)(value & 0xFF);

    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c1, INA226_ADDR, reg,
                                                   I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
    return (status == HAL_OK) ? 1 : 0;
}

uint8_t INA226_Init(void)
{
    uint16_t manuf_id = 0, die_id = 0;

    uint8_t ok1 = INA226_ReadReg16(INA226_MANUF_ID_REG, &manuf_id);
    uint8_t ok2 = INA226_ReadReg16(INA226_DIE_ID_REG, &die_id);

    printf("[INA226] read_ok=%u/%u manuf_id=0x%04X die_id=0x%04X\r\n",
           ok1, ok2, manuf_id, die_id);

    if (!ok1 || !ok2 || manuf_id != INA226_MANUF_ID_EXPECTED)
    {
        return 0;
    }

    if (!INA226_WriteReg16(INA226_CONFIG_REG, INA226_CONFIG_VALUE)) return 0;
    if (!INA226_WriteReg16(INA226_CALIB_REG, INA226_CALIB_VALUE))   return 0;

    /* Yazilan degerleri geri okuyup dogrula - yazma sessizce basarisiz olabilir */
    uint16_t cfg_check = 0, cal_check = 0;
    INA226_ReadReg16(INA226_CONFIG_REG, &cfg_check);
    INA226_ReadReg16(INA226_CALIB_REG, &cal_check);
    printf("[INA226] config=0x%04X (beklenen 0x%04X) calib=%u (beklenen %u)\r\n",
           cfg_check, INA226_CONFIG_VALUE, cal_check, INA226_CALIB_VALUE);

    if (cfg_check != INA226_CONFIG_VALUE || cal_check != INA226_CALIB_VALUE)
    {
        return 0;
    }

    HAL_Delay(20);  // ilk olcumun tamamlanmasi icin bekle
    return 1;
}

uint8_t INA226_ReadData(INA226_Data_t *out)
{
    uint16_t raw_bus = 0, raw_shunt = 0, raw_current = 0;

    if (!INA226_ReadReg16(INA226_BUS_V_REG, &raw_bus))         return 0;
    if (!INA226_ReadReg16(INA226_SHUNT_V_REG, &raw_shunt))     return 0;
    if (!INA226_ReadReg16(INA226_CURRENT_REG, &raw_current))   return 0;

    /* Bus gerilimi: isaretsiz, LSB = 1.25 mV */
    out->bus_voltage_v = (float)raw_bus * INA226_BUS_V_LSB;

    /* Sont gerilimi ve akim: ISARETLI (negatif akim = ters yon) */
    out->shunt_voltage_mv = (float)((int16_t)raw_shunt) * INA226_SHUNT_V_LSB_MV;
    out->current_a        = (float)((int16_t)raw_current) * INA226_CURRENT_LSB_A;

    out->power_w = out->bus_voltage_v * out->current_a;

    return 1;
}
