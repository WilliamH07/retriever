/* ===========================================================================
 *  sh2_hal_esp32_spi.h — voir sh2_hal_esp32_spi.c
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RT_SH2_HAL_ESP32_SPI_H
#define RT_SH2_HAL_ESP32_SPI_H

#include <stdbool.h>
#include <stdint.h>

#include "sh2_err.h"
#include "sh2_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int sclk;
    int miso;
    int mosi;
    int cs;
    int intn;
    int rstn;
    int ps0;
} rt_sh2_hal_pins_t;

/** Construit le HAL. Retourne NULL si un périphérique refuse de s'initialiser. */
sh2_Hal_t *rt_sh2_hal_init(const rt_sh2_hal_pins_t *pins, int spi_host, int clock_hz);

/** Attend une assertion de H_INTN. Exposé pour la tâche de service. */
bool rt_sh2_hal_wait_intn(uint32_t ms);

/**
 * Compteurs du portage. Existent pour une raison précise : quand le capteur se
 * tait, il faut pouvoir dire OÙ ça s'arrête — la ligne d'interruption, la
 * lecture, l'écriture, ou le réveil — au lieu de le déduire.
 */
typedef struct {
    uint32_t reads;          /**< hal_read() appelé avec H_INTN bas */
    uint32_t packets;        /**< paquets SHTP complets rendus à la pile */
    uint32_t empty_headers;  /**< en-têtes sans corps : normal, mais révélateur en masse */
    uint32_t writes;         /**< écritures abouties */
    uint32_t wake_timeouts;  /**< ⚠️ réveils sans réponse — PS0/WAKE suspect */
} rt_sh2_hal_counters_t;

void rt_sh2_hal_get_counters(rt_sh2_hal_counters_t *out);

#ifdef __cplusplus
}
#endif

#endif /* RT_SH2_HAL_ESP32_SPI_H */
