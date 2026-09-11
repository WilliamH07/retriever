/* rt_crc16.c — voir rt_crc16.h
 * Copyright (c) 2026 William Hanczyk — Apache License 2.0 */

#include "rt_crc16.h"

/* Version sans table : 12 octets par paquet à 100 Hz, soit 1200 octets par
 * seconde et environ 10 000 cycles sur un ESP32 à 240 MHz. Une table de 512
 * octets ferait gagner un temps qui n'existe pas et coûterait de la RAM là où
 * elle est comptée. Si le profilage dit un jour le contraire, la table se
 * substitue ici sans toucher à l'interface. */
uint16_t rt_crc16_byte(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)b << 8;
    for (int i = 0; i < 8; ++i) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

uint16_t rt_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = RT_CRC16_INIT;
    for (size_t i = 0; i < len; ++i) {
        crc = rt_crc16_byte(crc, data[i]);
    }
    return crc;
}
