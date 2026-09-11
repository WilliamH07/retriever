/* ===========================================================================
 *  link_uart.c — transport série, cadrage COBS
 *
 *  Le transport du banc. Un câble USB, une UART, et le cadrage décrit dans
 *  rt_framing.h. Rien de plus : toute l'intelligence de ce fichier tient dans
 *  le fait qu'il n'y en a pas.
 *
 *  ⚠️ Sur une DevKitC, UART0 est câblée sur le pont USB. Si la console ESP-IDF
 *  reste active sur cette UART, ses octets se mélangent aux paquets. Deux
 *  sorties, toutes deux prévues :
 *    - CONFIG_RETRIEVER_LINK_CONSOLE_TUNNEL=y (défaut) : les ESP_LOGx repartent
 *      en trames LOG. Un seul câble. ⚠️ Le bootloader ROM, le gestionnaire de
 *      panique et tout printf() continuent d'écrire en clair : le décodeur les
 *      rejette et se recale, mais la trace de plantage arrive hachée.
 *    - sinon : mettre la liaison sur UART2 et garder la console sur UART0,
 *      au prix d'un second adaptateur USB-série.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "link_backend.h"
#include "rt_framing.h"

static const char *TAG = "link.uart";

#define RX_CHUNK 256
#define UART_RX_BUF 2048
#define UART_TX_BUF 2048

static int s_uart;
static rt_frame_decoder_t s_dec;

static esp_err_t uart_init(const rt_link_config_t *cfg)
{
    s_uart = cfg->uart_num;
    rt_frame_decoder_init(&s_dec);

    const uart_config_t uc = {
        .baud_rate = cfg->uart_baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        /* Pas de contrôle de flux matériel : les cartes de développement ne
         * sortent pas RTS/CTS vers le pont USB. Le budget de débit du §AC.3
         * est calculé pour que la ligne ne sature jamais, ce qui rend le
         * contrôle de flux inutile — à condition de tenir ce budget. */
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(s_uart, &uc);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_pin(s_uart, cfg->uart_tx_gpio, cfg->uart_rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_driver_install(s_uart, UART_RX_BUF, UART_TX_BUF, 0, NULL, 0);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "uart%d a %d bauds", s_uart, cfg->uart_baud);
    return ESP_OK;
}

static esp_err_t uart_send(const rt_frame_t *f)
{
    uint8_t wire[RT_WIRE_MAX];
    const size_t n = rt_frame_encode(f, wire, sizeof(wire));
    if (n == 0u) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Écriture bloquante bornée : au-delà, c'est que le pont USB ne consomme
     * plus, et il vaut mieux perdre la trame que bloquer la tâche d'émission. */
    const int written = uart_write_bytes(s_uart, wire, n);
    return (written == (int)n) ? ESP_OK : ESP_FAIL;
}

static void uart_poll(TickType_t wait)
{
    uint8_t buf[RX_CHUNK];
    const int n = uart_read_bytes(s_uart, buf, sizeof(buf), wait);
    if (n <= 0) {
        return;
    }
    rt_frame_t f;
    for (int i = 0; i < n; ++i) {
        if (rt_frame_decoder_push(&s_dec, buf[i], &f)) {
            rt_link_deliver(&f);
        }
    }
}

static void uart_stats(rt_link_stats_t *out)
{
    out->framing = s_dec.stats;
    out->bus_off = false;
    out->bus_errors = s_dec.stats.crc_errors + s_dec.stats.format_errors +
                      s_dec.stats.overflows;
}

static const rt_link_backend_ops_t s_ops = {
    .name = "serie (COBS)",
    .init = uart_init,
    .send = uart_send,
    .poll = uart_poll,
    .stats = uart_stats,
};

const rt_link_backend_ops_t *rt_link_backend_uart(void) { return &s_ops; }
