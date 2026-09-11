/* ===========================================================================
 *  rt_framing.h — cadrage d'une trame canonique sur un flux d'octets
 *
 *  C'est la seule pièce que le transport série ajoute par rapport au CAN. Le
 *  CAN sait déjà où commence et où finit une trame, et il porte son propre CRC.
 *  Un flux série ne sait rien de tout cela : il faut le lui donner.
 *
 *  Format du paquet, AVANT encodage COBS :
 *
 *      octet 0     id & 0xFF
 *      octet 1     ((id >> 8) & 0x07) | (dlc << 4)
 *      octets 2..  données, `dlc` octets
 *      2 derniers  CRC-16/CCITT-FALSE des octets précédents, petit-boutiste
 *
 *  Puis : encodage COBS de l'ensemble, et un octet 0x00 de délimitation.
 *
 *      trame de 8 octets utiles → 14 octets sur le fil (6 de surcoût)
 *
 *  Le décodeur est incrémental et sans allocation : on lui donne les octets au
 *  fil de l'eau, il rend une trame quand il en a une complète et valide. Un
 *  récepteur qui démarre au milieu d'un paquet perd ce paquet et se recale sur
 *  le zéro suivant, sans état bloquant.
 *
 *  C99 pur, aucune dépendance ESP-IDF ni ROS : compilé à l'identique des deux
 *  côtés de la liaison et par les tests hôte.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RT_FRAMING_H
#define RT_FRAMING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "retriever_protocol.h"
#include "rt_cobs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Taille du paquet non encodé : en-tête + charge utile + CRC. */
#define RT_PACKET_MAX (2u + RT_MAX_PAYLOAD + 2u)

/** Taille maximale sur le fil, délimiteur compris. */
#define RT_WIRE_MAX (RT_COBS_MAX_ENCODED(RT_PACKET_MAX) + 1u)

/** Octet de délimitation. Par construction COBS, il n'apparaît nulle part ailleurs. */
#define RT_DELIMITER 0x00u

typedef struct {
    uint32_t frames_ok;      /**< trames décodées et validées */
    uint32_t crc_errors;     /**< cadrage correct, CRC faux : bruit sur la ligne */
    uint32_t format_errors;  /**< COBS invalide, longueur incohérente, dlc > 8 */
    uint32_t overflows;      /**< paquet plus long que RT_WIRE_MAX : flux désynchronisé */
} rt_framing_stats_t;

typedef struct {
    uint8_t buf[RT_WIRE_MAX];
    uint16_t len;
    bool overflow;              /**< le paquet courant est fichu, on attend le prochain zéro */
    rt_framing_stats_t stats;
} rt_frame_decoder_t;

/**
 * Encode une trame, délimiteur final compris.
 *
 * @return nombre d'octets écrits, ou 0 si la trame est invalide (dlc > 8) ou si
 *         `cap` est insuffisant. `cap` doit valoir au moins RT_WIRE_MAX.
 */
size_t rt_frame_encode(const rt_frame_t *f, uint8_t *out, size_t cap);

/** Remet le décodeur à zéro, statistiques comprises. */
void rt_frame_decoder_init(rt_frame_decoder_t *d);

/**
 * Donne un octet au décodeur.
 *
 * @return true si `out` contient une trame complète et valide. Les paquets
 *         rejetés incrémentent les compteurs et ne rendent rien : un appelant
 *         qui ignore la valeur de retour ne peut pas traiter une trame fausse.
 */
bool rt_frame_decoder_push(rt_frame_decoder_t *d, uint8_t byte, rt_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif /* RT_FRAMING_H */
