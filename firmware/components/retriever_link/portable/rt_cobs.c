/* rt_cobs.c — voir rt_cobs.h
 * Copyright (c) 2026 William Hanczyk — Apache License 2.0 */

#include "rt_cobs.h"

size_t rt_cobs_encode(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap)
{
    if (out_cap < RT_COBS_MAX_ENCODED(len)) {
        return 0;
    }

    size_t read = 0;
    size_t write = 1;   /* la position 0 recevra le premier code */
    size_t code_at = 0;
    uint8_t code = 1;

    while (read < len) {
        if (in[read] == 0u) {
            out[code_at] = code;
            code_at = write++;
            code = 1;
        } else {
            out[write++] = in[read];
            if (++code == 0xFFu) {
                /* Bloc plein : on ferme et on en ouvre un nouveau. */
                out[code_at] = code;
                code_at = write++;
                code = 1;
            }
        }
        read++;
    }
    out[code_at] = code;
    return write;
}

size_t rt_cobs_decode(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap)
{
    size_t read = 0;
    size_t write = 0;

    while (read < len) {
        uint8_t code = in[read];
        if (code == 0u) {
            return 0;   /* un zéro dans un bloc encodé : impossible */
        }
        read++;

        for (uint8_t i = 1; i < code; ++i) {
            if (read >= len || write >= out_cap) {
                return 0;
            }
            out[write++] = in[read++];
        }

        /* Un code de 0xFF signale un bloc de 254 octets sans zéro implicite :
         * pas de zéro à réinsérer. Sinon, et sauf en toute fin de bloc, le
         * code représente un zéro. */
        if (code != 0xFFu && read < len) {
            if (write >= out_cap) {
                return 0;
            }
            out[write++] = 0u;
        }
    }
    return write;
}
