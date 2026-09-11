/* ===========================================================================
 *  test_framing.c — tests hôte du cadrage série
 *
 *      make -C firmware/test
 *
 *  Ce que ces tests garantissent, et qui n'est pas négociable :
 *    - le flux encodé ne contient jamais d'octet nul ailleurs qu'aux frontières
 *    - une trame altérée d'un bit n'est jamais rendue à l'application
 *    - un récepteur branché au milieu du flux se recale sans intervention
 *    - le surcoût annoncé (6 octets) est le surcoût réel
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include <stdio.h>
#include <string.h>

#include "rt_cobs.h"
#include "rt_crc16.h"
#include "rt_framing.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            printf("  ÉCHEC %s:%d — ", __FILE__, __LINE__);                    \
            printf(__VA_ARGS__);                                               \
            printf("\n");                                                      \
        }                                                                      \
    } while (0)

/* --------------------------------------------------------------------------
 *  CRC
 * ----------------------------------------------------------------------- */

static void test_crc(void)
{
    printf("CRC-16/CCITT-FALSE\n");
    /* Vecteur de référence universel du catalogue des CRC. S'il tombe, c'est
     * que la variante a changé — et un changement de variante silencieux rend
     * incompatibles deux nœuds qui se croient d'accord. */
    const uint8_t check[] = "123456789";
    CHECK(rt_crc16(check, 9) == 0x29B1u, "CRC(\"123456789\") = 0x%04X, attendu 0x29B1",
          rt_crc16(check, 9));

    const uint8_t empty[1] = {0};
    CHECK(rt_crc16(empty, 0) == 0xFFFFu, "CRC du vide = 0x%04X", rt_crc16(empty, 0));
}

/* --------------------------------------------------------------------------
 *  COBS — vecteurs de l'article de Cheshire & Baker
 * ----------------------------------------------------------------------- */

static void cobs_vector(const char *label, const uint8_t *in, size_t n,
                        const uint8_t *want, size_t want_n)
{
    uint8_t enc[64];
    const size_t got = rt_cobs_encode(in, n, enc, sizeof(enc));
    checks++;
    if (got != want_n || memcmp(enc, want, want_n) != 0) {
        failures++;
        printf("  ÉCHEC COBS %s : obtenu [", label);
        for (size_t i = 0; i < got; ++i) printf("%02X ", enc[i]);
        printf("], attendu [");
        for (size_t i = 0; i < want_n; ++i) printf("%02X ", want[i]);
        printf("]\n");
    }

    uint8_t dec[64];
    const size_t back = rt_cobs_decode(enc, got, dec, sizeof(dec));
    CHECK(back == n && memcmp(dec, in, n) == 0, "COBS %s : aller-retour perdu", label);
}

static void test_cobs(void)
{
    printf("COBS\n");
    {
        const uint8_t in[] = {0x00};
        const uint8_t want[] = {0x01, 0x01};
        cobs_vector("un zéro", in, 1, want, 2);
    }
    {
        const uint8_t in[] = {0x00, 0x00};
        const uint8_t want[] = {0x01, 0x01, 0x01};
        cobs_vector("deux zéros", in, 2, want, 3);
    }
    {
        const uint8_t in[] = {0x11, 0x22, 0x00, 0x33};
        const uint8_t want[] = {0x03, 0x11, 0x22, 0x02, 0x33};
        cobs_vector("zéro au milieu", in, 4, want, 5);
    }
    {
        const uint8_t in[] = {0x11, 0x22, 0x33, 0x44};
        const uint8_t want[] = {0x05, 0x11, 0x22, 0x33, 0x44};
        cobs_vector("sans zéro", in, 4, want, 5);
    }
    {
        const uint8_t in[] = {0x11, 0x00, 0x00, 0x00};
        const uint8_t want[] = {0x02, 0x11, 0x01, 0x01, 0x01};
        cobs_vector("zéros en fin", in, 4, want, 5);
    }

    /* Aller-retour exhaustif sur toutes les valeurs d'un octet, à toutes les
     * longueurs utiles. C'est la propriété dont dépend tout le reste. */
    for (size_t n = 0; n <= RT_PACKET_MAX; ++n) {
        for (unsigned v = 0; v < 256u; ++v) {
            uint8_t in[RT_PACKET_MAX];
            for (size_t i = 0; i < n; ++i) in[i] = (uint8_t)((v + i) & 0xFFu);

            uint8_t enc[RT_WIRE_MAX];
            const size_t e = rt_cobs_encode(in, n, enc, sizeof(enc));
            CHECK(e > 0u, "encodage refusé pour n=%zu", n);
            for (size_t i = 0; i < e; ++i) {
                CHECK(enc[i] != 0u, "octet nul en position %zu d'un bloc encodé", i);
            }
            uint8_t dec[RT_PACKET_MAX];
            const size_t d = rt_cobs_decode(enc, e, dec, sizeof(dec));
            CHECK(d == n && memcmp(dec, in, n) == 0, "aller-retour n=%zu v=%u", n, v);
        }
    }
}

/* --------------------------------------------------------------------------
 *  Cadrage
 * ----------------------------------------------------------------------- */

static bool feed(rt_frame_decoder_t *d, const uint8_t *bytes, size_t n, rt_frame_t *out)
{
    bool got = false;
    for (size_t i = 0; i < n; ++i) {
        if (rt_frame_decoder_push(d, bytes[i], out)) got = true;
    }
    return got;
}

static void test_roundtrip(void)
{
    printf("Cadrage — aller-retour\n");

    rt_frame_decoder_t dec;
    rt_frame_decoder_init(&dec);

    for (unsigned i = 0; i < RT_FRAME_COUNT; ++i) {
        rt_frame_t in;
        in.id = rt_frame_table[i].id;
        in.dlc = rt_frame_table[i].dlc;
        for (unsigned b = 0; b < 8u; ++b) {
            /* Des données qui contiennent des zéros ET des 0xFF : ce sont les
             * deux cas que COBS traite différemment. */
            in.data[b] = (uint8_t)((b == 2u) ? 0x00u : (b == 5u ? 0xFFu : (i * 7u + b)));
        }

        uint8_t wire[RT_WIRE_MAX];
        const size_t n = rt_frame_encode(&in, wire, sizeof(wire));
        CHECK(n > 0u, "%s : encodage refusé", rt_frame_table[i].name);
        CHECK(wire[n - 1u] == RT_DELIMITER, "%s : pas de délimiteur final",
              rt_frame_table[i].name);
        for (size_t k = 0; k + 1u < n; ++k) {
            CHECK(wire[k] != 0u, "%s : octet nul avant le délimiteur",
                  rt_frame_table[i].name);
        }

        rt_frame_t out;
        memset(&out, 0, sizeof(out));
        CHECK(feed(&dec, wire, n, &out), "%s : trame non décodée", rt_frame_table[i].name);
        CHECK(out.id == in.id, "%s : id %03X au lieu de %03X", rt_frame_table[i].name,
              out.id, in.id);
        CHECK(out.dlc == in.dlc, "%s : dlc %u au lieu de %u", rt_frame_table[i].name,
              out.dlc, in.dlc);
        CHECK(memcmp(out.data, in.data, in.dlc) == 0, "%s : données altérées",
              rt_frame_table[i].name);
    }
    CHECK(dec.stats.crc_errors == 0u && dec.stats.format_errors == 0u,
          "erreurs parasites : %u CRC, %u format", dec.stats.crc_errors,
          dec.stats.format_errors);
}

static void test_overhead(void)
{
    printf("Cadrage — surcoût\n");
    rt_frame_t f = {.id = 0x210u, .dlc = 8u, .data = {1, 2, 3, 4, 5, 6, 7, 8}};
    uint8_t wire[RT_WIRE_MAX];
    const size_t n = rt_frame_encode(&f, wire, sizeof(wire));
    /* 2 en-tête + 8 données + 2 CRC + 1 COBS + 1 délimiteur = 14. Le calcul de
     * charge de liaison du §AC.3 repose sur ce chiffre exact. */
    CHECK(n == 14u, "trame pleine sur le fil : %zu octets, attendu 14", n);

    rt_frame_t g = {.id = 0x020u, .dlc = 1u, .data = {0xE5}};
    const size_t m = rt_frame_encode(&g, wire, sizeof(wire));
    CHECK(m == 7u, "trame d'un octet : %zu octets, attendu 7", m);
}

static void test_corruption(void)
{
    printf("Cadrage — altération\n");
    rt_frame_t f = {.id = 0x701u, .dlc = 8u, .data = {9, 8, 7, 6, 5, 4, 3, 2}};
    uint8_t wire[RT_WIRE_MAX];
    const size_t n = rt_frame_encode(&f, wire, sizeof(wire));

    /* Un bit retourné sur chaque octet du corps, un essai à la fois. Aucun ne
     * doit produire une trame. */
    for (size_t pos = 0; pos + 1u < n; ++pos) {
        for (unsigned bit = 0; bit < 8u; ++bit) {
            uint8_t copy[RT_WIRE_MAX];
            memcpy(copy, wire, n);
            copy[pos] ^= (uint8_t)(1u << bit);
            if (copy[pos] == 0u) {
                continue;  /* devient un délimiteur : c'est une perte de trame, pas une acceptation */
            }
            rt_frame_decoder_t d;
            rt_frame_decoder_init(&d);
            rt_frame_t out;
            const bool got = feed(&d, copy, n, &out);
            CHECK(!got, "un bit retourné (octet %zu, bit %u) a été accepté", pos, bit);
        }
    }
}

static void test_resync(void)
{
    printf("Cadrage — resynchronisation\n");
    rt_frame_t f = {.id = 0x211u, .dlc = 8u, .data = {1, 0, 3, 0, 5, 6, 7, 8}};
    uint8_t wire[RT_WIRE_MAX];
    const size_t n = rt_frame_encode(&f, wire, sizeof(wire));

    rt_frame_decoder_t d;
    rt_frame_decoder_init(&d);
    rt_frame_t out;

    /* On se branche au milieu d'une trame : les octets orphelins ne doivent
     * rien produire, et la trame suivante doit passer. */
    CHECK(!feed(&d, wire + 4, n - 4, &out), "un fragment a produit une trame");
    memset(&out, 0, sizeof(out));
    CHECK(feed(&d, wire, n, &out), "pas de recalage après un fragment");
    CHECK(out.id == 0x211u, "id après recalage : 0x%03X", out.id);

    /* Rafale de bruit sans délimiteur, puis deux trames valides. Le bruit se
     * colle à la première trame, qui est donc perdue : c'est le comportement
     * attendu et c'est la garantie réelle — on perd AU PLUS une trame après une
     * rafale, pas le flux. La seconde doit passer. */
    uint8_t noise[40];
    for (size_t i = 0; i < sizeof(noise); ++i) noise[i] = (uint8_t)(0xA0u + i);
    rt_frame_decoder_init(&d);
    feed(&d, noise, sizeof(noise), &out);
    CHECK(!feed(&d, wire, n, &out), "la trame collée au bruit a été acceptée");
    memset(&out, 0, sizeof(out));
    CHECK(feed(&d, wire, n, &out), "pas de recalage sur la trame suivante");
    CHECK(out.id == 0x211u, "id après bruit : 0x%03X", out.id);
    CHECK(d.stats.overflows > 0u || d.stats.format_errors > 0u,
          "le bruit n'a été compté nulle part");
}

int main(void)
{
    printf("cadrage — paquet max %u, fil max %u\n\n",
           (unsigned)RT_PACKET_MAX, (unsigned)RT_WIRE_MAX);

    test_crc();
    test_cobs();
    test_roundtrip();
    test_overhead();
    test_corruption();
    test_resync();

    printf("\n%d vérifications, %d échecs\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
