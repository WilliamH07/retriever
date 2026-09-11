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

static bool s_bus_off;

static esp_err_t twai_init_(const rt_link_config_t *cfg)
{
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)cfg->twai_tx_gpio, (gpio_num_t)cfg->twai_rx_gpio, TWAI_MODE_NORMAL);
    g.rx_queue_len = 32;
    g.tx_queue_len = 32;
    g.alerts_enabled = TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED |
                       TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR;

    /* Le débit n'est pas libre : le contrôleur TWAI demande une configuration
     * de temporisation figée à la compilation. Annoncer un débit dans les
     * journaux tout en câblant 500 kbit/s serait le genre d'écart qu'on
     * cherche une nuit entière. */
    twai_timing_config_t t;
    switch (cfg->twai_bitrate) {
    case 1000000: t = (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();   break;
    case 800000:  t = (twai_timing_config_t)TWAI_TIMING_CONFIG_800KBITS(); break;
    case 500000:  t = (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS(); break;
    case 250000:  t = (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS(); break;
    case 125000:  t = (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS(); break;
    default:
        ESP_LOGE(TAG, "debit can non gere : %d bit/s", cfg->twai_bitrate);
        return ESP_ERR_INVALID_ARG;
    }

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
    /* Écrêtage symétrique de celui de la réception : `msg.data` fait huit
     * octets, et une asymétrie entre les deux sens est une invitation. */
    msg.data_length_code = f->dlc > RT_MAX_PAYLOAD ? RT_MAX_PAYLOAD : f->dlc;
    memcpy(msg.data, f->data, msg.data_length_code);
    return twai_transmit(&msg, pdMS_TO_TICKS(10));
}

/**
 * Reprise après bus-off.
 *
 * ⚠️ `twai_initiate_recovery()` NE remet PAS le contrôleur en marche : après
 * 128 occurrences du signal bus-libre, il passe en état ARRÊTÉ, et il faut
 * rappeler `twai_start()`. L'oublier donne un nœud qui se tait définitivement
 * après le premier bus-off — et dont le diagnostic annonce que tout va bien,
 * puisque l'état n'est plus « bus-off ».
 *
 * C'est ici, dans la boucle de réception, et pas dans la lecture des
 * compteurs : une fonction de statistiques ne doit pas avoir d'effet de bord.
 */
static void twai_recover(void)
{
    twai_status_info_t st;
    if (twai_get_status_info(&st) != ESP_OK) {
        return;
    }
    if (st.state == TWAI_STATE_BUS_OFF) {
        if (!s_bus_off) {
            s_bus_off = true;
            ESP_LOGE(TAG, "bus-off : tentative de reprise");
        }
        (void)twai_initiate_recovery();
    } else if (st.state == TWAI_STATE_STOPPED && s_bus_off) {
        if (twai_start() == ESP_OK) {
            s_bus_off = false;
            ESP_LOGW(TAG, "bus repris");
        }
    }
}

static void twai_poll(TickType_t wait)
{
    twai_message_t msg;
    if (twai_receive(&msg, wait) != ESP_OK) {
        twai_recover();
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
    /* Lecture pure, aucun effet de bord : la reprise est dans twai_recover().
     * `bus_off` reste vrai pendant toute la reprise, pour que le diagnostic ne
     * déclare pas la liaison saine avant qu'elle ne le soit. */
    out->bus_off = s_bus_off || (st.state == TWAI_STATE_BUS_OFF);
    out->bus_errors = st.bus_error_count;
}

static const rt_link_backend_ops_t s_ops = {
    .name = "can (twai)",
    .init = twai_init_,
    .send = twai_send,
    .poll = twai_poll,
    .stats = twai_stats_,
};

const rt_link_backend_ops_t *rt_link_backend_twai(void) { return &s_ops; }
