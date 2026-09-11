/* ===========================================================================
 *  test_protocol.c — tests hôte de l'en-tête généré
 *
 *  Se compile et s'exécute sans ESP-IDF et sans ROS 2 :
 *
 *      make -C firmware/test
 *
 *  Trois familles de vérifications :
 *    1. aller-retour  — pack puis unpack redonne la valeur, à la résolution près
 *    2. vecteurs d'or — la disposition des octets sur le fil est figée
 *    3. saturation    — une valeur hors échelle sature, elle ne boucle pas
 *
 *  ⚠️ Les vecteurs d'or ne sont pas décoratifs. Sans eux, une modification du
 *  générateur peut changer silencieusement le format du fil : le firmware et
 *  ROS restent d'accord entre eux, et tous les deux sont d'accord sur le
 *  mauvais format. Les enregistrements passés deviennent illisibles sans que
 *  rien n'échoue.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "retriever_protocol.h"

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

#define CHECK_NEAR(a, b, tol)                                                  \
    CHECK(fabs((double)(a) - (double)(b)) <= (tol), #a " = %g, attendu %g ± %g", \
          (double)(a), (double)(b), (double)(tol))

static void check_bytes(const char *what, const rt_frame_t *f,
                        const uint8_t *expect, size_t n)
{
    checks++;
    if (f->dlc != n || memcmp(f->data, expect, n) != 0) {
        failures++;
        printf("  ÉCHEC %s : obtenu dlc=%u [", what, f->dlc);
        for (size_t i = 0; i < f->dlc; ++i) printf("%02X ", f->data[i]);
        printf("], attendu dlc=%zu [", n);
        for (size_t i = 0; i < n; ++i) printf("%02X ", expect[i]);
        printf("]\n");
    }
}

/* --------------------------------------------------------------------------
 *  1. Aller-retour
 * ----------------------------------------------------------------------- */

static void test_imu_quat(void)
{
    printf("IMU_QUAT\n");
    /* Quaternion d'une rotation de 90° autour de z. */
    rt_imu_quat_t in = {.w = 0.70710678f, .x = 0.0f, .y = 0.0f, .z = 0.70710678f};
    rt_frame_t f;
    rt_imu_quat_pack(&in, &f);

    CHECK(f.id == 0x210u, "id = 0x%03X", f.id);
    CHECK(f.dlc == 8u, "dlc = %u", f.dlc);

    rt_imu_quat_t out;
    CHECK(rt_imu_quat_unpack(&f, &out), "unpack refusé");

    /* Résolution Q14 : un demi-LSB vaut 1/32768. */
    const double lsb = 1.0 / 16384.0;
    CHECK_NEAR(out.w, in.w, lsb);
    CHECK_NEAR(out.x, in.x, lsb);
    CHECK_NEAR(out.y, in.y, lsb);
    CHECK_NEAR(out.z, in.z, lsb);

    /* La norme doit survivre à l'aller-retour : c'est le test que fait aussi
     * le pipeline d'auto-test [P4]. */
    double norm = sqrt((double)out.w * out.w + (double)out.x * out.x +
                       (double)out.y * out.y + (double)out.z * out.z);
    CHECK_NEAR(norm, 1.0, 1e-3);

    /* Vecteur d'or : 0.70710678 / (1/16384) = 11585.2 → 11585 = 0x2D41. */
    const uint8_t expect[8] = {0x41, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x41, 0x2D};
    check_bytes("IMU_QUAT 90° z", &f, expect, 8);
}

static void test_imu_gyro(void)
{
    printf("IMU_GYRO\n");
    rt_imu_gyro_t in = {.gx = 0.5f, .gy = -1.25f, .gz = 0.0f, .seq = 42, .flags = 0x07};
    rt_frame_t f;
    rt_imu_gyro_pack(&in, &f);

    rt_imu_gyro_t out;
    CHECK(rt_imu_gyro_unpack(&f, &out), "unpack refusé");
    CHECK_NEAR(out.gx, 0.5, 0.0005);
    CHECK_NEAR(out.gy, -1.25, 0.0005);
    CHECK_NEAR(out.gz, 0.0, 0.0005);
    CHECK(out.seq == 42, "seq = %u", out.seq);
    CHECK(out.flags == 0x07, "flags = 0x%02X", out.flags);

    /* 0.5 / 0.0005 = 1000 = 0x03E8 ; -1.25 / 0.0005 = -2500 = 0xF63C. */
    const uint8_t expect[8] = {0xE8, 0x03, 0x3C, 0xF6, 0x00, 0x00, 42, 0x07};
    check_bytes("IMU_GYRO", &f, expect, 8);
}

static void test_heartbeat(void)
{
    printf("HEARTBEAT_SAFETY\n");
    rt_heartbeat_safety_t in = {
        .state = RT_NODE_STATE_READY,
        .uptime_s = 3600,
        .err_count = 0,
        .protocol_hash = RT_PROTOCOL_HASH,
    };
    rt_frame_t f;
    rt_heartbeat_safety_pack(&in, &f);

    rt_heartbeat_safety_t out;
    CHECK(rt_heartbeat_safety_unpack(&f, &out), "unpack refusé");
    CHECK(out.state == RT_NODE_STATE_READY, "state = %u", out.state);
    CHECK(out.uptime_s == 3600, "uptime = %u", out.uptime_s);
    CHECK(out.protocol_hash == RT_PROTOCOL_HASH, "hash = 0x%08X", out.protocol_hash);
}

static void test_time_sync(void)
{
    printf("TIME_SYNC\n");
    /* Une date ROS plausible en microsecondes depuis l'époque : au-delà de
     * 2^32, ce qui vérifie que le champ 64 bits n'est pas tronqué. */
    const uint64_t t = 1789200000000000ull;
    rt_time_sync_t in = {.t_host_us = t};
    rt_frame_t f;
    rt_time_sync_pack(&in, &f);

    rt_time_sync_t out;
    CHECK(rt_time_sync_unpack(&f, &out), "unpack refusé");
    CHECK(out.t_host_us == t, "t = %llu, attendu %llu",
          (unsigned long long)out.t_host_us, (unsigned long long)t);
}

/* --------------------------------------------------------------------------
 *  2. Rejets
 * ----------------------------------------------------------------------- */

static void test_rejects(void)
{
    printf("Rejets\n");
    rt_frame_t f = {.id = 0x210u, .dlc = 8u, .data = {0}};
    rt_imu_gyro_t g;
    CHECK(!rt_imu_gyro_unpack(&f, &g), "un mauvais identifiant a été accepté");

    f.id = RT_ID_IMU_GYRO;
    f.dlc = 4u;
    CHECK(!rt_imu_gyro_unpack(&f, &g), "une trame trop courte a été acceptée");

    f.dlc = 8u;
    CHECK(rt_imu_gyro_unpack(&f, &g), "une trame valide a été refusée");
}

/* --------------------------------------------------------------------------
 *  3. Saturation
 * ----------------------------------------------------------------------- */

static void test_saturation(void)
{
    printf("Saturation\n");
    /* Pleine échelle gyro : ±32767 × 0.0005 = ±16.3835 rad/s. Une valeur au
     * double doit saturer, surtout pas repasser par zéro : un gyromètre qui
     * saturerait en changeant de signe ferait diverger l'EKF de façon
     * spectaculaire. */
    rt_imu_gyro_t in = {.gx = 40.0f, .gy = -40.0f, .gz = 0.0f, .seq = 0, .flags = 0};
    rt_frame_t f;
    rt_imu_gyro_pack(&in, &f);

    rt_imu_gyro_t out;
    rt_imu_gyro_unpack(&f, &out);
    CHECK(out.gx > 16.0f && out.gx < 16.4f, "gx saturé à %g", (double)out.gx);
    CHECK(out.gy < -16.0f && out.gy > -16.4f, "gy saturé à %g", (double)out.gy);

    rt_imu_quat_t q = {.w = 5.0f, .x = -5.0f, .y = 0.0f, .z = 0.0f};
    rt_imu_quat_pack(&q, &f);
    rt_imu_quat_t qo;
    rt_imu_quat_unpack(&f, &qo);
    CHECK(qo.w > 1.9f && qo.w < 2.01f, "w saturé à %g", (double)qo.w);
    CHECK(qo.x < -1.9f && qo.x > -2.01f, "x saturé à %g", (double)qo.x);
}

/* --------------------------------------------------------------------------
 *  4. Cohérence de la table
 * ----------------------------------------------------------------------- */

static void test_table(void)
{
    printf("Table des trames\n");
    CHECK(RT_FRAME_COUNT > 0u, "table vide");
    for (unsigned i = 0; i < RT_FRAME_COUNT; ++i) {
        CHECK(rt_frame_table[i].dlc <= RT_MAX_PAYLOAD, "%s : dlc %u > %u",
              rt_frame_table[i].name, rt_frame_table[i].dlc, RT_MAX_PAYLOAD);
        CHECK((rt_frame_table[i].id & ~RT_ID_MASK) == 0u, "%s : id hors 11 bits",
              rt_frame_table[i].name);
        for (unsigned j = i + 1; j < RT_FRAME_COUNT; ++j) {
            CHECK(rt_frame_table[i].id != rt_frame_table[j].id,
                  "identifiant en double : %s et %s", rt_frame_table[i].name,
                  rt_frame_table[j].name);
        }
    }
    CHECK(strcmp(rt_frame_name(RT_ID_IMU_QUAT), "IMU_QUAT") == 0, "nom incorrect");
    CHECK(strcmp(rt_frame_name(0x7FFu), "UNKNOWN") == 0, "identifiant inconnu mal traité");
}

int main(void)
{
    printf("protocole %s, hash 0x%08X\n\n", RT_PROTOCOL_VERSION, RT_PROTOCOL_HASH);

    test_imu_quat();
    test_imu_gyro();
    test_heartbeat();
    test_time_sync();
    test_rejects();
    test_saturation();
    test_table();

    printf("\n%d vérifications, %d échecs\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
