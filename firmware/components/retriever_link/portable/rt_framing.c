/* rt_framing.c — voir rt_framing.h
 * Copyright (c) 2026 William Hanczyk — Apache License 2.0 */

#include "rt_framing.h"

#include <string.h>

#include "rt_crc16.h"

size_t rt_frame_encode(const rt_frame_t *f, uint8_t *out, size_t cap)
{
    if (f == NULL || out == NULL || f->dlc > RT_MAX_PAYLOAD || cap < RT_WIRE_MAX) {
        return 0;
    }

    uint8_t pkt[RT_PACKET_MAX];
    pkt[0] = (uint8_t)(f->id & 0xFFu);
    pkt[1] = (uint8_t)(((f->id >> 8) & 0x07u) | (uint8_t)(f->dlc << 4));
    memcpy(pkt + 2, f->data, f->dlc);

    const size_t body = 2u + f->dlc;
    const uint16_t crc = rt_crc16(pkt, body);
    pkt[body] = (uint8_t)(crc & 0xFFu);
    pkt[body + 1] = (uint8_t)(crc >> 8);

    const size_t n = rt_cobs_encode(pkt, body + 2u, out, cap - 1u);
    if (n == 0u) {
        return 0;
    }
    out[n] = RT_DELIMITER;
    return n + 1u;
}

void rt_frame_decoder_init(rt_frame_decoder_t *d)
{
    memset(d, 0, sizeof(*d));
}

bool rt_frame_decoder_push(rt_frame_decoder_t *d, uint8_t byte, rt_frame_t *out)
{
    if (byte != RT_DELIMITER) {
        if (d->len < sizeof(d->buf)) {
            d->buf[d->len++] = byte;
        } else {
            /* Plus long qu'un paquet ne peut l'être : soit on s'est branché au
             * milieu d'un flux, soit la ligne est saturée de bruit. On jette et
             * on attend le prochain délimiteur pour se recaler. */
            d->overflow = true;
        }
        return false;
    }

    /* Délimiteur : fin de paquet. */
    const uint16_t len = d->len;
    const bool was_overflow = d->overflow;
    d->len = 0;
    d->overflow = false;

    if (was_overflow) {
        d->stats.overflows++;
        return false;
    }
    if (len == 0u) {
        /* Délimiteurs consécutifs. Pas une erreur : c'est ce qu'émet une ligne
         * au repos chez certains émetteurs, et c'est aussi ce qu'on obtient
         * juste après un recalage. */
        return false;
    }

    uint8_t pkt[RT_PACKET_MAX];
    const size_t n = rt_cobs_decode(d->buf, len, pkt, sizeof(pkt));
    if (n < 4u) {                      /* en-tête 2 + CRC 2 au minimum */
        d->stats.format_errors++;
        return false;
    }

    const size_t body = n - 2u;
    const uint16_t got = (uint16_t)(pkt[body] | ((uint16_t)pkt[body + 1] << 8));
    if (got != rt_crc16(pkt, body)) {
        d->stats.crc_errors++;
        return false;
    }

    const uint8_t dlc = (uint8_t)(pkt[1] >> 4);
    if (dlc > RT_MAX_PAYLOAD || (size_t)dlc + 2u != body) {
        /* Le CRC est bon mais l'en-tête se contredit : c'est un bug d'émetteur
         * ou une version de protocole différente, pas du bruit. On les compte
         * séparément parce qu'ils ne se diagnostiquent pas pareil. */
        d->stats.format_errors++;
        return false;
    }

    out->id = (uint16_t)(pkt[0] | ((uint16_t)(pkt[1] & 0x07u) << 8));
    out->dlc = dlc;
    memset(out->data, 0, sizeof(out->data));
    memcpy(out->data, pkt + 2, dlc);

    d->stats.frames_ok++;
    return true;
}
