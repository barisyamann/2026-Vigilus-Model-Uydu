/*
 * sd_spi.c - SD kart SPI modu dusuk seviye surucusu
 *
 * SD kartlar acilista kendi yerel "SD modunda" baslar. SPI moduna gecirmek
 * icin belirli bir el sikisma dizisi gerekir:
 *     CMD0   -> yazilimsal reset, SPI moduna gec
 *     CMD8   -> kart surumu ve calisma gerilimi kontrolu
 *     ACMD41 -> baslatmayi tamamla (kart hazir olana kadar tekrarlanir)
 *     CMD58  -> kapasite tipini ogren (SDSC mi SDHC mi)
 *
 * CS YONETIMI: CS bir islem boyunca kesintisiz LOW kalmalidir. SD_SendCmd
 * CS'e hic dokunmaz; CS'i cagiran fonksiyon yonetir.
 *
 * TRANSFER: Tum alis-veris tek parca (burst) yapiliyor. Komut cercevesi ve
 * yanit penceresi ayni HAL cagrisina giriyor, arada bosluk kalmiyor.
 *
 * HIZ: Standart 400 kHz alti baslatma sart kosuyor, ama kullandigimiz modul
 * o hizda MISO'yu duzgun surmuyor ve R1 baytlari bozuk geliyor. CMD0
 * gectikten hemen sonra 1.3 MHz'e cikiyoruz.
 */

#include "sd_spi.h"
#include "main.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;

#define SD_CS_LOW()   HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)
#define SD_CS_HIGH()  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)

/* --- SD komutlari --- */
#define CMD0    0    // GO_IDLE_STATE
#define CMD8    8    // SEND_IF_COND
#define CMD16  16    // SET_BLOCKLEN
#define CMD17  17    // READ_SINGLE_BLOCK
#define CMD24  24    // WRITE_BLOCK
#define CMD55  55    // APP_CMD (sonraki komut ACMD)
#define CMD41  41    // SD_SEND_OP_COND (ACMD)
#define CMD58  58    // READ_OCR

/* --- Veri belirtecleri (token) --- */
#define TOKEN_START_BLOCK   0xFE
#define DATA_RESP_MASK      0x1F
#define DATA_RESP_ACCEPTED  0x05

#define CMD_FRAME_LEN  16   /* 1 bos + 6 komut + 9 yanit penceresi */

static SD_Type_t card_type = SD_TYPE_UNKNOWN;

/* Komut yanitindan artan baytlar burada bekler (R3/R7 govdesi vs.) */
static uint8_t rx_cache[CMD_FRAME_LEN];
static uint8_t rx_len = 0;
static uint8_t rx_idx = 0;

static uint8_t sd_debug = 0;   /* 1 iken komut cercevesinin tamami dokulur */

/* ------------------------------------------------------------------ */
/* Dusuk seviye SPI                                                    */
/* ------------------------------------------------------------------ */

/* Tek parca cift yonlu transfer. buf hem gonderilir hem uzerine yazilir. */
static void SD_Burst(uint8_t *buf, uint16_t len)
{
    HAL_SPI_TransmitReceive(&hspi1, buf, buf, len, 1000);
}

/* Tek bayt oku: once onbellekten, bitince yeni bir burst cek */
static uint8_t SD_RxByte(void)
{
    if (rx_idx >= rx_len)
    {
        for (uint8_t i = 0; i < CMD_FRAME_LEN; i++) rx_cache[i] = 0xFF;
        SD_Burst(rx_cache, CMD_FRAME_LEN);
        rx_len = CMD_FRAME_LEN;
        rx_idx = 0;
    }
    return rx_cache[rx_idx++];
}

/* n adet bos saat cevrimi (0xFF gonder, gelen atilir) */
static void SD_Clocks(uint16_t n)
{
    uint8_t tmp[CMD_FRAME_LEN];
    while (n)
    {
        uint16_t chunk = (n > CMD_FRAME_LEN) ? CMD_FRAME_LEN : n;
        for (uint16_t i = 0; i < chunk; i++) tmp[i] = 0xFF;
        SD_Burst(tmp, chunk);
        n -= chunk;
    }
    rx_len = 0;
    rx_idx = 0;
}

/*
 * SPI hizini degistirir.
 *
 * DIKKAT: HAL_SPI_Init -> HAL_SPI_MspInit, PA5/6/7'yi GPIO_NOPULL ile
 * yeniden kurar. MISO bosta kalmasin diye pull-up'i her init sonrasi
 * burada geri koyuyoruz.
 */
static void SD_SetSpeed(uint32_t prescaler)
{
    HAL_SPI_DeInit(&hspi1);
    hspi1.Init.BaudRatePrescaler = prescaler;
    HAL_SPI_Init(&hspi1);

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6;            // SPI1_MISO
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    rx_len = 0;
    rx_idx = 0;
}

/* Kart mesgulken MISO'yu DUSUK tutar; serbest kalinca 0xFF okuruz */
static uint8_t SD_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (SD_RxByte() == 0xFF) return 1;
    }
    return 0;
}

/*
 * CRC7 (polinom x^7+x^3+1 = 0x09). SPI modunda CMD0 ve CMD8 disinda CRC
 * kontrol edilmez, ama bu ikisi icin dogru olmak zorunda.
 *
 * SIRALAMA KRITIK: once crc kaydirilir, SONRA karsilastirilir.
 * Dogrulama: CMD0/arg=0 -> 0x95, CMD8/arg=0x1AA -> 0x87
 */
static uint8_t SD_CRC7(uint8_t cmd, uint32_t arg)
{
    uint8_t data[5];
    data[0] = 0x40 | cmd;
    data[1] = (uint8_t)(arg >> 24);
    data[2] = (uint8_t)(arg >> 16);
    data[3] = (uint8_t)(arg >> 8);
    data[4] = (uint8_t)arg;

    uint8_t crc = 0;
    for (uint8_t i = 0; i < 5; i++)
    {
        uint8_t d = data[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            crc = (uint8_t)(crc << 1);
            if ((d & 0x80) ^ (crc & 0x80)) crc ^= 0x09;
            d = (uint8_t)(d << 1);
        }
    }
    return (uint8_t)((crc << 1) | 0x01);   // 7 bit CRC + stop biti
}

/*
 * Komut cercevesi ve yanit penceresi TEK transferde gider:
 *   [0]     bos bayt
 *   [1]     0b01 + komut no
 *   [2..5]  arguman (MSB once)
 *   [6]     CRC7 + stop biti
 *   [7..15] yanit penceresi (0xFF gonderilir)
 * R1, 7..15 arasinda en anlamli biti 0 olan ilk bayttir.
 * Ondan sonraki baytlar onbellege alinir (R3/R7 govdesi orada olabilir).
 *
 * CS'e DOKUNMAZ - cagiran taraf LOW tutuyor olmali.
 */
static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t frame[CMD_FRAME_LEN];

    frame[0] = 0xFF;
    frame[1] = 0x40 | cmd;
    frame[2] = (uint8_t)(arg >> 24);
    frame[3] = (uint8_t)(arg >> 16);
    frame[4] = (uint8_t)(arg >> 8);
    frame[5] = (uint8_t)arg;
    frame[6] = crc;
    for (uint8_t i = 7; i < CMD_FRAME_LEN; i++) frame[i] = 0xFF;

    SD_Burst(frame, CMD_FRAME_LEN);

    if (sd_debug)
    {
        printf("[SD] ham CMD%u: ", cmd);
        for (uint8_t i = 0; i < CMD_FRAME_LEN; i++) printf("%02X ", frame[i]);
        printf("\r\n");
    }

    for (uint8_t i = 7; i < CMD_FRAME_LEN; i++)
    {
        if ((frame[i] & 0x80) == 0)
        {
            rx_len = (uint8_t)(CMD_FRAME_LEN - (i + 1));
            for (uint8_t k = 0; k < rx_len; k++) rx_cache[k] = frame[i + 1 + k];
            rx_idx = 0;
            return frame[i];
        }
    }

    rx_len = 0;
    rx_idx = 0;
    return 0xFF;
}

/* ------------------------------------------------------------------ */
/* Baslatma                                                            */
/* ------------------------------------------------------------------ */

uint8_t SD_Init(void)
{
    card_type = SD_TYPE_UNKNOWN;

    SD_SetSpeed(SPI_BAUDRATEPRESCALER_256);   // 84MHz/256 = 328 kHz

    /* CRC7 fonksiyonunun kendi kendini dogrulamasi */
    printf("[SD] CRC7 kontrol: CMD0=0x%02X (95 olmali), CMD8=0x%02X (87 olmali)\r\n",
           SD_CRC7(CMD0, 0), SD_CRC7(CMD8, 0x000001AA));

    /*
     * 1) Kartin ic guc devresinin oturmasi icin bekle, sonra CS YUKSEK'ken
     *    en az 74 saat cevrimi gonder (standart sart). 20 bayt = 160 cevrim.
     */
    SD_CS_HIGH();
    HAL_Delay(250);
    SD_Clocks(20);

    SD_CS_LOW();   /* buradan itibaren CS surekli LOW */

    /* 2) CMD0: yazilimsal reset + SPI moduna gecis. Beklenen: 0x01 (idle) */
    uint8_t r1 = 0xFF;
    for (uint8_t retry = 0; retry < 100; retry++)
    {
        r1 = SD_SendCmd(CMD0, 0, SD_CRC7(CMD0, 0));
        if (r1 == 0x01) break;
        HAL_Delay(1);
    }
    printf("[SD] CMD0 -> 0x%02X\r\n", r1);

    if (r1 != 0x01)
    {
        SD_CS_HIGH();
        printf("[SD] HATA: kart CMD0'a cevap vermedi\r\n");
        return 0;
    }

    /*
     * Modul 328 kHz'de MISO'yu duzgun surmuyor; R1 baytlari bozuk geliyor.
     * CMD0 gectigine gore kart SPI modunda, hizlanabiliriz.
     */
    SD_SetSpeed(SPI_BAUDRATEPRESCALER_64);   // 84MHz/64 = 1.3 MHz
    printf("[SD] Hiz 1.3 MHz'e yukseltildi\r\n");

    /*
     * 3) CMD8: kart deseni geri yolluyorsa v2'dir. Desenin R7 govdesindeki
     *    yerini sabit varsaymak yerine yanit penceresinde ARIYORUZ.
     */
    uint8_t is_v2 = 0;
    const uint32_t patterns[2] = { 0x000001AA, 0x00000155 };

    for (uint8_t p = 0; p < 2; p++)
    {
        uint8_t want = (uint8_t)(patterns[p] & 0xFF);

        r1 = SD_SendCmd(CMD8, patterns[p], SD_CRC7(CMD8, patterns[p]));

        if (r1 == 0x05)   /* v1 kart: komutu bilmiyor */
        {
            printf("[SD] CMD8 -> 0x05 (SD v1 kart)\r\n");
            break;
        }

        uint8_t r7[4];
        for (uint8_t i = 0; i < 4; i++) r7[i] = SD_RxByte();

        for (uint8_t i = 0; i < 4; i++)
        {
            if (r7[i] == want) { is_v2 = 1; break; }
        }
    }

    if (is_v2)
    {
        printf("[SD] Kart v2 (desen geri geldi)\r\n");
    }
    else
    {
        printf("[SD] UYARI: desen bulunamadi -> v1 varsayiliyor\r\n");
    }

    /*
     * 4) ACMD41: baslatmayi tamamla.
     *    SDHC kartlar HCS (bit30=1) bekler. v2 SDSC kartlar ise 0'a donmeyi bekleyebilir.
     *    500ms icinde 0x00 olmazsa argumani 0'a dusuruyoruz (Fallback).
     */
    uint32_t acmd41_arg = is_v2 ? 0x40000000UL : 0x00000000UL;
    uint32_t start = HAL_GetTick();
    uint16_t iter  = 0;
    uint8_t  r55   = 0xFF;

    do
    {
        SD_CS_LOW();

        r55 = SD_SendCmd(CMD55, 0, SD_CRC7(CMD55, 0));

        if (r55 <= 0x01)
        {
            r1 = SD_SendCmd(CMD41, acmd41_arg, SD_CRC7(CMD41, acmd41_arg));
        }

        /* Komut bitti, kartin dahili islemlerini yurutmesi icin CS HIGH yapilir */
        SD_CS_HIGH();
        SD_Clocks(2);

        iter++;
        if (r1 == 0x00) break; // Standart ve saglikli kartlar burada bitirir.

        /* FALLBACK: 500 ms gecti ve kart hala 0x00 olmadiysa HCS bitini kaldir */
        if ((HAL_GetTick() - start) > 500 && acmd41_arg != 0)
        {
            acmd41_arg = 0x00000000UL;
        }

        HAL_Delay(10);

    } while ((HAL_GetTick() - start) < 2000);

    printf("[SD] ACMD41 -> 0x%02X (%u tur, %lu ms)\r\n",
           r1, iter, (unsigned long)(HAL_GetTick() - start));

    /*
     * 5) QUIRK KONTROLU VE CMD58 (READ_OCR):
     * Klon/Uyumsuz kartlar R1=0x01'de takili kalabilir.
     * Gercek durumu OCR'nin 31. bitinden (Power-Up Status) okuyacagiz.
     */
    SD_CS_LOW();
    uint8_t r_ocr = SD_SendCmd(CMD58, 0, SD_CRC7(CMD58, 0));

    uint8_t ocr[4];
    for (uint8_t i = 0; i < 4; i++) ocr[i] = SD_RxByte();

    printf("[SD] CMD58 -> 0x%02X, OCR = %02X %02X %02X %02X\r\n",
           r_ocr, ocr[0], ocr[1], ocr[2], ocr[3]);

    /* OCR Bit 31 (0x80) '1' ise kart baslatmayi gercekte bitirmis demektir */
    uint8_t is_power_up_finished = (ocr[0] & 0x80) != 0;

    if (!is_power_up_finished && r1 != 0x00)
    {
        SD_CS_HIGH();
        printf("[SD] HATA: Kart baslatilamadi (Gercekten yanit yok)\r\n");
        return 0;
    }

    if (r1 != 0x00 && is_power_up_finished)
    {
        printf("[SD] QUIRK ALGILANDI: Kart R1=0x01 dondu ama OCR 'Hazir' diyor. Devam ediliyor!\r\n");
    }

    /* CCS (Bit 30) kontrolu ile kapasite tipini belirle */
    card_type = (ocr[0] & 0x40) ? SD_TYPE_SDHC : SD_TYPE_SDSC;

    /* SDSC'de blok boyutu 512'ye sabitlenmelidir */
    if (card_type == SD_TYPE_SDSC)
    {
        SD_SendCmd(CMD16, SD_BLOCK_SIZE, SD_CRC7(CMD16, SD_BLOCK_SIZE));
    }

    SD_CS_HIGH();
    SD_Clocks(1);

    /* Islem bitti, veri transfer hizina cikabiliriz. 84MHz/16 = 5.25 MHz */
    SD_SetSpeed(SPI_BAUDRATEPRESCALER_16);

    printf("[SD] Kart tipi: %s, SPI hizi 5.25 MHz'e yukseltildi.\r\n",
           (card_type == SD_TYPE_SDHC) ? "SDHC (blok adresleme)" : "SDSC (bayt adresleme)");

    return 1;
}

SD_Type_t SD_GetType(void)
{
    return card_type;
}

/* ------------------------------------------------------------------ */
/* Blok okuma / yazma                                                  */
/* ------------------------------------------------------------------ */

/*
 * SDHC blok numarasi bekler, SDSC bayt ofseti bekler.
 * Bu farki tek yerde hallediyoruz ki cagiran taraf hep blok numarasi kullansin.
 */
static uint32_t SD_BlockToAddr(uint32_t block_addr)
{
    return (card_type == SD_TYPE_SDHC) ? block_addr
                                       : (block_addr * SD_BLOCK_SIZE);
}

uint8_t SD_ReadBlock(uint32_t block_addr, uint8_t *buf)
{
    if (card_type == SD_TYPE_UNKNOWN) return 0;

    uint32_t addr = SD_BlockToAddr(block_addr);

    SD_CS_LOW();

    uint8_t r1 = SD_SendCmd(CMD17, addr, SD_CRC7(CMD17, addr));
    if (r1 != 0x00)
    {
        SD_CS_HIGH();
        printf("[SD] CMD17 reddedildi, r1=0x%02X\r\n", r1);
        return 0;
    }

    /* Veri basligini bekle (0xFE); kart bu sirada 0xFF gonderir */
    uint8_t token = 0xFF;
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 200)
    {
        token = SD_RxByte();
        if (token != 0xFF) break;
    }

    if (token != TOKEN_START_BLOCK)
    {
        SD_CS_HIGH();
        printf("[SD] Veri basligi gelmedi, token=0x%02X\r\n", token);
        return 0;
    }

    /* Onbellekte kalan baytlar zaten verinin basidir - once onlari al */
    uint16_t n = 0;
    while (rx_idx < rx_len && n < SD_BLOCK_SIZE) buf[n++] = rx_cache[rx_idx++];

    if (n < SD_BLOCK_SIZE)
    {
        for (uint16_t i = n; i < SD_BLOCK_SIZE; i++) buf[i] = 0xFF;
        SD_Burst(&buf[n], (uint16_t)(SD_BLOCK_SIZE - n));
        rx_len = 0;
        rx_idx = 0;
    }

    SD_Clocks(2);   // CRC (2 bayt) - SPI modunda dogrulanmaz ama okunmali

    SD_CS_HIGH();
    SD_Clocks(1);
    return 1;
}

uint8_t SD_WriteBlock(uint32_t block_addr, const uint8_t *buf)
{
    if (card_type == SD_TYPE_UNKNOWN) return 0;

    uint32_t addr = SD_BlockToAddr(block_addr);

    SD_CS_LOW();

    uint8_t r1 = SD_SendCmd(CMD24, addr, SD_CRC7(CMD24, addr));
    if (r1 != 0x00)
    {
        SD_CS_HIGH();
        printf("[SD] CMD24 reddedildi, r1=0x%02X\r\n", r1);
        return 0;
    }

    uint8_t hdr[2] = { 0xFF, TOKEN_START_BLOCK };   // bos bayt + veri basligi
    HAL_SPI_Transmit(&hspi1, hdr, 2, 100);
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, SD_BLOCK_SIZE, 1000);

    uint8_t crc[2] = { 0xFF, 0xFF };                // sahte CRC
    HAL_SPI_Transmit(&hspi1, crc, 2, 100);

    rx_len = 0;
    rx_idx = 0;

    /* Veri yaniti: alt 5 bit 00101 ise kabul edildi */
    uint8_t resp = SD_RxByte();
    if ((resp & DATA_RESP_MASK) != DATA_RESP_ACCEPTED)
    {
        SD_CS_HIGH();
        printf("[SD] Yazma reddedildi, yanit=0x%02X\r\n", resp);
        return 0;
    }

    /* Kart dahili flash programlamayi bitirene kadar bekle */
    if (!SD_WaitReady(500))
    {
        SD_CS_HIGH();
        printf("[SD] Yazma sonrasi zaman asimi\r\n");
        return 0;
    }

    SD_CS_HIGH();
    SD_Clocks(1);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Guvenli dogrulama testi (SALT OKUMA - karta yazmaz)                 */
/* ------------------------------------------------------------------ */

/*
 * Blok 0, kartin MBR (Master Boot Record) bolumudur. Son iki bayti her
 * zaman 0x55 0xAA olmalidir - bolumleme tablosunun imzasidir.
 * Bunu dogru okuyabiliyorsak blok okuma zinciri uctan uca calisiyor demektir.
 */
uint8_t SD_TestReadMBR(void)
{
    uint8_t buf[SD_BLOCK_SIZE];

    if (!SD_ReadBlock(0, buf))
    {
        printf("[SD] TEST: blok 0 okunamadi\r\n");
        return 0;
    }

    printf("[SD] TEST: blok 0 ilk 16 bayt: ");
    for (uint8_t i = 0; i < 16; i++) printf("%02X ", buf[i]);
    printf("\r\n");

    printf("[SD] TEST: imza = %02X %02X (55 AA bekleniyor)\r\n",
           buf[510], buf[511]);

    if (buf[510] == 0x55 && buf[511] == 0xAA)
    {
        printf("[SD] TEST BASARILI - blok okuma calisiyor\r\n");
        return 1;
    }

    printf("[SD] TEST: imza uyusmadi (kart formatsiz olabilir)\r\n");
    return 0;
}
/*
 * Yazma zinciri dogrulamasi: oku -> yaz -> geri oku -> karsilastir -> eski
 * icerigi geri yaz. Hedef blogu bilerek dosya sisteminin veri alaninda
 * secmek gerekir; yine de eski icerik geri yazildigi icin kalici hasar
 * birakmaz (geri yazma da basarisiz olursa o blok bozulur - o yuzden
 * onemli veri iceren kartta CALISTIRMA).
 */
uint8_t SD_TestWriteReadBack(uint32_t block_addr)
{
    static uint8_t orig[SD_BLOCK_SIZE];
    static uint8_t test[SD_BLOCK_SIZE];
    static uint8_t back[SD_BLOCK_SIZE];

    printf("[SD] YAZMA TESTI: blok %lu\r\n", (unsigned long)block_addr);

    if (!SD_ReadBlock(block_addr, orig))
    {
        printf("[SD] YAZMA TESTI: eski icerik okunamadi, iptal\r\n");
        return 0;
    }

    /* Taninabilir bir desen: 0xA5 ile XOR'lanmis indeks */
    for (uint16_t i = 0; i < SD_BLOCK_SIZE; i++)
    {
        test[i] = (uint8_t)(i ^ 0xA5);
    }

    if (!SD_WriteBlock(block_addr, test))
    {
        printf("[SD] YAZMA TESTI: YAZMA BASARISIZ (kart yazilamiyor)\r\n");
        return 0;
    }

    if (!SD_ReadBlock(block_addr, back))
    {
        printf("[SD] YAZMA TESTI: geri okuma basarisiz\r\n");
        return 0;
    }

    uint16_t mismatch = 0;
    uint16_t first_bad = 0xFFFF;
    for (uint16_t i = 0; i < SD_BLOCK_SIZE; i++)
    {
        if (back[i] != test[i])
        {
            if (first_bad == 0xFFFF) first_bad = i;
            mismatch++;
        }
    }

    if (mismatch == 0)
    {
        printf("[SD] YAZMA TESTI: BASARILI - 512/512 bayt dogru\r\n");
    }
    else
    {
        printf("[SD] YAZMA TESTI: %u bayt uyusmadi, ilk hata index %u "
               "(beklenen %02X, gelen %02X)\r\n",
               mismatch, first_bad,
               test[first_bad], back[first_bad]);
    }

    /* Eski icerigi geri yaz */
    if (!SD_WriteBlock(block_addr, orig))
    {
        printf("[SD] YAZMA TESTI: UYARI - eski icerik geri yazilamadi!\r\n");
        return 0;
    }
    printf("[SD] YAZMA TESTI: eski icerik geri yazildi\r\n");

    return (mismatch == 0);
}
