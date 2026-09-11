/* ===========================================================================
 *  imu.h — interface capteur inertiel, côté firmware
 *
 *  Volontairement minuscule. Toute la complexité du BNO085 — SHTP, SH-2,
 *  séquence de réveil, gestion du reset — est derrière ces quelques fonctions.
 *  Le reste du firmware ne connaît que `rt_imu_sample_t`, et le jour où l'IMU
 *  change, seul ce composant change.
 *
 *  CONVENTION DE REPÈRE — le point qui fait perdre le plus de temps sur ce
 *  genre de projet, alors autant l'écrire ici :
 *
 *    Le BNO085 travaille dans le repère Android ✅ : X vers la droite du
 *    boîtier, Y vers le haut dans le plan de la face, Z sortant de la face.
 *    C'est un repère DIRECT, donc aucune inversion d'axe n'est nécessaire —
 *    le §3 de REP-145 n'a pas à s'appliquer ici.
 *
 *    Le quaternion du Rotation Vector exprime l'orientation du repère capteur
 *    dans un repère monde référencé au nord magnétique et à la gravité ✅,
 *    c'est-à-dire un repère ENU (x est, y nord, z haut) — le MÊME que celui
 *    que REP-103 impose aux repères monde de ROS. Le repère monde n'a donc
 *    aucune conversion à subir.
 *
 *    ⚠️ Le firmware n'applique AUCUNE rotation de montage. Il publie ce que le
 *    capteur mesure, dans les axes du capteur. Le passage du repère capteur au
 *    repère du robot est décrit une seule fois, dans l'URDF, par la position de
 *    `imu_link` par rapport à `base_link`. C'est la manière ROS de faire, et
 *    surtout c'est la seule qui reste vraie quand on démonte le capteur pour le
 *    remonter autrement : on corrige un fichier, pas un firmware.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RETRIEVER_IMU_H
#define RETRIEVER_IMU_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Quelle fusion d'orientation demander au capteur. */
typedef enum {
    /** 9 axes, magnétomètre compris : cap absolu, mais sensible aux moteurs. */
    RT_IMU_MODE_ROTATION_VECTOR = 0,
    /** 6 axes : pas de cap absolu, dérive en lacet, immunité magnétique totale. */
    RT_IMU_MODE_GAME_ROTATION_VECTOR = 1,
    /** 9 axes à corrections de lacet lissées : pas de saut de cap. */
    RT_IMU_MODE_ARVR_STABILIZED_RV = 2,
} rt_imu_mode_t;

typedef struct {
    int spi_host;
    int gpio_sclk;
    int gpio_miso;
    int gpio_mosi;
    int gpio_cs;
    int gpio_intn;   /**< H_INTN, actif bas, obligatoire */
    int gpio_rstn;   /**< reset, actif bas */
    int gpio_ps0;    /**< PS0/WAKE — DOIT être piloté, pas câblé en dur (§C15) */
    int clock_hz;    /**< 3 000 000 au plus ✅ */
    rt_imu_mode_t mode;
    int rate_hz;
    bool enable_mag;
    int queue_len;
} rt_imu_config_t;

/**
 * Valeur de `quat_accuracy_rad` signifiant « le capteur ne fournit pas
 * d'estimation ». C'est la borne haute exacte de l'encodage du protocole
 * (u16 d'échelle 1e-4). ⚠️ Ne JAMAIS publier 0 à la place : pour un EKF, 0
 * n'est pas « inconnu », c'est « parfait ».
 */
#define RT_IMU_ACCURACY_UNREPORTED 6.5535f

#define RT_IMU_VALID_QUAT  0x01u
#define RT_IMU_VALID_GYRO  0x02u
#define RT_IMU_VALID_ACCEL 0x04u
#define RT_IMU_VALID_MAG   0x08u

typedef struct {
    uint64_t t_us;            /**< horloge locale de l'ESP32 à la réception */
    float quat[4];            /**< w, x, y, z — repère capteur dans le monde ENU */
    float quat_accuracy_rad;  /**< estimation d'erreur fournie par le capteur */
    float gyro[3];            /**< rad/s, repère capteur */
    float accel[3];           /**< m/s², gravité COMPRISE (REP-145) */
    float mag[3];             /**< µT, repère capteur */
    uint8_t status_rot;       /**< 0 non fiable … 3 haute */
    uint8_t status_gyro;
    uint8_t status_accel;
    uint8_t status_mag;
    uint8_t valid;            /**< masque RT_IMU_VALID_* */
    uint8_t seq;              /**< compteur d'échantillon, cyclique */
} rt_imu_sample_t;

/**
 * Ouvre le capteur et démarre la tâche de service.
 *
 * ⚠️ Prérequis matériels que le logiciel ne peut pas contourner (§C15) :
 *   - PS1 ET PS0 hauts AVANT le reset et jusqu'après la première assertion de
 *     H_INTN — sinon le composant démarre en I²C, précisément le bus interdit
 *   - BOOTN tiré haut par 10 kΩ — bas au reset = mode bootloader
 *   - VDD monte avant ou avec VDDIO
 *   - sur les cartes Adafruit et SparkFun, PS0/PS1 sont souvent strappés pour
 *     l'I²C : il y a un pontet à modifier
 */
esp_err_t rt_imu_init(const rt_imu_config_t *cfg);

/** Retire un échantillon de la file. Bloque jusqu'à `wait`. */
esp_err_t rt_imu_read(rt_imu_sample_t *out, TickType_t wait);

/** Nombre de resets du capteur depuis le démarrage. Doit rester à zéro. */
uint32_t rt_imu_reset_count(void);

/** Échantillons perdus faute de place en file. */
uint32_t rt_imu_dropped(void);

#ifdef __cplusplus
}
#endif

#endif /* RETRIEVER_IMU_H */
