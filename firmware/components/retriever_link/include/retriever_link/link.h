/* ===========================================================================
 *  link.h — couche de liaison Retriever, côté firmware
 *
 *  UNE SEULE IDÉE : le code applicatif n'émet et ne reçoit que des rt_frame_t.
 *  Il ne sait pas, et ne doit jamais savoir, si ces trames passent par un bus
 *  CAN ou par un câble USB. Le jour où la carte CAN existe, on change une
 *  ligne de configuration et un fichier de sdkconfig — pas une ligne de la
 *  tâche IMU, pas une ligne de la machine à états.
 *
 *  C'est la seule raison d'être de ce composant, et c'est ce qui permet de
 *  commencer le logiciel maintenant, alors que le matériel CAN n'est pas
 *  encore conçu.
 *
 *  ⚠️ Ce que le transport série NE fournit PAS, et qu'aucune couche logicielle
 *  ne peut inventer :
 *    - l'arbitrage : deux trames prêtes en même temps sont sérialisées par une
 *      file de priorité logicielle, pas par le bus. C'est une APPROXIMATION.
 *    - l'acquittement et la retransmission automatique.
 *    - le confinement de défaut (error-passive, bus-off).
 *    - le multipoint : un câble par nœud.
 *  → Conséquence de conception, non négociable : AUCUNE fonction de sécurité
 *    n'est validée sur le transport série. Les niveaux N4 et N5 de la chaîne
 *    d'arrêt restent matériels, et le banc série ne sert jamais à prononcer
 *    une recette de sécurité. Voir §AB.5 du dossier.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RETRIEVER_LINK_H
#define RETRIEVER_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "retriever_protocol.h"
#include "rt_framing.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RT_LINK_BACKEND_UART = 0,  /**< cadrage COBS sur UART — banc */
    RT_LINK_BACKEND_TWAI = 1,  /**< CAN 2.0A par le contrôleur TWAI — cible */
} rt_link_backend_t;

typedef struct {
    rt_link_backend_t backend;

    /* UART */
    int uart_num;
    int uart_tx_gpio;
    int uart_rx_gpio;
    int uart_baud;

    /* TWAI */
    int twai_tx_gpio;
    int twai_rx_gpio;
    int twai_bitrate;

    /* Files. Dimensionnées en trames, pas en octets. */
    int tx_queue_len;
    int tx_urgent_queue_len;
    int rx_queue_len;

    /* Identité du nœud, recopiée dans le heartbeat. */
    uint8_t node_id;
} rt_link_config_t;

/** Valeurs par défaut du banc : UART0, 921 600 bauds, un seul câble USB. */
#define RT_LINK_CONFIG_BENCH_DEFAULT()                                         \
    {                                                                          \
        .backend = RT_LINK_BACKEND_UART, .uart_num = 0, .uart_tx_gpio = 1,     \
        .uart_rx_gpio = 3, .uart_baud = 921600, .twai_tx_gpio = 5,             \
        .twai_rx_gpio = 4, .twai_bitrate = 500000, .tx_queue_len = 64,         \
        .tx_urgent_queue_len = 8, .rx_queue_len = 32,                          \
        .node_id = RT_NODE_ID_SAFETY,                                          \
    }

typedef struct {
    uint32_t tx_frames;
    uint32_t tx_dropped;      /**< file pleine : la trame a été perdue, jamais bloquée */
    uint32_t rx_frames;
    uint32_t rx_dropped;      /**< file de réception pleine côté application */
    rt_framing_stats_t framing;
    uint32_t bus_errors;      /**< TWAI : compteur d'erreurs du contrôleur */
    bool     bus_off;         /**< TWAI uniquement ; toujours faux en série */
} rt_link_stats_t;

/**
 * Fonction appelée dans la tâche de réception, pour chaque trame reçue, AVANT
 * la mise en file. Sert aux trames qui doivent être traitées sans délai —
 * l'écho de LINK_PING, une demande d'arrêt.
 *
 * ⚠️ S'exécute dans le contexte de la tâche de réception : pas d'appel long,
 * pas d'allocation, pas de journalisation verbeuse.
 *
 * @return true si la trame est consommée et ne doit PAS être mise en file.
 */
typedef bool (*rt_link_rx_hook_t)(const rt_frame_t *f, void *user);

/**
 * Initialise le transport et démarre les tâches d'émission et de réception.
 * À appeler APRÈS hw_init() : la liaison n'est pas une fonction de sécurité et
 * ne doit pas retarder l'établissement de l'état sûr.
 */
esp_err_t rt_link_init(const rt_link_config_t *cfg);

/** Installe le crochet de réception. NULL pour le retirer. */
void rt_link_set_rx_hook(rt_link_rx_hook_t hook, void *user);

/**
 * Met une trame en file d'émission normale.
 * @param wait  attente maximale si la file est pleine. 0 = ne jamais bloquer.
 * @return ESP_OK, ou ESP_ERR_TIMEOUT si la file est restée pleine (la trame est
 *         perdue et comptée dans tx_dropped — c'est voulu : une boucle de
 *         contrôle ne doit jamais s'arrêter parce qu'une liaison est saturée).
 */
esp_err_t rt_link_send(const rt_frame_t *f, TickType_t wait);

/**
 * Met une trame en file prioritaire. Elle double toutes les trames normales en
 * attente. C'est le substitut logiciel de l'arbitrage CAN — un substitut, pas
 * un équivalent : il n'agit qu'à l'intérieur de ce nœud.
 */
esp_err_t rt_link_send_urgent(const rt_frame_t *f);

/** Retire une trame de la file de réception. */
esp_err_t rt_link_recv(rt_frame_t *f, TickType_t wait);

/** Copie les compteurs. Tout compteur d'erreur est publié (§G.4 règle 5). */
void rt_link_get_stats(rt_link_stats_t *out);

/** Horloge locale monotone, en microsecondes depuis le démarrage. */
uint64_t rt_link_now_us(void);

/**
 * Mémorise une trame TIME_SYNC reçue. La conversion vers le temps ROS se fait
 * côté calculateur : le nœud se contente de garder le dernier couple observé
 * et de le rendre disponible au diagnostic.
 */
void rt_link_note_time_sync(uint64_t t_host_us, uint64_t t_local_us);

/** Dernier décalage observé entre l'horloge hôte et l'horloge locale, en µs. */
int64_t rt_link_time_offset_us(bool *valid);

#ifdef __cplusplus
}
#endif

#endif /* RETRIEVER_LINK_H */
