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
#define BOARD_IMU_CLOCK_HZ   3000000     /* 3 MHz maximum ✅ */

/*  Câblage du banc — à reporter tel quel :
 *
 *    BNO085            DevKitC        note
 *    ------            -------        ----
 *    VIN / 3V3         3V3            VDD monte avant ou avec VDDIO
 *    GND               GND            masse commune, un seul point
 *    SCK / SCL         GPIO18
 *    SDA / MOSI        GPIO23         « DI » sur certaines cartes
 *    DO  / MISO        GPIO19         « SDO » / « ADR » selon les cartes
 *    CS                GPIO22
 *    INT               GPIO25         actif bas
 *    RST               GPIO33         actif bas
 *    PS0               GPIO26         ⚠️ décoller le pontet I²C de la carte
 *    PS1               3V3            ⚠️ idem — les deux hauts AVANT le reset
 *    BOOTN             3V3 via 10 kΩ  bas au reset = bootloader
 *
 *  ⚠️ Sur les cartes Adafruit et SparkFun, PS0 et PS1 sont strappés pour l'I²C.
 *  Tant que ce pontet n'est pas modifié, le composant démarre en I²C et le SPI
 *  ne répondra jamais — sans message d'erreur, parce qu'il n'y a personne pour
 *  en émettre un. C'est la première chose à vérifier si sh2_getProdIds() échoue.
 */

/* --- IMU, réglages ------------------------------------------------------- */
#define BOARD_IMU_RATE_HZ    100
#define BOARD_IMU_QUEUE_LEN  16

/* --- Identité ------------------------------------------------------------ */
#define BOARD_NODE_ID        RT_NODE_ID_SAFETY

#endif /* RETRIEVER_BOARD_CONFIG_H */
