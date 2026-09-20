/* ===========================================================================
 *  main.c — nœud SAFETY, étape banc B1 : l'IMU jusqu'à ROS
 *
 *  Ce que ce programme fait aujourd'hui :
 *    - établit l'état sûr AVANT toute autre chose (§G.4 règle 4)
 *    - ouvre la liaison, et y détourne les journaux
 *    - ouvre le BNO085 en SPI
 *    - émet à 100 Hz le triplet quaternion / gyromètre / accéléromètre
 *    - émet à 10 Hz la qualité de l'IMU et un battement de cœur
 *    - répond aux LINK_PING pour que la latence soit mesurable
 *
 *  Ce qu'il ne fait PAS encore, et qui viendra dans cet ordre :
 *    - la machine à états de sécurité (§J.2)
 *    - la précharge et le contacteur
 *    - le budget de courant, l'ACS758, le dialogue BMS
 *  Rien de tout cela ne changera la couche de liaison ni la chaîne IMU : c'est
 *  précisément ce que cette étape sert à vérifier.
 *
 *  ⚠️ Ce nœud, sur le banc, n'a AUCUNE fonction de sécurité. Le mot « safety »
 *  dans son nom décrit sa destination, pas son état actuel.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_config.h"
#include "retriever_imu/imu.h"
#include "retriever_link/link.h"
#include "retriever_link/link_log.h"
#include "retriever_protocol.h"

static const char *TAG = "safety";

#define PUB_TASK_PRIO   17
#define HOUSE_TASK_PRIO  5

static uint32_t s_link_send_failures;

/* --------------------------------------------------------------------------
 *  État sûr
 * ----------------------------------------------------------------------- */

static void hw_init_safe_state(void)
{
    /* Sur le banc, il n'y a ni contacteur, ni ligne /SAFE, ni variateur : il
     * n'y a donc rien à mettre en sécurité. La fonction existe quand même, et
     * elle est appelée en premier, parce que c'est ici que viendront le
     * contacteur ouvert et /SAFE asserté, et que l'ordre des opérations de
     * app_main() ne devra alors plus être rediscuté.
     *
     * ⚠️ Ne rien ajouter AVANT cet appel dans app_main(). */
}

/* --------------------------------------------------------------------------
 *  Réception
 * ----------------------------------------------------------------------- */

static bool on_frame(const rt_frame_t *f, void *user)
{
    (void)user;

    if (f->id == RT_ID_LINK_PING) {
        rt_link_ping_t ping;
        if (!rt_link_ping_unpack(f, &ping)) {
            return true;
        }
        if (ping.target != BOARD_NODE_ID) {
            return true;
        }
        /* Écho immédiat, dans la tâche de réception, par la file prioritaire :
         * la mesure ne doit pas inclure notre propre temps d'ordonnancement,
         * sinon elle mesure le firmware et pas la liaison. */
        rt_link_pong_t pong = {
            .source = BOARD_NODE_ID,
            .seq = ping.seq,
            .t_tx_us = ping.t_tx_us,
        };
        rt_frame_t out;
        rt_link_pong_pack(&pong, &out);
        rt_link_send_urgent(&out);
        return true;
    }

    if (f->id == RT_ID_IMU_CAL_CMD) {
        rt_imu_cal_cmd_t cmd;
        if (!rt_imu_cal_cmd_unpack(f, &cmd)) {
            return true;
        }
        /* ⚠️ Le garde n'est pas décoratif : CLEAR efface un DCD en flash, ce
         * qui coûte dix minutes de manipulations à refaire. Une trame dont le
         * CRC passe mais dont le contenu est faux — un décalage de champ après
         * un changement de protocole non synchronisé, par exemple — ne doit pas
         * pouvoir le déclencher. */
        if (cmd.magic != 0xCAu) {
            ESP_LOGW(TAG, "commande d'etalonnage sans garde, ignoree");
            return true;
        }
        /* On dépose seulement : la pile SH-2 appartient à la tâche de l'IMU. */
        rt_imu_cal_request(cmd.action, cmd.sensors);
        return true;
    }

    return false;   /* tout le reste part en file, pour la tâche de service */
}

/* --------------------------------------------------------------------------
 *  Publication de l'IMU
 * ----------------------------------------------------------------------- */

static void send_or_count(const rt_frame_t *f)
{
    /* Attente bornée à un tick : sur un lien sain la file ne se remplit pas, et
     * si elle se remplit, il vaut mieux perdre un échantillon d'IMU que
     * retarder le suivant. Une IMU en retard est pire qu'une IMU trouée : l'EKF
     * sait traiter un trou, il ne sait pas traiter un horodatage faux. */
    if (rt_link_send(f, 1) != ESP_OK) {
        s_link_send_failures++;
    }
}

static void publish_sample(const rt_imu_sample_t *s)
{
    rt_frame_t f;

    const uint8_t flags = (uint8_t)(((s->valid & RT_IMU_VALID_QUAT) ? 0x01u : 0u) |
                                    ((s->valid & RT_IMU_VALID_GYRO) ? 0x02u : 0u) |
                                    ((s->valid & RT_IMU_VALID_ACCEL) ? 0x04u : 0u));

    /* L'ordre compte : QUAT, GYRO, ACCEL. Le calculateur s'en sert pour
     * regrouper le triplet même si un compteur est perdu. */
    const rt_imu_quat_t q = {
        .w = s->quat[0], .x = s->quat[1], .y = s->quat[2], .z = s->quat[3],
    };
    rt_imu_quat_pack(&q, &f);
    send_or_count(&f);

    const rt_imu_gyro_t g = {
        .gx = s->gyro[0], .gy = s->gyro[1], .gz = s->gyro[2],
        .seq = s->seq, .flags = flags,
    };
    rt_imu_gyro_pack(&g, &f);
    send_or_count(&f);

    const rt_imu_accel_t a = {
        .ax = s->accel[0], .ay = s->accel[1], .az = s->accel[2],
        .seq = s->seq, .flags = flags,
    };
    rt_imu_accel_pack(&a, &f);
    send_or_count(&f);
}

static void publish_mag(const rt_imu_sample_t *last)
{
    if ((last->valid & RT_IMU_VALID_MAG) == 0u) {
        return;
    }
    const rt_imu_mag_t m = {
        .mx = last->mag[0], .my = last->mag[1], .mz = last->mag[2],
        .seq = last->seq, .flags = last->status_mag,
    };
    rt_frame_t f;
    rt_imu_mag_pack(&m, &f);
    send_or_count(&f);
}

static void publish_status(const rt_imu_sample_t *last)
{
    const uint32_t dropped = rt_imu_dropped();
    const rt_imu_status_t st = {
        .quat_accuracy = last->quat_accuracy_rad,
        .status_rot = last->status_rot,
        .status_gyro = last->status_gyro,
        .status_accel = last->status_accel,
        .reset_count = (uint8_t)(rt_imu_reset_count() > 255u ? 255u : rt_imu_reset_count()),
        .dropped = (uint16_t)(dropped > 0xFFFFu ? 0xFFFFu : dropped),
    };
    rt_frame_t f;
    rt_imu_status_pack(&st, &f);
    send_or_count(&f);
}

/* Le pilote d'IMU et le protocole definissent la meme enumeration chacun de
 * leur cote, pour rester independants. C'est ici, le seul endroit qui voit les
 * deux, qu'on verifie qu'ils disent la meme chose. */
_Static_assert((int)RT_IMU_CAL_ACTION_NONE == (int)RT_IMU_CAL_NONE, "imu_cal_action a divergé");
_Static_assert((int)RT_IMU_CAL_ACTION_ENABLE == (int)RT_IMU_CAL_ENABLE, "imu_cal_action a divergé");
_Static_assert((int)RT_IMU_CAL_ACTION_DISABLE == (int)RT_IMU_CAL_DISABLE, "imu_cal_action a divergé");
_Static_assert((int)RT_IMU_CAL_ACTION_SAVE == (int)RT_IMU_CAL_SAVE, "imu_cal_action a divergé");
_Static_assert((int)RT_IMU_CAL_ACTION_CLEAR == (int)RT_IMU_CAL_CLEAR, "imu_cal_action a divergé");

static void publish_cal(void)
{
    rt_imu_cal_state_t c;
    rt_imu_get_cal(&c);
    const rt_imu_cal_t m = {
        .status_mag = c.status_mag,
        .enabled = c.enabled,
        .saves = c.saves,
        .last_action = c.last_action,
        .last_result = c.last_result,
        .flags = (uint8_t)(c.autosave ? 0x01u : 0x00u),
    };
    rt_frame_t f;
    rt_imu_cal_pack(&m, &f);
    send_or_count(&f);
}

static void publish_task(void *arg)
{
    (void)arg;
    rt_imu_sample_t s;
    rt_imu_sample_t last;
    memset(&last, 0, sizeof(last));

    int64_t next_status_us = esp_timer_get_time();
    uint32_t silence_ms = 0;

    for (;;) {
        if (rt_imu_read(&s, pdMS_TO_TICKS(50)) == ESP_OK) {
            publish_sample(&s);
            last = s;
            silence_ms = 0;
        } else {
            silence_ms += 50u;
            if (silence_ms >= 1000u) {
                /* Un capteur qui se tait ne produit aucune erreur : il faut
                 * aller la chercher. C'est le mode de défaillance le plus
                 * fréquent d'une IMU sur SPI.
                 * ⚠️ Une égalité stricte n'alerterait qu'une seule fois : après
                 * dix secondes de silence, plus rien. On répète. */
                silence_ms = 0u;
                ESP_LOGW(TAG, "aucun echantillon depuis 1 s");
            }
        }

        const int64_t now = esp_timer_get_time();
        if (now >= next_status_us) {
            next_status_us = now + 100000;   /* 10 Hz */
            publish_status(&last);
            publish_mag(&last);
            publish_cal();
        }
    }
}

/* --------------------------------------------------------------------------
 *  Entretien
 * ----------------------------------------------------------------------- */

static void housekeeping_task(void *arg)
{
    (void)arg;
    const int64_t t0 = esp_timer_get_time();
    int ticks = 0;

    for (;;) {
        /* On vide la file de réception : les trames non traitées par le crochet
         * atterrissent ici. Aujourd'hui il n'y a que TIME_SYNC, déjà absorbée
         * par la couche de liaison, mais laisser la file se remplir masquerait
         * les compteurs de perte. */
        rt_frame_t f;
        while (rt_link_recv(&f, 0) == ESP_OK) {
            /* Rien à faire de plus pour l'instant. */
        }

        rt_link_stats_t st;
        rt_link_get_stats(&st);

        const rt_heartbeat_safety_t hb = {
            .state = RT_NODE_STATE_READY,
            .uptime_s = (uint16_t)((esp_timer_get_time() - t0) / 1000000),
            .err_count = (uint8_t)((st.framing.crc_errors + st.framing.format_errors +
                                    st.tx_dropped + s_link_send_failures) > 255u
                                       ? 255u
                                       : (st.framing.crc_errors + st.framing.format_errors +
                                          st.tx_dropped + s_link_send_failures)),
            .protocol_hash = RT_PROTOCOL_HASH,
        };
        rt_frame_t hbf;
        rt_heartbeat_safety_pack(&hb, &hbf);
        rt_link_send(&hbf, 0);

        if (++ticks % 100 == 0) {   /* toutes les 10 s */
            bool sync_valid = false;
            const int64_t offset = rt_link_time_offset_us(&sync_valid);
            ESP_LOGI(TAG,
                     "tx=%" PRIu32 " rx=%" PRIu32 " crc=%" PRIu32 " fmt=%" PRIu32
                     " perdues=%" PRIu32 " imu_perdus=%" PRIu32 " resets=%" PRIu32
                     " sync=%s",
                     st.tx_frames, st.rx_frames, st.framing.crc_errors,
                     st.framing.format_errors, st.tx_dropped, rt_imu_dropped(),
                     rt_imu_reset_count(), sync_valid ? "ok" : "absente");
            (void)offset;
        }

        vTaskDelay(pdMS_TO_TICKS(100));   /* 10 Hz */
    }
}

/* --------------------------------------------------------------------------
 *  Démarrage
 * ----------------------------------------------------------------------- */

void app_main(void)
{
    hw_init_safe_state();

    rt_link_config_t link = RT_LINK_CONFIG_BENCH_DEFAULT();
    link.uart_num = BOARD_LINK_UART_NUM;
    link.uart_tx_gpio = BOARD_LINK_UART_TX;
    link.uart_rx_gpio = BOARD_LINK_UART_RX;
    link.uart_baud = BOARD_LINK_BAUD;
    link.twai_tx_gpio = BOARD_CAN_TX;
    link.twai_rx_gpio = BOARD_CAN_RX;
    link.node_id = BOARD_NODE_ID;
#if CONFIG_RETRIEVER_LINK_BACKEND_TWAI
    link.backend = RT_LINK_BACKEND_TWAI;
#else
    link.backend = RT_LINK_BACKEND_UART;
#endif

    ESP_ERROR_CHECK(rt_link_init(&link));
    rt_link_set_rx_hook(on_frame, NULL);

#if CONFIG_RETRIEVER_LINK_CONSOLE_TUNNEL
    /* À partir d'ici, ESP_LOGx part en trames LOG et non plus sur la console.
     * Tout ce qui a été journalisé avant reste sur l'UART, mélangé aux paquets :
     * c'est normal et sans conséquence, le décodeur se recale. */
    rt_link_log_install();
#endif

    ESP_LOGI(TAG, "retriever safety — protocole %s (0x%08" PRIX32 ")", RT_PROTOCOL_VERSION,
             (uint32_t)RT_PROTOCOL_HASH);

    const rt_imu_config_t imu = {
        .spi_host = BOARD_IMU_SPI_HOST,
        .gpio_sclk = BOARD_IMU_SCLK,
        .gpio_miso = BOARD_IMU_MISO,
        .gpio_mosi = BOARD_IMU_MOSI,
        .gpio_cs = BOARD_IMU_CS,
        .gpio_intn = BOARD_IMU_INTN,
        .gpio_rstn = BOARD_IMU_RSTN,
        .gpio_ps0 = BOARD_IMU_PS0,
        .clock_hz = BOARD_IMU_CLOCK_HZ,
#if CONFIG_RETRIEVER_IMU_MODE_GAME
        .mode = RT_IMU_MODE_GAME_ROTATION_VECTOR,
#elif CONFIG_RETRIEVER_IMU_MODE_ARVR
        .mode = RT_IMU_MODE_ARVR_STABILIZED_RV,
#else
        .mode = RT_IMU_MODE_ROTATION_VECTOR,
#endif
        .rate_hz = BOARD_IMU_RATE_HZ,
#if defined(CONFIG_RETRIEVER_IMU_ENABLE_MAG)
        /* Kconfig ne definit le symbole QUE lorsque l'option vaut y : dans une
         * expression C, un symbole absent est une erreur de compilation, pas un
         * zero. C'est la difference avec une directive #if. */
        .enable_mag = true,
#else
        .enable_mag = false,
#endif
        .queue_len = BOARD_IMU_QUEUE_LEN,
    };

    if (rt_imu_init(&imu) != ESP_OK) {
        /* On ne redémarre PAS en boucle : un nœud qui redémarre sans arrêt est
         * indiagnosticable à distance. Le heartbeat continue, il porte l'état,
         * et l'opérateur voit un nœud vivant qui annonce son défaut. */
        ESP_LOGE(TAG, "imu indisponible — le noeud reste en ligne pour rester diagnosticable");
    } else {
        xTaskCreate(publish_task, "imu_pub", 4096, NULL, PUB_TASK_PRIO, NULL);
    }

    xTaskCreate(housekeeping_task, "house", 4096, NULL, HOUSE_TASK_PRIO, NULL);
}
