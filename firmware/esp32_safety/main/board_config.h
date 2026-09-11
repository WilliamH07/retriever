/* ===========================================================================
 *  board_config.h — affectation des broches
 *
 *  ⚠️ CECI EST LA CONFIGURATION DU BANC, pas celle de la carte `safety_power`.
 *  La carte n'est pas routée ; quand elle le sera, l'affectation définitive
 *  ira dans `hardware/pcb/safety_power/icd.yaml`, qui est la source de vérité
 *  pour le matériel, et ce fichier sera mis en correspondance avec elle.
 *
 *  Règles de choix des broches sur ESP32-WROOM-32, et pourquoi :
 *
 *    GPIO6-11   flash SPI intégrée. Jamais.
 *    GPIO12     MTDI, strapping VDD_SDIO. Un tirage haut empêche le démarrage,
 *               avec un symptôme muet ✅. Interdite dans tout le projet (§C14a).
 *    GPIO0,2,15 broches de strapping : utilisables, mais tout tirage externe
 *               change le mode de démarrage. Évitées ici pour que le banc n'ait
 *               aucun piège de plus que nécessaire.
 *    GPIO34-39  ENTRÉE SEULE et SANS tirage interne ✅. Toute entrée sur ces
 *               broches exige un tirage externe de 10 kΩ.
 *    GPIO16,17  inutilisables sur les variantes PSRAM (D0WDR2-V3) ✅ —
 *               mesure P3 non faite, donc on ne s'en sert pas.
 *
 *  H_INTN est volontairement sur GPIO25 et non sur GPIO34 comme le propose le
 *  §G.3 : sur le banc, on veut un tirage interne disponible plutôt qu'une
 *  résistance à souder. Sur la carte finale, où le tirage externe sera au
 *  schéma, GPIO34 redeviendra le bon choix.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RETRIEVER_BOARD_CONFIG_H
#define RETRIEVER_BOARD_CONFIG_H

/* Pour SPI3_HOST. L'interface du composant IMU expose volontairement un `int`
 * plutot qu'un type ESP-IDF, mais la VALEUR doit bien venir de quelque part. */
#include "driver/spi_master.h"

/* --- Liaison ------------------------------------------------------------- */
/* UART0 : le pont USB de la DevKitC. Les ESP_LOGx sont détournés vers des
 * trames LOG (voir link_log.c), donc un seul câble suffit — le bootloader et le
 * gestionnaire de panique, eux, écrivent toujours en clair. */
#define BOARD_LINK_UART_NUM  0
#define BOARD_LINK_UART_TX   1
#define BOARD_LINK_UART_RX   3
#define BOARD_LINK_BAUD      921600

/* Cible CAN, pour mémoire — aucun transceiver n'est câblé sur le banc. */
#define BOARD_CAN_TX         5
#define BOARD_CAN_RX         4

/* --- BNO085, SPI --------------------------------------------------------- */
#define BOARD_IMU_SPI_HOST   SPI3_HOST   /* VSPI */
#define BOARD_IMU_SCLK       18
#define BOARD_IMU_MISO       19
#define BOARD_IMU_MOSI       23
#define BOARD_IMU_CS         22
#define BOARD_IMU_INTN       25          /* actif bas, obligatoire */
#define BOARD_IMU_RSTN       33          /* actif bas */
#define BOARD_IMU_PS0        26          /* PS0/WAKE — piloté, jamais câblé en dur */
/* 1 MHz, pas 3.
 *
 * La datasheet autorise 3 MHz ✅ ; le pilote Adafruit, lui, qui fonctionne,
 * construit son bus à 1 MHz. Pour un flux de 100 Hz, un paquet SHTP de
 * vingt octets prend 160 µs à 1 MHz : la marge de temps ne coûte rien ici,
 * alors qu'une marge insuffisante sur MOSI ne se voit que sous la forme
 * d'écritures refusées par le composant, sans aucune erreur côté maître. */
#define BOARD_IMU_CLOCK_HZ   1000000

/*  Câblage du banc.
 *
 *  ⚠️ LE PIÈGE DE CE CAPTEUR, et il fait perdre une journée à qui l'ignore :
 *  en SPI, ce n'est PAS `SDA` qui porte les données vers le capteur. La
 *  datasheet BNO08x multiplexe les broches d'interface ainsi ✅ :
 *
 *      broche 19   H_SCL (I²C)  →  SCK
 *      broche 20   H_SDA (I²C)  →  H_MISO    capteur → ESP32
 *      broche 17   SA0   (I²C)  →  H_MOSI    ESP32 → capteur
 *      broche 18   —            →  H_CSN
 *      broche 14   H_INTN       →  H_INTN
 *
 *  Autrement dit, la broche de SÉLECTION D'ADRESSE I²C devient MOSI. Selon les
 *  cartes elle est sérigraphiée `ADDR`, `ADR`, `SA0`, `DI` ou `SI`.
 *
 *      Fonction         DevKitC     Sérigraphies rencontrées
 *      --------         -------     ------------------------
 *      alimentation     3V3         `3V3` · `VIN`
 *      masse            GND         `GND`
 *      horloge          GPIO18      `SCL` · `SCK`
 *      capteur → ESP32  GPIO19      `SDA` · `SO` · `MISO`
 *      ESP32 → capteur  GPIO23      `ADDR` · `ADR` · `SA0` · `DI` · `SI`
 *      sélection        GPIO22      `CS`
 *      interruption     GPIO25      `INT`   actif bas
 *      reset            GPIO33      `RST`   actif bas
 *      PS0 / WAKE       GPIO26      `PS0` · `P0` · `WAK`
 *      PS1              3V3         `PS1` · `P1`
 *      BOOTN            3V3 / 10 kΩ `BOOT` · `BT` — absent du connecteur =
 *                                   déjà tiré haut sur la carte
 *
 *  ⚠️ PS1 ET PS0 doivent être HAUTS AVANT LE RESET, sinon le composant démarre
 *  en I²C et le SPI ne répondra jamais — sans message d'erreur, parce qu'il n'y
 *  a personne pour en émettre un (§C15). Sur les cartes où PS0 et PS1 sont des
 *  cavaliers à souder (SparkFun), il faut les fermer. Sur celles où ce sont des
 *  broches, PS1 va au 3V3 et PS0 au GPIO, qui l'emporte sur le tirage bas.
 *
 *  ⚠️ PS0 est repris comme WAKE après le reset ✅. Il doit donc aller à un GPIO,
 *  et jamais être câblé en dur au 3V3.
 *
 *  `RST` doit être câblé, pas laissé en l'air : le firmware fait sa propre
 *  séquence de reset après avoir positionné PS0, et c'est elle qui garantit que
 *  le capteur voit les bons niveaux au bon moment, quel que soit l'ordre de
 *  mise sous tension.
 */

/* --- IMU, réglages ------------------------------------------------------- */
#define BOARD_IMU_RATE_HZ    100
#define BOARD_IMU_QUEUE_LEN  16

/* --- Identité ------------------------------------------------------------ */
#define BOARD_NODE_ID        RT_NODE_ID_SAFETY

#endif /* RETRIEVER_BOARD_CONFIG_H */
