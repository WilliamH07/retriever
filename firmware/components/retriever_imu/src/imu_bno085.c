/* ===========================================================================
 *  imu_bno085.c — BNO085 derrière l'interface rt_imu_*
 *
 *  Une tâche, qui ne fait qu'une chose : attendre H_INTN, appeler sh2_service(),
 *  laisser les rappels remplir l'échantillon courant, et publier le triplet
 *  quand il est complet.
 *
 *  Le regroupement en triplet mérite une explication. Le BNO085 envoie le
 *  quaternion, le gyromètre et l'accéléromètre dans des rapports SÉPARÉS, à la
 *  même cadence mais pas dans la même trame. Les publier séparément obligerait
 *  le calculateur à les réassocier lui-même, ce qui est exactement le genre de
 *  travail qu'on ne veut pas faire deux fois. On les rassemble donc ici, où
 *  l'information d'appartenance existe encore, et on les émet avec un même
 *  compteur `seq`.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include "retriever_imu/imu.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "sh2.h"
#include "sh2_SensorValue.h"
#include "sh2_err.h"
#include "sh2_hal_esp32_spi.h"

static const char *TAG = "imu";

#define IMU_TASK_PRIO  18
#define IMU_TASK_STACK 4096

static QueueHandle_t s_queue;
static rt_imu_config_t s_cfg;
static uint32_t s_resets;
static uint32_t s_dropped;
static uint8_t s_seq;

/* Échantillon en cours d'assemblage. Écrit uniquement par la tâche IMU. */
static rt_imu_sample_t s_building;

static sh2_SensorId_t orientation_sensor(rt_imu_mode_t mode)
{
    switch (mode) {
    case RT_IMU_MODE_GAME_ROTATION_VECTOR:
        return SH2_GAME_ROTATION_VECTOR;
    case RT_IMU_MODE_ARVR_STABILIZED_RV:
        return SH2_ARVR_STABILIZED_RV;
    case RT_IMU_MODE_ROTATION_VECTOR:
    default:
        return SH2_ROTATION_VECTOR;
    }
}

static const char *mode_name(rt_imu_mode_t mode)
{
    switch (mode) {
    case RT_IMU_MODE_GAME_ROTATION_VECTOR: return "game rotation vector (6 axes)";
    case RT_IMU_MODE_ARVR_STABILIZED_RV:   return "arvr stabilized rv (9 axes lisses)";
    default:                               return "rotation vector (9 axes)";
    }
}

/* --------------------------------------------------------------------------
 *  Rappels SH-2
 * ----------------------------------------------------------------------- */

static void on_event(void *cookie, sh2_AsyncEvent_t *event)
{
    (void)cookie;
    if (event->eventId == SH2_RESET) {
        /* ⚠️ Un reset spontané du capteur perd toute la configuration des
         * rapports. Le détecter et le RECOMPTER est ce qui distingue une IMU
         * qui s'est tue d'une IMU qu'on croit vivante. */
        s_resets++;
        ESP_LOGW(TAG, "reset du capteur (%u depuis le demarrage)", (unsigned)s_resets);
    }
}

static uint64_t rt_imu_now_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

/* Le magnétomètre arrive à 10 Hz, le triplet à 100 Hz : son drapeau de validité
 * est conservé d'un échantillon à l'autre. Au-delà de ce délai, il ne l'est
 * plus — un magnétomètre arrêté ne doit pas continuer à publier sa dernière
 * valeur avec un compteur frais. */
#define MAG_STALE_US 300000ull

static uint64_t s_last_mag_us;

/* Un événement décodé par grandeur. Quand le triplet ne se complète jamais,
 * c'est ce tableau qui dit lequel des trois manque — et s'il en manque un ou
 * les trois, le diagnostic n'est pas le même. */
static uint32_t s_ev_total;
static uint32_t s_ev_quat;
static uint32_t s_ev_gyro;
static uint32_t s_ev_accel;
static uint32_t s_ev_decode_fail;

static void publish_if_complete(void)
{
    const uint8_t needed = RT_IMU_VALID_QUAT | RT_IMU_VALID_GYRO | RT_IMU_VALID_ACCEL;
    if ((s_building.valid & needed) != needed) {
        return;
    }
    s_building.seq = s_seq++;
    if (xQueueSend(s_queue, &s_building, 0) != pdTRUE) {
        s_dropped++;
    }
    /* On garde le magnétomètre d'un échantillon sur l'autre — il arrive moins
     * souvent que le triplet — mais pas indéfiniment. */
    if (rt_imu_now_us() - s_last_mag_us > MAG_STALE_US) {
        s_building.valid = 0u;
    } else {
        s_building.valid &= (uint8_t)RT_IMU_VALID_MAG;
    }
}

static void on_sensor(void *cookie, sh2_SensorEvent_t *event)
{
    (void)cookie;
    s_ev_total++;
    sh2_SensorValue_t v;
    if (sh2_decodeSensorEvent(&v, event) != SH2_OK) {
        s_ev_decode_fail++;
        return;
    }

    /* On ne recopie PAS v.timestamp : le protocole ne transporte aucun
     * horodatage par échantillon (les huit octets sont pleins), et l'horloge
     * que le HAL rend à SH-2 est un uint32_t qui reboucle toutes les 71 minutes.
     * Un champ mort vaut mieux qu'un champ faux ; l'horodatage se fait côté
     * calculateur, voir §AF.4. */
    s_building.t_us = rt_imu_now_us();

    switch (v.sensorId) {
    case SH2_ROTATION_VECTOR:
    case SH2_ARVR_STABILIZED_RV:
        s_building.quat[0] = v.un.rotationVector.real;
        s_building.quat[1] = v.un.rotationVector.i;
        s_building.quat[2] = v.un.rotationVector.j;
        s_building.quat[3] = v.un.rotationVector.k;
        s_building.quat_accuracy_rad = v.un.rotationVector.accuracy;
        s_building.status_rot = (uint8_t)(v.status & 0x03u);
        s_building.valid |= RT_IMU_VALID_QUAT;
        s_ev_quat++;
        break;

    case SH2_GAME_ROTATION_VECTOR:
        s_building.quat[0] = v.un.gameRotationVector.real;
        s_building.quat[1] = v.un.gameRotationVector.i;
        s_building.quat[2] = v.un.gameRotationVector.j;
        s_building.quat[3] = v.un.gameRotationVector.k;
        /* ⚠️ Le game rotation vector ne fournit AUCUNE estimation d'erreur : le
         * champ n'existe tout simplement pas dans la structure (c'est un
         * sh2_RotationVector_t, sans accuracy, là où le rotation vector est un
         * sh2_RotationVectorWAcc_t). Publier 0 ferait croire à une orientation
         * parfaite et ferait diverger l'EKF.
         * On publie donc la BORNE HAUTE EXACTE de l'encodage — u16 d'échelle
         * 1e-4, soit 6,5535 rad — que le calculateur reconnaît sans ambiguïté
         * comme « non renseignée ». */
        s_building.quat_accuracy_rad = RT_IMU_ACCURACY_UNREPORTED;
        s_building.status_rot = (uint8_t)(v.status & 0x03u);
        s_building.valid |= RT_IMU_VALID_QUAT;
        break;

    case SH2_GYROSCOPE_CALIBRATED:
        s_building.gyro[0] = v.un.gyroscope.x;
        s_building.gyro[1] = v.un.gyroscope.y;
        s_building.gyro[2] = v.un.gyroscope.z;
        s_building.status_gyro = (uint8_t)(v.status & 0x03u);
        s_building.valid |= RT_IMU_VALID_GYRO;
        s_ev_gyro++;
        break;

    case SH2_ACCELEROMETER:
        /* SH2_ACCELEROMETER inclut la gravité — c'est bien ce que demande
         * REP-145 (+g au repos). SH2_LINEAR_ACCELERATION, qui la retire, serait
         * un contresens sur ce topic. */
        s_building.accel[0] = v.un.accelerometer.x;
        s_building.accel[1] = v.un.accelerometer.y;
        s_building.accel[2] = v.un.accelerometer.z;
        s_building.status_accel = (uint8_t)(v.status & 0x03u);
        s_building.valid |= RT_IMU_VALID_ACCEL;
        s_ev_accel++;
        break;

    case SH2_MAGNETIC_FIELD_CALIBRATED:
        s_building.mag[0] = v.un.magneticField.x;
        s_building.mag[1] = v.un.magneticField.y;
        s_building.mag[2] = v.un.magneticField.z;
        s_building.status_mag = (uint8_t)(v.status & 0x03u);
        s_building.valid |= RT_IMU_VALID_MAG;
        s_last_mag_us = rt_imu_now_us();
        break;

    default:
        return;
    }

    publish_if_complete();
}

/* --------------------------------------------------------------------------
 *  Configuration des rapports
 * ----------------------------------------------------------------------- */

static int enable_report(sh2_SensorId_t id, uint32_t interval_us)
{
    sh2_SensorConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.reportInterval_us = interval_us;
    /* Pas de batching : on veut la fraîcheur, pas le débit. Un lot de dix
     * échantillons arrivés ensemble détruit l'horodatage qu'on est allé
     * chercher si loin. */
    cfg.batchInterval_us = 0;
    return sh2_setSensorConfig(id, &cfg);
}

static esp_err_t configure_reports(void)
{
    const uint32_t interval = (uint32_t)(1000000 / (s_cfg.rate_hz > 0 ? s_cfg.rate_hz : 100));

    struct {
        sh2_SensorId_t id;
        const char *name;
        bool required;
    } reports[] = {
        {orientation_sensor(s_cfg.mode), "orientation", true},
        {SH2_GYROSCOPE_CALIBRATED, "gyro", true},
        {SH2_ACCELEROMETER, "accel", true},
        {SH2_MAGNETIC_FIELD_CALIBRATED, "mag", false},
    };

    for (size_t i = 0; i < sizeof(reports) / sizeof(reports[0]); ++i) {
        if (!reports[i].required && !s_cfg.enable_mag) {
            continue;
        }
        /* Le magnétomètre n'a aucun intérêt à 100 Hz : il est lent et bruité.
         * 10 Hz suffit largement pour juger de la qualité magnétique du site. */
        const uint32_t period = reports[i].required ? interval : 100000u;
        const int rc = enable_report(reports[i].id, period);
        if (rc != SH2_OK) {
            if (reports[i].required) {
                ESP_LOGE(TAG, "rapport %s refuse (%d)", reports[i].name, rc);
                return ESP_FAIL;
            }
            ESP_LOGW(TAG, "rapport %s refuse (%d), on continue", reports[i].name, rc);
        }
    }
    return ESP_OK;
}

/* --------------------------------------------------------------------------
 *  Tâche de service
 * ----------------------------------------------------------------------- */

static void report_counters(void)
{
    rt_sh2_hal_counters_t c;
    rt_sh2_hal_get_counters(&c);
    ESP_LOGI(TAG,
             "spi lectures=%u paquets=%u vides=%u ecritures=%u reveils_manques=%u",
             (unsigned)c.reads, (unsigned)c.packets, (unsigned)c.empty_headers,
             (unsigned)c.writes, (unsigned)c.wake_timeouts);
    ESP_LOGI(TAG, "evenements total=%u quat=%u gyro=%u accel=%u indecodables=%u",
             (unsigned)s_ev_total, (unsigned)s_ev_quat, (unsigned)s_ev_gyro,
             (unsigned)s_ev_accel, (unsigned)s_ev_decode_fail);
}

static void imu_task(void *arg)
{
    (void)arg;
    uint32_t last_resets = s_resets;
    int64_t next_report_us = esp_timer_get_time() + 5000000;

    for (;;) {
        /* H_INTN est le signal : le composant prévient quand il a quelque
         * chose. Le délai d'attente n'est là que pour pouvoir se réveiller et
         * constater qu'il ne dit plus rien. */
        rt_sh2_hal_wait_intn(50);
        sh2_service();

        const int64_t now_us = esp_timer_get_time();
        if (now_us >= next_report_us) {
            next_report_us = now_us + 5000000;   /* toutes les 5 s */
            report_counters();
        }

        if (s_resets != last_resets) {
            last_resets = s_resets;
            /* Après un reset, les rapports sont perdus : il faut les redemander,
             * sinon l'IMU se tait définitivement sans que rien n'échoue. */
            if (configure_reports() != ESP_OK) {
                ESP_LOGE(TAG, "reconfiguration impossible apres reset");
            } else {
                ESP_LOGW(TAG, "rapports reconfigures apres reset");
            }
        }
    }
}

/* --------------------------------------------------------------------------
 *  Interface publique
 * ----------------------------------------------------------------------- */

esp_err_t rt_imu_init(const rt_imu_config_t *cfg)
{
    s_cfg = *cfg;
    memset(&s_building, 0, sizeof(s_building));

    s_queue = xQueueCreate((UBaseType_t)(cfg->queue_len > 0 ? cfg->queue_len : 8),
                           sizeof(rt_imu_sample_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const rt_sh2_hal_pins_t pins = {
        .sclk = cfg->gpio_sclk,
        .miso = cfg->gpio_miso,
        .mosi = cfg->gpio_mosi,
        .cs = cfg->gpio_cs,
        .intn = cfg->gpio_intn,
        .rstn = cfg->gpio_rstn,
        .ps0 = cfg->gpio_ps0,
    };
    sh2_Hal_t *hal = rt_sh2_hal_init(&pins, cfg->spi_host, cfg->clock_hz);
    if (hal == NULL) {
        ESP_LOGE(TAG, "hal spi indisponible");
        return ESP_FAIL;
    }

    const int rc = sh2_open(hal, on_event, NULL);
    if (rc != SH2_OK) {
        ESP_LOGE(TAG, "sh2_open a echoue (%d)", rc);
        return ESP_FAIL;
    }
    sh2_setSensorCallback(on_sensor, NULL);

    /* Premier contact réel : si l'identification du produit répond, le bus, le
     * mode SPI, la séquence PS0/PS1 et le reset sont tous corrects d'un coup.
     * C'est la vérification n°1 de la recette du banc. */
    sh2_ProductIds_t ids;
    memset(&ids, 0, sizeof(ids));
    if (sh2_getProdIds(&ids) == SH2_OK && ids.numEntries > 0) {
        ESP_LOGI(TAG, "bno08x present : firmware %u.%u.%u",
                 ids.entry[0].swVersionMajor, ids.entry[0].swVersionMinor,
                 (unsigned)ids.entry[0].swVersionPatch);
    } else {
        ESP_LOGE(TAG, "le capteur ne repond pas a l'identification produit");
        return ESP_FAIL;
    }

    if (configure_reports() != ESP_OK) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "%s a %d Hz%s", mode_name(s_cfg.mode), s_cfg.rate_hz,
             s_cfg.enable_mag ? ", magnetometre a 10 Hz" : "");

    if (xTaskCreate(imu_task, "imu", IMU_TASK_STACK, NULL, IMU_TASK_PRIO, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t rt_imu_read(rt_imu_sample_t *out, TickType_t wait)
{
    if (s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return xQueueReceive(s_queue, out, wait) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

uint32_t rt_imu_reset_count(void) { return s_resets; }
uint32_t rt_imu_dropped(void) { return s_dropped; }
