/* ===========================================================================
 *  link.c — couche de liaison, partie commune
 *
 *  Deux tâches, deux files, un transport enfichable.
 *
 *      application ──► file urgente ──┐
 *                  ──► file normale ──┴──► tâche TX ──► backend ──► fil
 *      application ◄── file RX ◄── crochet ◄── tâche RX ◄── backend ◄── fil
 *
 *  Les deux files sont dimensionnées en trames et allouées une fois pour
 *  toutes : aucune allocation après l'initialisation (§G.4 règle 1).
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include "retriever_link/link.h"

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "link_backend.h"

static const char *TAG = "link";

#define TX_TASK_PRIO   20
#define RX_TASK_PRIO   23
#define TX_STACK       3072
#define RX_STACK       3072

static rt_link_config_t s_cfg;
static QueueHandle_t s_tx_q;
static QueueHandle_t s_tx_urgent_q;
static QueueHandle_t s_rx_q;
static rt_link_stats_t s_stats;
static rt_link_rx_hook_t s_hook;
static void *s_hook_user;
static bool s_started;

static volatile int64_t s_time_offset_us;
static volatile bool s_time_offset_valid;

static const rt_link_backend_ops_t *s_backend;

uint64_t rt_link_now_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

void rt_link_note_time_sync(uint64_t t_host_us, uint64_t t_local_us)
{
    s_time_offset_us = (int64_t)t_host_us - (int64_t)t_local_us;
    s_time_offset_valid = true;
}

int64_t rt_link_time_offset_us(bool *valid)
{
    if (valid) {
        *valid = s_time_offset_valid;
    }
    return s_time_offset_us;
}

void rt_link_set_rx_hook(rt_link_rx_hook_t hook, void *user)
{
    s_hook_user = user;
    s_hook = hook;
}

void rt_link_get_stats(rt_link_stats_t *out)
{
    *out = s_stats;
    if (s_backend && s_backend->stats) {
        s_backend->stats(out);
    }
}

/* --------------------------------------------------------------------------
 *  Émission
 * ----------------------------------------------------------------------- */

esp_err_t rt_link_send(const rt_frame_t *f, TickType_t wait)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(s_tx_q, f, wait) != pdTRUE) {
        s_stats.tx_dropped++;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t rt_link_send_urgent(const rt_frame_t *f)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Jamais d'attente sur la file urgente : si elle est pleine, c'est que le
     * lien est mort, et attendre ne ferait que propager le blocage à l'appelant
     * — qui est, par construction, la partie du code qu'il ne faut pas bloquer. */
    if (xQueueSend(s_tx_urgent_q, f, 0) != pdTRUE) {
        s_stats.tx_dropped++;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void tx_task(void *arg)
{
    (void)arg;
    rt_frame_t f;
    for (;;) {
        /* La file urgente passe toujours devant. C'est ce qui remplace, à
         * l'intérieur d'un nœud, l'arbitrage que le bus CAN fera plus tard
         * entre les nœuds. */
        if (xQueueReceive(s_tx_urgent_q, &f, 0) == pdTRUE) {
            if (s_backend->send(&f) == ESP_OK) {
                s_stats.tx_frames++;
            } else {
                s_stats.tx_dropped++;
            }
            continue;
        }
        if (xQueueReceive(s_tx_q, &f, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (s_backend->send(&f) == ESP_OK) {
                s_stats.tx_frames++;
            } else {
                s_stats.tx_dropped++;
            }
        }
    }
}

/* --------------------------------------------------------------------------
 *  Réception
 * ----------------------------------------------------------------------- */

esp_err_t rt_link_recv(rt_frame_t *f, TickType_t wait)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    return xQueueReceive(s_rx_q, f, wait) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

void rt_link_deliver(const rt_frame_t *f)
{
    s_stats.rx_frames++;

    /* TIME_SYNC est traité ici, au plus près de la réception : c'est le seul
     * endroit où l'horodatage local est encore celui de l'arrivée. */
    if (f->id == RT_ID_TIME_SYNC) {
        rt_time_sync_t ts;
        if (rt_time_sync_unpack(f, &ts)) {
            rt_link_note_time_sync(ts.t_host_us, rt_link_now_us());
        }
    }

    if (s_hook && s_hook(f, s_hook_user)) {
        return;
    }
    if (xQueueSend(s_rx_q, f, 0) != pdTRUE) {
        s_stats.rx_dropped++;
    }
}

static void rx_task(void *arg)
{
    (void)arg;
    for (;;) {
        s_backend->poll(pdMS_TO_TICKS(20));
    }
}

/* --------------------------------------------------------------------------
 *  Initialisation
 * ----------------------------------------------------------------------- */

esp_err_t rt_link_init(const rt_link_config_t *cfg)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    s_cfg = *cfg;
    memset(&s_stats, 0, sizeof(s_stats));

    s_tx_q = xQueueCreate((UBaseType_t)cfg->tx_queue_len, sizeof(rt_frame_t));
    s_tx_urgent_q = xQueueCreate((UBaseType_t)cfg->tx_urgent_queue_len, sizeof(rt_frame_t));
    s_rx_q = xQueueCreate((UBaseType_t)cfg->rx_queue_len, sizeof(rt_frame_t));
    if (!s_tx_q || !s_tx_urgent_q || !s_rx_q) {
        ESP_LOGE(TAG, "files non allouees");
        return ESP_ERR_NO_MEM;
    }

    switch (cfg->backend) {
    case RT_LINK_BACKEND_UART:
        s_backend = rt_link_backend_uart();
        break;
    case RT_LINK_BACKEND_TWAI:
        s_backend = rt_link_backend_twai();
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = s_backend->init(&s_cfg);
    if (err != ESP_OK) {
        return err;
    }

    s_started = true;

    if (xTaskCreate(rx_task, "link_rx", RX_STACK, NULL, RX_TASK_PRIO, NULL) != pdPASS ||
        xTaskCreate(tx_task, "link_tx", TX_STACK, NULL, TX_TASK_PRIO, NULL) != pdPASS) {
        s_started = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "%s, noeud %u, protocole %s (0x%08" PRIX32 ")", s_backend->name,
             (unsigned)cfg->node_id, RT_PROTOCOL_VERSION, (uint32_t)RT_PROTOCOL_HASH);
    return ESP_OK;
}
