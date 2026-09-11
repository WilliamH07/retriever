/* ===========================================================================
 *  rt_crc16.h — CRC-16/CCITT-FALSE
 *
 *  Polynôme 0x1021, initialisation 0xFFFF, pas de réflexion, pas de XOR final.
 *  C'est la variante « FALSE » du catalogue, celle dont le vecteur de test
 *  CRC("123456789") vaut 0x29B1 — vérifié par test_framing.c.
 *
 *  Pourquoi celui-là : distance de Hamming 4 sur des données bien plus longues
 *  que nos paquets (12 octets au plus), donc toute erreur de 1, 2 ou 3 bits est
 *  détectée, ainsi que toute rafale de 16 bits ou moins. 🟡 (tables de Koopman)
 *
 *  ⚠️ Ce n'est PAS l'équivalent du CRC-15 du CAN, qui est accompagné d'un ACK
 *  et d'une retransmission automatique. Voir §AC.4 : la liaison série détecte,
 *  elle ne corrige pas et ne réémet pas.
 *
 *  C99 pur : ce fichier est compilé à l'identique par le firmware ESP-IDF, par
 *  le nœud ROS 2 et par les tests hôte. Une seule implémentation.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RT_CRC16_H
#define RT_CRC16_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RT_CRC16_INIT 0xFFFFu

/** Met à jour un CRC courant avec un octet. */
uint16_t rt_crc16_byte(uint16_t crc, uint8_t b);

/** CRC d'un tampon complet, initialisation comprise. */
uint16_t rt_crc16(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* RT_CRC16_H */
