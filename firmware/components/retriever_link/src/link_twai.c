/* ===========================================================================
 *  link_twai.c — transport CAN 2.0A par le contrôleur TWAI de l'ESP32
 *
 *  ⚠️ ÉTAT : écrit, jamais exécuté. La carte `safety_bus_distribution` n'est
 *  pas fabriquée et aucun transceiver n'est câblé. Ce fichier est là pour une
 *  raison précise : il PROUVE que le passage de la série au CAN ne touche
 *  aucune ligne au-dessus de la couche de liaison. S'il avait fallu plus que
 *  ces quatre-vingts lignes, c'est que l'abstraction aurait été mauvaise.
 *
 *  À faire avant la première mise en service (§AB.6) :
 *    - transceiver SN65HVD230 ou TCAN1042HV, STB tiré à la masse (§C5)
 *    - terminaisons 120 Ω aux DEUX extrémités physiques, nulle part ailleurs
 *    - vérifier que GPIO5 est bien haut au boot (broche de strapping)
 *    - relire §F.2 avant de brancher quoi que ce soit
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include <string.h>

#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "link_backend.h"

static const char *TAG = "link.twai";

static esp_err_t twai_init_(const rt_link_config_t *cfg)
{
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)cfg->twai_tx_gpio, (gpio_num_t)cfg->twai_rx_gpio, TWAI_MODE_NORMAL);
    g.rx_queue_len = 32;
    g.tx_queue_len = 32;
    /* Reprise automatique après bus-off, équivalent du `restart-ms` de
     * SocketCAN côté Linux (§F.5-L1). Sans cela, un nœud parti en bus-off y
     * reste jusqu'au redémarrage. */
    g.alerts_enabled = TWAI_ALERT_BUS_OFF | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR;

    const twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
    const twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g, &t, &f);
    if (err != ESP_OK) {
        return err;
    }
    err = twai_start();
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "can a %d bit/s, tx=%d rx=%d", cfg->twai_bitrate, cfg->twai_tx_gpio,
             cfg->twai_rx_gpio);
    return ESP_OK;
}

static esp_err_t twai_send(const rt_frame_t *f)
{
    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier = f->id;      /* 11 bits */
    msg.data_length_code = f->dlc;
    memcpy(msg.data, f->data, f->dlc);
    return twai_transmit(&msg, pdMS_TO_TICKS(10));
}

static void twai_poll(TickType_t wait)
{
    twai_message_t msg;
    if (twai_receive(&msg, wait) != ESP_OK) {
        return;
    }
    if (msg.extd || msg.rtr) {
        return;   /* le protocole n'utilise que des trames de données 11 bits */
    }
    rt_frame_t f;
    f.id = (uint16_t)msg.identifier;
    f.dlc = msg.data_length_code > RT_MAX_PAYLOAD ? RT_MAX_PAYLOAD : msg.data_length_code;
    memset(f.data, 0, sizeof(f.data));
    memcpy(f.data, msg.data, f.dlc);
    rt_link_deliver(&f);
}

static void twai_stats_(rt_link_stats_t *out)
{
    twai_status_info_t st;
    if (twai_get_status_info(&st) != ESP_OK) {
        return;
    }
    out->bus_off = (st.state == TWAI_STATE_BUS_OFF);
    out->bus_errors = st.bus_error_count;
    if (out->bus_off) {
        /* Redémarrage automatique : l'équivalent embarqué de restart-ms. */
        (void)twai_initiate_recovery();
    }
}

static const rt_link_backend_ops_t s_ops = {
    .name = "can (twai)",
    .init = twai_init_,
    .send = twai_send,
    .poll = twai_poll,
    .stats = twai_stats_,
};

const rt_link_backend_ops_t *rt_link_backend_twai(void) { return &s_ops; }
