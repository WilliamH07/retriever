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

#ifdef __cplusplus
}
#endif

#endif /* RT_SH2_HAL_ESP32_SPI_H */
