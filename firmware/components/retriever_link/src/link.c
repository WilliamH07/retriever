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
#define RT_LINK_RX_POLL_MS 2
#define RX_STACK       3072

static rt_link_config_t s_cfg;
static QueueHandle_t s_tx_q;
static QueueHandle_t s_tx_urgent_q;
static QueueHandle_t s_rx_q;
static rt_link_stats_t s_stats;
static rt_link_rx_hook_t s_hook;
static void *s_hook_user;
static bool s_started;

/* ⚠️ `volatile` ne donne PAS l'atomicité : sur un cœur 32 bits, la lecture d'un
 * 64 bits peut être déchirée entre ses deux moitiés. Section critique. */
static int64_t s_time_offset_us;
static bool s_time_offset_valid;

static const rt_link_backend_ops_t *s_backend;
static TaskHandle_t s_tx_task;

/* Les compteurs sont incrémentés par la tâche TX, par la tâche RX et par
 * n'importe quel appelant. Le §G.4 règle 5 en fait une sortie de premier plan :
 * s'ils doivent être opposables, il faut qu'ils soient justes. Une section
 * critique de quelques instructions coûte moins qu'un compteur douteux. */
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

#define STATS_INC(field)                                                       \
    do {                                                                       \
        portENTER_CRITICAL(&s_stats_lock);                                     \
        s_stats.field++;                                                       \
        portEXIT_CRITICAL(&s_stats_lock);                                      \
    } while (0)

uint64_t rt_link_now_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

void rt_link_note_time_sync(uint64_t t_host_us, uint64_t t_local_us)
{
    portENTER_CRITICAL(&s_stats_lock);
    s_time_offset_us = (int64_t)t_host_us - (int64_t)t_local_us;
    s_time_offset_valid = true;
    portEXIT_CRITICAL(&s_stats_lock);
}

int64_t rt_link_time_offset_us(bool *valid)
{
    portENTER_CRITICAL(&s_stats_lock);
    const int64_t offset = s_time_offset_us;
    const bool ok = s_time_offset_valid;
    portEXIT_CRITICAL(&s_stats_lock);
    if (valid) {
        *valid = ok;
    }
    return offset;
}

void rt_link_set_rx_hook(rt_link_rx_hook_t hook, void *user)
{
    s_hook_user = user;
    s_hook = hook;
}

void rt_link_get_stats(rt_link_stats_t *out)
{
    portENTER_CRITICAL(&s_stats_lock);
    *out = s_stats;
    portEXIT_CRITICAL(&s_stats_lock);
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
        STATS_INC(tx_dropped);
        return ESP_ERR_TIMEOUT;
    }
    if (s_tx_task) {
        xTaskNotifyGive(s_tx_task);
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
        STATS_INC(tx_dropped);
        return ESP_ERR_NO_MEM;
    }
    if (s_tx_task) {
        xTaskNotifyGive(s_tx_task);
    }
    return ESP_OK;
}

static void send_one(const rt_frame_t *f)
{
    if (s_backend->send(f) == ESP_OK) {
        STATS_INC(tx_frames);
    } else {
        STATS_INC(tx_dropped);
    }
}

static bool drain_urgent(void)
{
    rt_frame_t f;
    bool any = false;
    while (xQueueReceive(s_tx_urgent_q, &f, 0) == pdTRUE) {
        send_one(&f);
        any = true;
    }
    return any;
}

static void tx_task(void *arg)
{
    (void)arg;
    rt_frame_t f;
    for (;;) {
        /* ⚠️ La tâche est RÉVEILLÉE par l'émetteur, elle ne scrute pas.
         *
         * La version précédente bloquait 20 ms sur la file normale : une trame
         * urgente déposée juste après le début de cette attente y restait
         * jusqu'à son expiration. La file prioritaire ne servait donc à rien,
         * et les 20 ms se retrouvaient dans la mesure d'aller-retour — puis,
         * par `latency_offset_ms`, dans l'horodatage de chaque échantillon. */
        drain_urgent();

        while (xQueueReceive(s_tx_q, &f, 0) == pdTRUE) {
            send_one(&f);
            /* Entre deux trames normales, l'urgente repasse devant. */
            drain_urgent();
        }

        /* Le délai n'est qu'un filet : en régime normal, c'est la notification
         * qui réveille. */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
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
    STATS_INC(rx_frames);

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
        STATS_INC(rx_dropped);
    }
}

static void rx_task(void *arg)
{
    (void)arg;
    for (;;) {
        /* 2 ms et non 20 : `uart_read_bytes` rend la main à l'expiration du
         * délai, donc ce délai EST la latence de réception. À 1 kHz de tick,
         * 500 réveils par seconde sur un ESP32 à 240 MHz ne se mesurent pas. */
        s_backend->poll(pdMS_TO_TICKS(RT_LINK_RX_POLL_MS));
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

    /* ⚠️ L'ordre compte : si l'on posait `s_started` avant de créer les tâches
     * et que la seconde création échouait, la première tournerait déjà et
     * livrerait des trames dans un objet qu'on vient de déclarer mort. */
    TaskHandle_t rx_handle = NULL;
    if (xTaskCreate(rx_task, "link_rx", RX_STACK, NULL, RX_TASK_PRIO, &rx_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(tx_task, "link_tx", TX_STACK, NULL, TX_TASK_PRIO, &s_tx_task) != pdPASS) {
        vTaskDelete(rx_handle);
        s_tx_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_started = true;

    ESP_LOGI(TAG, "%s, noeud %u, protocole %s (0x%08" PRIX32 ")", s_backend->name,
             (unsigned)cfg->node_id, RT_PROTOCOL_VERSION, (uint32_t)RT_PROTOCOL_HASH);
    return ESP_OK;
}
