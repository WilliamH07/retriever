/* ===========================================================================
 *  rt_cobs.h — Consistent Overhead Byte Stuffing
 *
 *  Transforme un bloc d'octets quelconque en un bloc qui ne contient AUCUN
 *  zéro. Le zéro devient donc un délimiteur de paquet sans ambiguïté, et un
 *  récepteur qui se branche en cours de flux se resynchronise au zéro suivant.
 *
 *  Pourquoi COBS plutôt que SLIP : le surcoût est borné et minuscule — un
 *  octet, plus un par tranche de 254 — alors que SLIP peut, dans le pire cas,
 *  doubler la taille du paquet. Sur une liaison dimensionnée au plus juste, un
 *  format dont le pire cas est imprévisible est un format qu'on ne peut pas
 *  budgéter.
 *
 *  Référence : Cheshire & Baker, « Consistent Overhead Byte Stuffing »,
 *  IEEE/ACM Transactions on Networking, 1999.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RT_COBS_H
#define RT_COBS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Taille encodée maximale pour `n` octets utiles, délimiteur NON compris. */
#define RT_COBS_MAX_ENCODED(n) ((n) + ((n) / 254u) + 1u)

/**
 * Encode `len` octets. N'écrit PAS le délimiteur final : l'appelant l'ajoute,
 * ce qui lui laisse le choix d'émettre plusieurs paquets d'un seul write.
 *
 * @return nombre d'octets écrits, ou 0 si `out_cap` est insuffisant.
 */
size_t rt_cobs_encode(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap);

/**
 * Décode un bloc encodé, délimiteur exclu.
 *
 * @return nombre d'octets décodés, ou 0 si l'encodage est invalide (code de
 *         saut qui sort du bloc) ou si `out_cap` est insuffisant.
 */
size_t rt_cobs_decode(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* RT_COBS_H */
