/* ===========================================================================
 *  emit_vectors.c — émet les octets « sur le fil » de chaque trame
 *
 *  Sert à une seule chose : permettre à tools/check_protocol_sync.py de
 *  vérifier que l'implémentation Python du protocole produit exactement les
 *  mêmes octets que l'implémentation C. Trois langages décrivent le même
 *  protocole ; deux sont générés depuis le YAML, le troisième le lit à
 *  l'exécution. Si les trois ne s'accordent pas, c'est ici qu'on l'apprend.
 *
 *  Sortie : une ligne par trame, « NOM<TAB>id<TAB>dlc<TAB>hex ».
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include <stdio.h>
#include <string.h>

#include "retriever_protocol.h"
#include "rt_framing.h"

int main(void)
{
    for (unsigned i = 0; i < RT_FRAME_COUNT; ++i) {
        rt_frame_t f;
        f.id = rt_frame_table[i].id;
        f.dlc = rt_frame_table[i].dlc;
        for (unsigned b = 0; b < RT_MAX_PAYLOAD; ++b) {
            /* Motif déterministe, contenant des zéros et des 0xFF : les deux
             * cas que COBS traite différemment. */
            f.data[b] = (uint8_t)((b == 2u) ? 0x00u : (b == 5u ? 0xFFu : (i * 7u + b)));
        }

        uint8_t wire[RT_WIRE_MAX];
        const size_t n = rt_frame_encode(&f, wire, sizeof(wire));

        printf("%s\t%u\t%u\t", rt_frame_table[i].name, f.id, f.dlc);
        for (size_t k = 0; k < n; ++k) {
            printf("%02x", wire[k]);
        }
        printf("\n");
    }
    return 0;
}
