/* ===========================================================================
 *  retriever_protocol.h — GÉNÉRÉ, NE PAS MODIFIER À LA MAIN
 *
 *  Source      : firmware/protocol/protocol.yaml
 *  Générateur  : firmware/protocol/generate.py
 *  Version     : 0.1.0
 *  Hash        : 0xE8391C47  (e8391c479174884910363bc1ccd7f7db4624fe10cca7f401faf56a43fde03e80)
 *
 *  Toute modification doit se faire dans le YAML puis passer par le
 *  générateur. La CI (tools/check_protocol_sync.py) échoue sinon.
 *
 *  Représentation : entiers petit-boutiste, charge utile de 8 octets
 *  au plus. Identique en CAN 2.0A et sur le transport série : la couche de
 *  liaison ne voit qu'une trame canonique rt_frame_t.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RETRIEVER_PROTOCOL_H
#define RETRIEVER_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RT_PROTOCOL_VERSION   "0.1.0"
#define RT_PROTOCOL_HASH      0xE8391C47u
#define RT_PROTOCOL_HASH_FULL "e8391c479174884910363bc1ccd7f7db4624fe10cca7f401faf56a43fde03e80"
#define RT_MAX_PAYLOAD        8u
#define RT_ID_BITS            11u
#define RT_ID_MASK            0x7FFu

/* Trame canonique. Le seul type que voit le code applicatif : le transport
 * (CAN ou série) est en dessous et n'apparaît nulle part au-dessus. */
typedef struct {
    uint16_t id;                    /* identifiant sur RT_ID_BITS bits */
    uint8_t  dlc;                   /* 0..RT_MAX_PAYLOAD */
    uint8_t  data[RT_MAX_PAYLOAD];
} rt_frame_t;

/* --- Accès petit-boutiste, sans hypothèse d'alignement --------------------- */

static inline long rt_lround(double v)
{
    return (long)(v >= 0.0 ? v + 0.5 : v - 0.5);
}

static inline void rt_put_u8(uint8_t *p, uint8_t v)   { p[0] = v; }
static inline void rt_put_i8(uint8_t *p, int8_t v)    { p[0] = (uint8_t)v; }
static inline void rt_put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8); }
static inline void rt_put_i16(uint8_t *p, int16_t v)  { rt_put_u16(p, (uint16_t)v); }
static inline void rt_put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static inline void rt_put_i32(uint8_t *p, int32_t v)  { rt_put_u32(p, (uint32_t)v); }
static inline void rt_put_u64(uint8_t *p, uint64_t v)
{
    rt_put_u32(p, (uint32_t)(v & 0xFFFFFFFFu));
    rt_put_u32(p + 4, (uint32_t)(v >> 32));
}

static inline uint8_t  rt_get_u8(const uint8_t *p)  { return p[0]; }
static inline int8_t   rt_get_i8(const uint8_t *p)  { return (int8_t)p[0]; }
static inline uint16_t rt_get_u16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static inline int16_t  rt_get_i16(const uint8_t *p) { return (int16_t)rt_get_u16(p); }
static inline uint32_t rt_get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline int32_t  rt_get_i32(const uint8_t *p) { return (int32_t)rt_get_u32(p); }
static inline uint64_t rt_get_u64(const uint8_t *p)
{
    return (uint64_t)rt_get_u32(p) | ((uint64_t)rt_get_u32(p + 4) << 32);
}

/* --- Énumérations ---------------------------------------------------------- */

/* Identité d'un nœud. Sert d'index dans HEARTBEAT (0x700 + node_id). */
typedef enum {
    RT_NODE_ID_HOST = 0,
    RT_NODE_ID_SAFETY = 1,
    RT_NODE_ID_MOTION_FRONT = 2,
    RT_NODE_ID_MOTION_REAR = 3,
} rt_node_id_e;

/* Commandes d'étalonnage de l'IMU, portées par IMU_CAL_CMD. Le BNO085 sait corriger ses biais en fonctionnement, mais il faut le lui demander, et l'écrire en flash pour que ça survive à la mise hors tension. */
typedef enum {
    RT_IMU_CAL_ACTION_NONE = 0,
    RT_IMU_CAL_ACTION_ENABLE = 1,
    RT_IMU_CAL_ACTION_DISABLE = 2,
    RT_IMU_CAL_ACTION_SAVE = 3,
    RT_IMU_CAL_ACTION_CLEAR = 4,
} rt_imu_cal_action_e;

/* État de haut niveau publié par tout nœud dans son HEARTBEAT. */
typedef enum {
    RT_NODE_STATE_BOOT = 0,
    RT_NODE_STATE_SELFTEST = 1,
    RT_NODE_STATE_READY = 2,
    RT_NODE_STATE_ACTIVE = 3,
    RT_NODE_STATE_DEGRADED = 4,
    RT_NODE_STATE_FAULT = 5,
} rt_node_state_e;

/* FSM de sécurité de l'ESP32-SAFETY (§J.2). Autoritaire. */
typedef enum {
    RT_SAFETY_STATE_INIT = 0,
    RT_SAFETY_STATE_SAFE = 1,
    RT_SAFETY_STATE_PRECHARGE = 2,
    RT_SAFETY_STATE_LIVE_DISARMED = 3,
    RT_SAFETY_STATE_LIVE_ARMED = 4,
    RT_SAFETY_STATE_BRAKING = 5,
    RT_SAFETY_STATE_FAULT = 6,
    RT_SAFETY_STATE_ESTOP = 7,
} rt_safety_state_e;

/* Cause du dernier passage en BRAKING / FAULT / ESTOP. Mémorisée et publiée tant que le défaut n'est pas acquitté : un robot qui s'arrête doit dire pourquoi (§J.2). */
typedef enum {
    RT_FAULT_CAUSE_NONE = 0,
    RT_FAULT_CAUSE_ESTOP_BUTTON = 1,
    RT_FAULT_CAUSE_HOST_HEARTBEAT = 2,
    RT_FAULT_CAUSE_NODE_HEARTBEAT = 3,
    RT_FAULT_CAUSE_BUS_OFF = 4,
    RT_FAULT_CAUSE_OVERCURRENT = 5,
    RT_FAULT_CAUSE_OVERCURRENT_SLOW = 6,
    RT_FAULT_CAUSE_UNDERVOLTAGE = 7,
    RT_FAULT_CAUSE_OVERVOLTAGE_REGEN = 8,
    RT_FAULT_CAUSE_OVERTEMP = 9,
    RT_FAULT_CAUSE_TILT = 10,
    RT_FAULT_CAUSE_PRECHARGE_TIMEOUT = 11,
    RT_FAULT_CAUSE_CONTACTOR_STUCK = 12,
    RT_FAULT_CAUSE_WATCHDOG = 13,
    RT_FAULT_CAUSE_PROTOCOL_MISMATCH = 14,
    RT_FAULT_CAUSE_SELFTEST_FAILED = 15,
} rt_fault_cause_e;

/* Drapeaux par roue (§H.5). Champ de bits. */
#define RT_WHEEL_FLAG_STALL 0x01u
#define RT_WHEEL_FLAG_HALL_FAULT 0x02u
#define RT_WHEEL_FLAG_CMD_TIMEOUT 0x04u
#define RT_WHEEL_FLAG_DRIVER_FAULT 0x08u
#define RT_WHEEL_FLAG_CURRENT_LIMIT 0x10u
#define RT_WHEEL_FLAG_OVERTEMP 0x20u

/* Niveau d'un fragment de journal tunnellisé (§AC.5). */
typedef enum {
    RT_LOG_LEVEL_ERROR = 0,
    RT_LOG_LEVEL_WARN = 1,
    RT_LOG_LEVEL_INFO = 2,
    RT_LOG_LEVEL_DEBUG = 3,
} rt_log_level_e;

/* --- Identifiants ---------------------------------------------------------- */
#define RT_ID_SAFETY_STATE             0x010u
#define RT_ID_ESTOP_REQUEST            0x020u
#define RT_ID_CMD_WHEELS_FRONT         0x100u
#define RT_ID_CMD_WHEELS_REAR          0x101u
#define RT_ID_FB_WHEELS_FRONT          0x180u
#define RT_ID_FB_WHEELS_REAR           0x181u
#define RT_ID_MOT_STATUS_FRONT         0x190u
#define RT_ID_MOT_STATUS_REAR          0x191u
#define RT_ID_POWER                    0x200u
#define RT_ID_BATTERY                  0x201u
#define RT_ID_CELLS_A                  0x202u
#define RT_ID_CELLS_B                  0x203u
#define RT_ID_CELLS_C                  0x204u
#define RT_ID_IMU_QUAT                 0x210u
#define RT_ID_IMU_GYRO                 0x211u
#define RT_ID_IMU_ACCEL                0x212u
#define RT_ID_IMU_MAG                  0x214u
#define RT_ID_IMU_STATUS               0x213u
#define RT_ID_IMU_CAL                  0x215u
#define RT_ID_THERMAL                  0x220u
#define RT_ID_TIME_SYNC                0x300u
#define RT_ID_ARM_REQUEST              0x310u
#define RT_ID_CONFIG                   0x320u
#define RT_ID_IMU_CAL_CMD              0x321u
#define RT_ID_LINK_PING                0x330u
#define RT_ID_LINK_PONG                0x331u
#define RT_ID_LOG                      0x7F0u
#define RT_ID_HEARTBEAT_SAFETY         0x701u
#define RT_ID_HEARTBEAT_MOTION_FRONT   0x702u
#define RT_ID_HEARTBEAT_MOTION_REAR    0x703u

#define RT_DLC_SAFETY_STATE            8u
#define RT_DLC_ESTOP_REQUEST           1u
#define RT_DLC_CMD_WHEELS_FRONT        6u
#define RT_DLC_CMD_WHEELS_REAR         6u
#define RT_DLC_FB_WHEELS_FRONT         8u
#define RT_DLC_FB_WHEELS_REAR          8u
#define RT_DLC_MOT_STATUS_FRONT        4u
#define RT_DLC_MOT_STATUS_REAR         4u
#define RT_DLC_POWER                   8u
#define RT_DLC_BATTERY                 8u
#define RT_DLC_CELLS_A                 8u
#define RT_DLC_CELLS_B                 8u
#define RT_DLC_CELLS_C                 8u
#define RT_DLC_IMU_QUAT                8u
#define RT_DLC_IMU_GYRO                8u
#define RT_DLC_IMU_ACCEL               8u
#define RT_DLC_IMU_MAG                 8u
#define RT_DLC_IMU_STATUS              8u
#define RT_DLC_IMU_CAL                 6u
#define RT_DLC_THERMAL                 8u
#define RT_DLC_TIME_SYNC               8u
#define RT_DLC_ARM_REQUEST             2u
#define RT_DLC_CONFIG                  8u
#define RT_DLC_IMU_CAL_CMD             3u
#define RT_DLC_LINK_PING               7u
#define RT_DLC_LINK_PONG               7u
#define RT_DLC_LOG                     8u
#define RT_DLC_HEARTBEAT_SAFETY        8u
#define RT_DLC_HEARTBEAT_MOTION_FRONT  8u
#define RT_DLC_HEARTBEAT_MOTION_REAR   8u

/* --- Trames ---------------------------------------------------------------- */

/* SAFETY_STATE  id 0x010  dlc 8  émetteur SAFETY  100 Hz  [planned]
 * Miroir de la FSM de sécurité. Aucune autorité côté ROS : lecture seule.
 *
 *   @0 state: u8
 *   @1 cause: u8
 *   @2 arm_blockers: u16  Bitmap des 11 conditions d'armement non satisfaites (§J.2)
 *   @4 flags: u8  b0 estop_input b1 contactor_cmd b2 safe_line b3 bench_mode
 *   @5 counter: u8  Incrémenté à chaque émission, détection de gel
 *   @6 uptime_s: u16 [s]
 */
typedef struct {
    uint8_t  state;
    uint8_t  cause;
    uint16_t arm_blockers;
    uint8_t  flags;
    uint8_t  counter;
    uint16_t uptime_s;
} rt_safety_state_t;

static inline void rt_safety_state_pack(const rt_safety_state_t *m, rt_frame_t *f)
{
    f->id = RT_ID_SAFETY_STATE;
    f->dlc = RT_DLC_SAFETY_STATE;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_state = (uint8_t)(m->state);
    rt_put_u8(f->data + 0, raw_state);
    uint8_t raw_cause = (uint8_t)(m->cause);
    rt_put_u8(f->data + 1, raw_cause);
    uint16_t raw_arm_blockers = (uint16_t)(m->arm_blockers);
    rt_put_u16(f->data + 2, raw_arm_blockers);
    uint8_t raw_flags = (uint8_t)(m->flags);
    rt_put_u8(f->data + 4, raw_flags);
    uint8_t raw_counter = (uint8_t)(m->counter);
    rt_put_u8(f->data + 5, raw_counter);
    uint16_t raw_uptime_s = (uint16_t)(m->uptime_s);
    rt_put_u16(f->data + 6, raw_uptime_s);
}

static inline bool rt_safety_state_unpack(const rt_frame_t *f, rt_safety_state_t *m)
{
    if (f->id != RT_ID_SAFETY_STATE || f->dlc < RT_DLC_SAFETY_STATE) return false;
    m->state = rt_get_u8(f->data + 0);
    m->cause = rt_get_u8(f->data + 1);
    m->arm_blockers = rt_get_u16(f->data + 2);
    m->flags = rt_get_u8(f->data + 4);
    m->counter = rt_get_u8(f->data + 5);
    m->uptime_s = rt_get_u16(f->data + 6);
    return true;
}

/* ESTOP_REQUEST  id 0x020  dlc 1  émetteur HOST  [planned]
 * Arrêt logiciel demandé par le calculateur. N'est PAS un arrêt d'urgence.
 *
 *   @0 magic: u8  0xE5 — une trame corrompue ne doit pas armer un arrêt par hasard
 */
typedef struct {
    uint8_t  magic;
} rt_estop_request_t;

static inline void rt_estop_request_pack(const rt_estop_request_t *m, rt_frame_t *f)
{
    f->id = RT_ID_ESTOP_REQUEST;
    f->dlc = RT_DLC_ESTOP_REQUEST;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_magic = (uint8_t)(m->magic);
    rt_put_u8(f->data + 0, raw_magic);
}

static inline bool rt_estop_request_unpack(const rt_frame_t *f, rt_estop_request_t *m)
{
    if (f->id != RT_ID_ESTOP_REQUEST || f->dlc < RT_DLC_ESTOP_REQUEST) return false;
    m->magic = rt_get_u8(f->data + 0);
    return true;
}

/* CMD_WHEELS_FRONT  id 0x100  dlc 6  émetteur HOST  50 Hz  [planned]
 * Consignes de vitesse roues avant. Gauche puis droite.
 *
 *   @0 left: i16 [rad/s]
 *   @2 right: i16 [rad/s]
 *   @4 seq: u8  Compteur cyclique, rejet des trames rejouées
 *   @5 crc8: u8  CRC8 applicatif sur les 5 octets précédents (§Q niveau 2)
 */
typedef struct {
    float    left;
    float    right;
    uint8_t  seq;
    uint8_t  crc8;
} rt_cmd_wheels_front_t;

static inline void rt_cmd_wheels_front_pack(const rt_cmd_wheels_front_t *m, rt_frame_t *f)
{
    f->id = RT_ID_CMD_WHEELS_FRONT;
    f->dlc = RT_DLC_CMD_WHEELS_FRONT;
    memset(f->data, 0, sizeof(f->data));
    double v_left = ((double)(m->left) - (0.0)) / (0.001);
    if (v_left > 32767.0) v_left = 32767.0;
    if (v_left < -32768.0) v_left = -32768.0;
    int16_t raw_left = (int16_t)rt_lround(v_left);
    rt_put_i16(f->data + 0, raw_left);
    double v_right = ((double)(m->right) - (0.0)) / (0.001);
    if (v_right > 32767.0) v_right = 32767.0;
    if (v_right < -32768.0) v_right = -32768.0;
    int16_t raw_right = (int16_t)rt_lround(v_right);
    rt_put_i16(f->data + 2, raw_right);
    uint8_t raw_seq = (uint8_t)(m->seq);
    rt_put_u8(f->data + 4, raw_seq);
    uint8_t raw_crc8 = (uint8_t)(m->crc8);
    rt_put_u8(f->data + 5, raw_crc8);
}

static inline bool rt_cmd_wheels_front_unpack(const rt_frame_t *f, rt_cmd_wheels_front_t *m)
{
    if (f->id != RT_ID_CMD_WHEELS_FRONT || f->dlc < RT_DLC_CMD_WHEELS_FRONT) return false;
    m->left = (float)((double)(rt_get_i16(f->data + 0)) * (0.001) + (0.0));
    m->right = (float)((double)(rt_get_i16(f->data + 2)) * (0.001) + (0.0));
    m->seq = rt_get_u8(f->data + 4);
    m->crc8 = rt_get_u8(f->data + 5);
    return true;
}

/* CMD_WHEELS_REAR  id 0x101  dlc 6  émetteur HOST  50 Hz  [planned]
 * Même disposition que CMD_WHEELS_FRONT.
 *
 *   @0 left: i16 [rad/s]
 *   @2 right: i16 [rad/s]
 *   @4 seq: u8  Compteur cyclique, rejet des trames rejouées
 *   @5 crc8: u8  CRC8 applicatif sur les 5 octets précédents (§Q niveau 2)
 */
typedef struct {
    float    left;
    float    right;
    uint8_t  seq;
    uint8_t  crc8;
} rt_cmd_wheels_rear_t;

static inline void rt_cmd_wheels_rear_pack(const rt_cmd_wheels_rear_t *m, rt_frame_t *f)
{
    f->id = RT_ID_CMD_WHEELS_REAR;
    f->dlc = RT_DLC_CMD_WHEELS_REAR;
    memset(f->data, 0, sizeof(f->data));
    double v_left = ((double)(m->left) - (0.0)) / (0.001);
    if (v_left > 32767.0) v_left = 32767.0;
    if (v_left < -32768.0) v_left = -32768.0;
    int16_t raw_left = (int16_t)rt_lround(v_left);
    rt_put_i16(f->data + 0, raw_left);
    double v_right = ((double)(m->right) - (0.0)) / (0.001);
    if (v_right > 32767.0) v_right = 32767.0;
    if (v_right < -32768.0) v_right = -32768.0;
    int16_t raw_right = (int16_t)rt_lround(v_right);
    rt_put_i16(f->data + 2, raw_right);
    uint8_t raw_seq = (uint8_t)(m->seq);
    rt_put_u8(f->data + 4, raw_seq);
    uint8_t raw_crc8 = (uint8_t)(m->crc8);
    rt_put_u8(f->data + 5, raw_crc8);
}

static inline bool rt_cmd_wheels_rear_unpack(const rt_frame_t *f, rt_cmd_wheels_rear_t *m)
{
    if (f->id != RT_ID_CMD_WHEELS_REAR || f->dlc < RT_DLC_CMD_WHEELS_REAR) return false;
    m->left = (float)((double)(rt_get_i16(f->data + 0)) * (0.001) + (0.0));
    m->right = (float)((double)(rt_get_i16(f->data + 2)) * (0.001) + (0.0));
    m->seq = rt_get_u8(f->data + 4);
    m->crc8 = rt_get_u8(f->data + 5);
    return true;
}

/* FB_WHEELS_FRONT  id 0x180  dlc 8  émetteur MOTION_FRONT  50 Hz  [planned]
 * Vitesses mesurées et incréments de position, roues avant.
 *
 *   @0 vel_left: i16 [rad/s]
 *   @2 vel_right: i16 [rad/s]
 *   @4 dpos_left: i16 [counts]  Incrément de comptes depuis la trame précédente
 *   @6 dpos_right: i16 [counts]
 */
typedef struct {
    float    vel_left;
    float    vel_right;
    int16_t  dpos_left;
    int16_t  dpos_right;
} rt_fb_wheels_front_t;

static inline void rt_fb_wheels_front_pack(const rt_fb_wheels_front_t *m, rt_frame_t *f)
{
    f->id = RT_ID_FB_WHEELS_FRONT;
    f->dlc = RT_DLC_FB_WHEELS_FRONT;
    memset(f->data, 0, sizeof(f->data));
    double v_vel_left = ((double)(m->vel_left) - (0.0)) / (0.001);
    if (v_vel_left > 32767.0) v_vel_left = 32767.0;
    if (v_vel_left < -32768.0) v_vel_left = -32768.0;
    int16_t raw_vel_left = (int16_t)rt_lround(v_vel_left);
    rt_put_i16(f->data + 0, raw_vel_left);
    double v_vel_right = ((double)(m->vel_right) - (0.0)) / (0.001);
    if (v_vel_right > 32767.0) v_vel_right = 32767.0;
    if (v_vel_right < -32768.0) v_vel_right = -32768.0;
    int16_t raw_vel_right = (int16_t)rt_lround(v_vel_right);
    rt_put_i16(f->data + 2, raw_vel_right);
    int16_t raw_dpos_left = (int16_t)(m->dpos_left);
    rt_put_i16(f->data + 4, raw_dpos_left);
    int16_t raw_dpos_right = (int16_t)(m->dpos_right);
    rt_put_i16(f->data + 6, raw_dpos_right);
}

static inline bool rt_fb_wheels_front_unpack(const rt_frame_t *f, rt_fb_wheels_front_t *m)
{
    if (f->id != RT_ID_FB_WHEELS_FRONT || f->dlc < RT_DLC_FB_WHEELS_FRONT) return false;
    m->vel_left = (float)((double)(rt_get_i16(f->data + 0)) * (0.001) + (0.0));
    m->vel_right = (float)((double)(rt_get_i16(f->data + 2)) * (0.001) + (0.0));
    m->dpos_left = rt_get_i16(f->data + 4);
    m->dpos_right = rt_get_i16(f->data + 6);
    return true;
}

/* FB_WHEELS_REAR  id 0x181  dlc 8  émetteur MOTION_REAR  50 Hz  [planned]
 * Même disposition que FB_WHEELS_FRONT.
 *
 *   @0 vel_left: i16 [rad/s]
 *   @2 vel_right: i16 [rad/s]
 *   @4 dpos_left: i16 [counts]  Incrément de comptes depuis la trame précédente
 *   @6 dpos_right: i16 [counts]
 */
typedef struct {
    float    vel_left;
    float    vel_right;
    int16_t  dpos_left;
    int16_t  dpos_right;
} rt_fb_wheels_rear_t;

static inline void rt_fb_wheels_rear_pack(const rt_fb_wheels_rear_t *m, rt_frame_t *f)
{
    f->id = RT_ID_FB_WHEELS_REAR;
    f->dlc = RT_DLC_FB_WHEELS_REAR;
    memset(f->data, 0, sizeof(f->data));
    double v_vel_left = ((double)(m->vel_left) - (0.0)) / (0.001);
    if (v_vel_left > 32767.0) v_vel_left = 32767.0;
    if (v_vel_left < -32768.0) v_vel_left = -32768.0;
    int16_t raw_vel_left = (int16_t)rt_lround(v_vel_left);
    rt_put_i16(f->data + 0, raw_vel_left);
    double v_vel_right = ((double)(m->vel_right) - (0.0)) / (0.001);
    if (v_vel_right > 32767.0) v_vel_right = 32767.0;
    if (v_vel_right < -32768.0) v_vel_right = -32768.0;
    int16_t raw_vel_right = (int16_t)rt_lround(v_vel_right);
    rt_put_i16(f->data + 2, raw_vel_right);
    int16_t raw_dpos_left = (int16_t)(m->dpos_left);
    rt_put_i16(f->data + 4, raw_dpos_left);
    int16_t raw_dpos_right = (int16_t)(m->dpos_right);
    rt_put_i16(f->data + 6, raw_dpos_right);
}

static inline bool rt_fb_wheels_rear_unpack(const rt_frame_t *f, rt_fb_wheels_rear_t *m)
{
    if (f->id != RT_ID_FB_WHEELS_REAR || f->dlc < RT_DLC_FB_WHEELS_REAR) return false;
    m->vel_left = (float)((double)(rt_get_i16(f->data + 0)) * (0.001) + (0.0));
    m->vel_right = (float)((double)(rt_get_i16(f->data + 2)) * (0.001) + (0.0));
    m->dpos_left = rt_get_i16(f->data + 4);
    m->dpos_right = rt_get_i16(f->data + 6);
    return true;
}

/* MOT_STATUS_FRONT  id 0x190  dlc 4  émetteur MOTION_FRONT  10 Hz  [planned]
 *
 *   @0 flags_left: u8
 *   @1 flags_right: u8
 *   @2 temp_c: i8 [degC]  Dissipateur, NTC
 *   @3 seq: u8
 */
typedef struct {
    uint8_t  flags_left;
    uint8_t  flags_right;
    int8_t   temp_c;
    uint8_t  seq;
} rt_mot_status_front_t;

static inline void rt_mot_status_front_pack(const rt_mot_status_front_t *m, rt_frame_t *f)
{
    f->id = RT_ID_MOT_STATUS_FRONT;
    f->dlc = RT_DLC_MOT_STATUS_FRONT;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_flags_left = (uint8_t)(m->flags_left);
    rt_put_u8(f->data + 0, raw_flags_left);
    uint8_t raw_flags_right = (uint8_t)(m->flags_right);
    rt_put_u8(f->data + 1, raw_flags_right);
    int8_t raw_temp_c = (int8_t)(m->temp_c);
    rt_put_i8(f->data + 2, raw_temp_c);
    uint8_t raw_seq = (uint8_t)(m->seq);
    rt_put_u8(f->data + 3, raw_seq);
}

static inline bool rt_mot_status_front_unpack(const rt_frame_t *f, rt_mot_status_front_t *m)
{
    if (f->id != RT_ID_MOT_STATUS_FRONT || f->dlc < RT_DLC_MOT_STATUS_FRONT) return false;
    m->flags_left = rt_get_u8(f->data + 0);
    m->flags_right = rt_get_u8(f->data + 1);
    m->temp_c = rt_get_i8(f->data + 2);
    m->seq = rt_get_u8(f->data + 3);
    return true;
}

/* MOT_STATUS_REAR  id 0x191  dlc 4  émetteur MOTION_REAR  10 Hz  [planned]
 * Même disposition que MOT_STATUS_FRONT.
 *
 *   @0 flags_left: u8
 *   @1 flags_right: u8
 *   @2 temp_c: i8 [degC]  Dissipateur, NTC
 *   @3 seq: u8
 */
typedef struct {
    uint8_t  flags_left;
    uint8_t  flags_right;
    int8_t   temp_c;
    uint8_t  seq;
} rt_mot_status_rear_t;

static inline void rt_mot_status_rear_pack(const rt_mot_status_rear_t *m, rt_frame_t *f)
{
    f->id = RT_ID_MOT_STATUS_REAR;
    f->dlc = RT_DLC_MOT_STATUS_REAR;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_flags_left = (uint8_t)(m->flags_left);
    rt_put_u8(f->data + 0, raw_flags_left);
    uint8_t raw_flags_right = (uint8_t)(m->flags_right);
    rt_put_u8(f->data + 1, raw_flags_right);
    int8_t raw_temp_c = (int8_t)(m->temp_c);
    rt_put_i8(f->data + 2, raw_temp_c);
    uint8_t raw_seq = (uint8_t)(m->seq);
    rt_put_u8(f->data + 3, raw_seq);
}

static inline bool rt_mot_status_rear_unpack(const rt_frame_t *f, rt_mot_status_rear_t *m)
{
    if (f->id != RT_ID_MOT_STATUS_REAR || f->dlc < RT_DLC_MOT_STATUS_REAR) return false;
    m->flags_left = rt_get_u8(f->data + 0);
    m->flags_right = rt_get_u8(f->data + 1);
    m->temp_c = rt_get_i8(f->data + 2);
    m->seq = rt_get_u8(f->data + 3);
    return true;
}

/* POWER  id 0x200  dlc 8  émetteur SAFETY  20 Hz  [planned]
 * Mesures de la chaîne de puissance. I_bus est signé : négatif =
 * régénération.
 *
 *   @0 v_bus: u16 [V]  Aval contacteur
 *   @2 v_pack: u16 [V]  Amont contacteur
 *   @4 i_bus: i16 [A]
 *   @6 flags: u8  b0 budget_limiting b1 regen b2 precharge_active
 *   @7 seq: u8
 */
typedef struct {
    float    v_bus;
    float    v_pack;
    float    i_bus;
    uint8_t  flags;
    uint8_t  seq;
} rt_power_t;

static inline void rt_power_pack(const rt_power_t *m, rt_frame_t *f)
{
    f->id = RT_ID_POWER;
    f->dlc = RT_DLC_POWER;
    memset(f->data, 0, sizeof(f->data));
    double v_v_bus = ((double)(m->v_bus) - (0.0)) / (0.001);
    if (v_v_bus > 65535.0) v_v_bus = 65535.0;
    if (v_v_bus < 0.0) v_v_bus = 0.0;
    uint16_t raw_v_bus = (uint16_t)rt_lround(v_v_bus);
    rt_put_u16(f->data + 0, raw_v_bus);
    double v_v_pack = ((double)(m->v_pack) - (0.0)) / (0.001);
    if (v_v_pack > 65535.0) v_v_pack = 65535.0;
    if (v_v_pack < 0.0) v_v_pack = 0.0;
    uint16_t raw_v_pack = (uint16_t)rt_lround(v_v_pack);
    rt_put_u16(f->data + 2, raw_v_pack);
    double v_i_bus = ((double)(m->i_bus) - (0.0)) / (0.001);
    if (v_i_bus > 32767.0) v_i_bus = 32767.0;
    if (v_i_bus < -32768.0) v_i_bus = -32768.0;
    int16_t raw_i_bus = (int16_t)rt_lround(v_i_bus);
    rt_put_i16(f->data + 4, raw_i_bus);
    uint8_t raw_flags = (uint8_t)(m->flags);
    rt_put_u8(f->data + 6, raw_flags);
    uint8_t raw_seq = (uint8_t)(m->seq);
    rt_put_u8(f->data + 7, raw_seq);
}

static inline bool rt_power_unpack(const rt_frame_t *f, rt_power_t *m)
{
    if (f->id != RT_ID_POWER || f->dlc < RT_DLC_POWER) return false;
    m->v_bus = (float)((double)(rt_get_u16(f->data + 0)) * (0.001) + (0.0));
    m->v_pack = (float)((double)(rt_get_u16(f->data + 2)) * (0.001) + (0.0));
    m->i_bus = (float)((double)(rt_get_i16(f->data + 4)) * (0.001) + (0.0));
    m->flags = rt_get_u8(f->data + 6);
    m->seq = rt_get_u8(f->data + 7);
    return true;
}

/* BATTERY  id 0x201  dlc 8  émetteur SAFETY  2 Hz  [planned]
 * Télémétrie lue dans le BMS M365 en LECTURE SEULE (§F.5-L5).
 *
 *   @0 soc_pct: u8 [%]
 *   @1 temp_c: i8 [degC]
 *   @2 current_ma: i32 [mA]  Courant rapporté par le BMS, pas par l'ACS758
 *   @6 cycles: u16
 */
typedef struct {
    uint8_t  soc_pct;
    int8_t   temp_c;
    int32_t  current_ma;
    uint16_t cycles;
} rt_battery_t;

static inline void rt_battery_pack(const rt_battery_t *m, rt_frame_t *f)
{
    f->id = RT_ID_BATTERY;
    f->dlc = RT_DLC_BATTERY;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_soc_pct = (uint8_t)(m->soc_pct);
    rt_put_u8(f->data + 0, raw_soc_pct);
    int8_t raw_temp_c = (int8_t)(m->temp_c);
    rt_put_i8(f->data + 1, raw_temp_c);
    int32_t raw_current_ma = (int32_t)(m->current_ma);
    rt_put_i32(f->data + 2, raw_current_ma);
    uint16_t raw_cycles = (uint16_t)(m->cycles);
    rt_put_u16(f->data + 6, raw_cycles);
}

static inline bool rt_battery_unpack(const rt_frame_t *f, rt_battery_t *m)
{
    if (f->id != RT_ID_BATTERY || f->dlc < RT_DLC_BATTERY) return false;
    m->soc_pct = rt_get_u8(f->data + 0);
    m->temp_c = rt_get_i8(f->data + 1);
    m->current_ma = rt_get_i32(f->data + 2);
    m->cycles = rt_get_u16(f->data + 6);
    return true;
}

/* CELLS_A  id 0x202  dlc 8  émetteur SAFETY  0.2 Hz  [planned]
 * Tensions des cellules 1 à 4.
 *
 *   @0 cell1: u16 [mV]
 *   @2 cell2: u16 [mV]
 *   @4 cell3: u16 [mV]
 *   @6 cell4: u16 [mV]
 */
typedef struct {
    uint16_t cell1;
    uint16_t cell2;
    uint16_t cell3;
    uint16_t cell4;
} rt_cells_a_t;

static inline void rt_cells_a_pack(const rt_cells_a_t *m, rt_frame_t *f)
{
    f->id = RT_ID_CELLS_A;
    f->dlc = RT_DLC_CELLS_A;
    memset(f->data, 0, sizeof(f->data));
    uint16_t raw_cell1 = (uint16_t)(m->cell1);
    rt_put_u16(f->data + 0, raw_cell1);
    uint16_t raw_cell2 = (uint16_t)(m->cell2);
    rt_put_u16(f->data + 2, raw_cell2);
    uint16_t raw_cell3 = (uint16_t)(m->cell3);
    rt_put_u16(f->data + 4, raw_cell3);
    uint16_t raw_cell4 = (uint16_t)(m->cell4);
    rt_put_u16(f->data + 6, raw_cell4);
}

static inline bool rt_cells_a_unpack(const rt_frame_t *f, rt_cells_a_t *m)
{
    if (f->id != RT_ID_CELLS_A || f->dlc < RT_DLC_CELLS_A) return false;
    m->cell1 = rt_get_u16(f->data + 0);
    m->cell2 = rt_get_u16(f->data + 2);
    m->cell3 = rt_get_u16(f->data + 4);
    m->cell4 = rt_get_u16(f->data + 6);
    return true;
}

/* CELLS_B  id 0x203  dlc 8  émetteur SAFETY  0.2 Hz  [planned]
 * Tensions des cellules 5 à 8.
 *
 *   @0 cell5: u16 [mV]
 *   @2 cell6: u16 [mV]
 *   @4 cell7: u16 [mV]
 *   @6 cell8: u16 [mV]
 */
typedef struct {
    uint16_t cell5;
    uint16_t cell6;
    uint16_t cell7;
    uint16_t cell8;
} rt_cells_b_t;

static inline void rt_cells_b_pack(const rt_cells_b_t *m, rt_frame_t *f)
{
    f->id = RT_ID_CELLS_B;
    f->dlc = RT_DLC_CELLS_B;
    memset(f->data, 0, sizeof(f->data));
    uint16_t raw_cell5 = (uint16_t)(m->cell5);
    rt_put_u16(f->data + 0, raw_cell5);
    uint16_t raw_cell6 = (uint16_t)(m->cell6);
    rt_put_u16(f->data + 2, raw_cell6);
    uint16_t raw_cell7 = (uint16_t)(m->cell7);
    rt_put_u16(f->data + 4, raw_cell7);
    uint16_t raw_cell8 = (uint16_t)(m->cell8);
    rt_put_u16(f->data + 6, raw_cell8);
}

static inline bool rt_cells_b_unpack(const rt_frame_t *f, rt_cells_b_t *m)
{
    if (f->id != RT_ID_CELLS_B || f->dlc < RT_DLC_CELLS_B) return false;
    m->cell5 = rt_get_u16(f->data + 0);
    m->cell6 = rt_get_u16(f->data + 2);
    m->cell7 = rt_get_u16(f->data + 4);
    m->cell8 = rt_get_u16(f->data + 6);
    return true;
}

/* CELLS_C  id 0x204  dlc 8  émetteur SAFETY  0.2 Hz  [planned]
 * Cellules 9 et 10, puis le delta inter-cellules — l'indicateur de santé
 * n°1.
 *
 *   @0 cell9: u16 [mV]
 *   @2 cell10: u16 [mV]
 *   @4 cell_min: u16 [mV]
 *   @6 delta_mv: u16 [mV]
 */
typedef struct {
    uint16_t cell9;
    uint16_t cell10;
    uint16_t cell_min;
    uint16_t delta_mv;
} rt_cells_c_t;

static inline void rt_cells_c_pack(const rt_cells_c_t *m, rt_frame_t *f)
{
    f->id = RT_ID_CELLS_C;
    f->dlc = RT_DLC_CELLS_C;
    memset(f->data, 0, sizeof(f->data));
    uint16_t raw_cell9 = (uint16_t)(m->cell9);
    rt_put_u16(f->data + 0, raw_cell9);
    uint16_t raw_cell10 = (uint16_t)(m->cell10);
    rt_put_u16(f->data + 2, raw_cell10);
    uint16_t raw_cell_min = (uint16_t)(m->cell_min);
    rt_put_u16(f->data + 4, raw_cell_min);
    uint16_t raw_delta_mv = (uint16_t)(m->delta_mv);
    rt_put_u16(f->data + 6, raw_delta_mv);
}

static inline bool rt_cells_c_unpack(const rt_frame_t *f, rt_cells_c_t *m)
{
    if (f->id != RT_ID_CELLS_C || f->dlc < RT_DLC_CELLS_C) return false;
    m->cell9 = rt_get_u16(f->data + 0);
    m->cell10 = rt_get_u16(f->data + 2);
    m->cell_min = rt_get_u16(f->data + 4);
    m->delta_mv = rt_get_u16(f->data + 6);
    return true;
}

/* IMU_QUAT  id 0x210  dlc 8  émetteur SAFETY  100 Hz  [bench]
 * Quaternion du Rotation Vector, format Q14 — le format natif du BNO085,
 * donc aucune perte de conversion. Les 8 octets sont pleins : le compteur et
 * la qualité voyagent dans GYRO et IMU_STATUS.
 *
 *   @0 w: i16 [1]  Q14, 1/16384
 *   @2 x: i16 [1]
 *   @4 y: i16 [1]
 *   @6 z: i16 [1]
 */
typedef struct {
    float    w;
    float    x;
    float    y;
    float    z;
} rt_imu_quat_t;

static inline void rt_imu_quat_pack(const rt_imu_quat_t *m, rt_frame_t *f)
{
    f->id = RT_ID_IMU_QUAT;
    f->dlc = RT_DLC_IMU_QUAT;
    memset(f->data, 0, sizeof(f->data));
    double v_w = ((double)(m->w) - (0.0)) / (6.103515625e-05);
    if (v_w > 32767.0) v_w = 32767.0;
    if (v_w < -32768.0) v_w = -32768.0;
    int16_t raw_w = (int16_t)rt_lround(v_w);
    rt_put_i16(f->data + 0, raw_w);
    double v_x = ((double)(m->x) - (0.0)) / (6.103515625e-05);
    if (v_x > 32767.0) v_x = 32767.0;
    if (v_x < -32768.0) v_x = -32768.0;
    int16_t raw_x = (int16_t)rt_lround(v_x);
    rt_put_i16(f->data + 2, raw_x);
    double v_y = ((double)(m->y) - (0.0)) / (6.103515625e-05);
    if (v_y > 32767.0) v_y = 32767.0;
    if (v_y < -32768.0) v_y = -32768.0;
    int16_t raw_y = (int16_t)rt_lround(v_y);
    rt_put_i16(f->data + 4, raw_y);
    double v_z = ((double)(m->z) - (0.0)) / (6.103515625e-05);
    if (v_z > 32767.0) v_z = 32767.0;
    if (v_z < -32768.0) v_z = -32768.0;
    int16_t raw_z = (int16_t)rt_lround(v_z);
    rt_put_i16(f->data + 6, raw_z);
}

static inline bool rt_imu_quat_unpack(const rt_frame_t *f, rt_imu_quat_t *m)
{
    if (f->id != RT_ID_IMU_QUAT || f->dlc < RT_DLC_IMU_QUAT) return false;
    m->w = (float)((double)(rt_get_i16(f->data + 0)) * (6.103515625e-05) + (0.0));
    m->x = (float)((double)(rt_get_i16(f->data + 2)) * (6.103515625e-05) + (0.0));
    m->y = (float)((double)(rt_get_i16(f->data + 4)) * (6.103515625e-05) + (0.0));
    m->z = (float)((double)(rt_get_i16(f->data + 6)) * (6.103515625e-05) + (0.0));
    return true;
}

/* IMU_GYRO  id 0x211  dlc 8  émetteur SAFETY  100 Hz  [bench]
 * Gyromètre calibré. Pleine échelle ±16,38 rad/s (±938 °/s) 📐.
 *
 *   @0 gx: i16 [rad/s]
 *   @2 gy: i16 [rad/s]
 *   @4 gz: i16 [rad/s]
 *   @6 seq: u8  Identifie le triplet QUAT+GYRO+ACCEL
 *   @7 flags: u8  b0 quat_valid b1 gyro_valid b2 accel_valid b3 stale
 */
typedef struct {
    float    gx;
    float    gy;
    float    gz;
    uint8_t  seq;
    uint8_t  flags;
} rt_imu_gyro_t;

static inline void rt_imu_gyro_pack(const rt_imu_gyro_t *m, rt_frame_t *f)
{
    f->id = RT_ID_IMU_GYRO;
    f->dlc = RT_DLC_IMU_GYRO;
    memset(f->data, 0, sizeof(f->data));
    double v_gx = ((double)(m->gx) - (0.0)) / (0.0005);
    if (v_gx > 32767.0) v_gx = 32767.0;
    if (v_gx < -32768.0) v_gx = -32768.0;
    int16_t raw_gx = (int16_t)rt_lround(v_gx);
    rt_put_i16(f->data + 0, raw_gx);
    double v_gy = ((double)(m->gy) - (0.0)) / (0.0005);
    if (v_gy > 32767.0) v_gy = 32767.0;
    if (v_gy < -32768.0) v_gy = -32768.0;
    int16_t raw_gy = (int16_t)rt_lround(v_gy);
    rt_put_i16(f->data + 2, raw_gy);
    double v_gz = ((double)(m->gz) - (0.0)) / (0.0005);
    if (v_gz > 32767.0) v_gz = 32767.0;
    if (v_gz < -32768.0) v_gz = -32768.0;
    int16_t raw_gz = (int16_t)rt_lround(v_gz);
    rt_put_i16(f->data + 4, raw_gz);
    uint8_t raw_seq = (uint8_t)(m->seq);
    rt_put_u8(f->data + 6, raw_seq);
    uint8_t raw_flags = (uint8_t)(m->flags);
    rt_put_u8(f->data + 7, raw_flags);
}

static inline bool rt_imu_gyro_unpack(const rt_frame_t *f, rt_imu_gyro_t *m)
{
    if (f->id != RT_ID_IMU_GYRO || f->dlc < RT_DLC_IMU_GYRO) return false;
    m->gx = (float)((double)(rt_get_i16(f->data + 0)) * (0.0005) + (0.0));
    m->gy = (float)((double)(rt_get_i16(f->data + 2)) * (0.0005) + (0.0));
    m->gz = (float)((double)(rt_get_i16(f->data + 4)) * (0.0005) + (0.0));
    m->seq = rt_get_u8(f->data + 6);
    m->flags = rt_get_u8(f->data + 7);
    return true;
}

/* IMU_ACCEL  id 0x212  dlc 8  émetteur SAFETY  100 Hz  [bench]
 * Accélération, gravité incluse. Pleine échelle ±65,5 m/s² 📐.
 *
 *   @0 ax: i16 [m/s^2]
 *   @2 ay: i16 [m/s^2]
 *   @4 az: i16 [m/s^2]
 *   @6 seq: u8
 *   @7 flags: u8
 */
typedef struct {
    float    ax;
    float    ay;
    float    az;
    uint8_t  seq;
    uint8_t  flags;
} rt_imu_accel_t;

static inline void rt_imu_accel_pack(const rt_imu_accel_t *m, rt_frame_t *f)
{
    f->id = RT_ID_IMU_ACCEL;
    f->dlc = RT_DLC_IMU_ACCEL;
    memset(f->data, 0, sizeof(f->data));
    double v_ax = ((double)(m->ax) - (0.0)) / (0.002);
    if (v_ax > 32767.0) v_ax = 32767.0;
    if (v_ax < -32768.0) v_ax = -32768.0;
    int16_t raw_ax = (int16_t)rt_lround(v_ax);
    rt_put_i16(f->data + 0, raw_ax);
    double v_ay = ((double)(m->ay) - (0.0)) / (0.002);
    if (v_ay > 32767.0) v_ay = 32767.0;
    if (v_ay < -32768.0) v_ay = -32768.0;
    int16_t raw_ay = (int16_t)rt_lround(v_ay);
    rt_put_i16(f->data + 2, raw_ay);
    double v_az = ((double)(m->az) - (0.0)) / (0.002);
    if (v_az > 32767.0) v_az = 32767.0;
    if (v_az < -32768.0) v_az = -32768.0;
    int16_t raw_az = (int16_t)rt_lround(v_az);
    rt_put_i16(f->data + 4, raw_az);
    uint8_t raw_seq = (uint8_t)(m->seq);
    rt_put_u8(f->data + 6, raw_seq);
    uint8_t raw_flags = (uint8_t)(m->flags);
    rt_put_u8(f->data + 7, raw_flags);
}

static inline bool rt_imu_accel_unpack(const rt_frame_t *f, rt_imu_accel_t *m)
{
    if (f->id != RT_ID_IMU_ACCEL || f->dlc < RT_DLC_IMU_ACCEL) return false;
    m->ax = (float)((double)(rt_get_i16(f->data + 0)) * (0.002) + (0.0));
    m->ay = (float)((double)(rt_get_i16(f->data + 2)) * (0.002) + (0.0));
    m->az = (float)((double)(rt_get_i16(f->data + 4)) * (0.002) + (0.0));
    m->seq = rt_get_u8(f->data + 6);
    m->flags = rt_get_u8(f->data + 7);
    return true;
}

/* IMU_MAG  id 0x214  dlc 8  émetteur SAFETY  10 Hz  [bench]
 * Champ magnétique calibré. Pleine échelle ±327 µT 📐 — le champ terrestre
 * vaut 25 à 65 µT selon le lieu, donc la marge sert à VOIR les perturbations
 * plutôt qu'à les saturer. C'est précisément l'usage prévu : juger si le cap
 * magnétique restera exploitable une fois l'IMU montée près de quatre
 * moteurs-roues (§N.3).
 *
 *   @0 mx: i16 [uT]
 *   @2 my: i16 [uT]
 *   @4 mz: i16 [uT]
 *   @6 seq: u8
 *   @7 flags: u8  b0..b1 statut d'étalonnage du magnétomètre
 */
typedef struct {
    float    mx;
    float    my;
    float    mz;
    uint8_t  seq;
    uint8_t  flags;
} rt_imu_mag_t;

static inline void rt_imu_mag_pack(const rt_imu_mag_t *m, rt_frame_t *f)
{
    f->id = RT_ID_IMU_MAG;
    f->dlc = RT_DLC_IMU_MAG;
    memset(f->data, 0, sizeof(f->data));
    double v_mx = ((double)(m->mx) - (0.0)) / (0.01);
    if (v_mx > 32767.0) v_mx = 32767.0;
    if (v_mx < -32768.0) v_mx = -32768.0;
    int16_t raw_mx = (int16_t)rt_lround(v_mx);
    rt_put_i16(f->data + 0, raw_mx);
    double v_my = ((double)(m->my) - (0.0)) / (0.01);
    if (v_my > 32767.0) v_my = 32767.0;
    if (v_my < -32768.0) v_my = -32768.0;
    int16_t raw_my = (int16_t)rt_lround(v_my);
    rt_put_i16(f->data + 2, raw_my);
    double v_mz = ((double)(m->mz) - (0.0)) / (0.01);
    if (v_mz > 32767.0) v_mz = 32767.0;
    if (v_mz < -32768.0) v_mz = -32768.0;
    int16_t raw_mz = (int16_t)rt_lround(v_mz);
    rt_put_i16(f->data + 4, raw_mz);
    uint8_t raw_seq = (uint8_t)(m->seq);
    rt_put_u8(f->data + 6, raw_seq);
    uint8_t raw_flags = (uint8_t)(m->flags);
    rt_put_u8(f->data + 7, raw_flags);
}

static inline bool rt_imu_mag_unpack(const rt_frame_t *f, rt_imu_mag_t *m)
{
    if (f->id != RT_ID_IMU_MAG || f->dlc < RT_DLC_IMU_MAG) return false;
    m->mx = (float)((double)(rt_get_i16(f->data + 0)) * (0.01) + (0.0));
    m->my = (float)((double)(rt_get_i16(f->data + 2)) * (0.01) + (0.0));
    m->mz = (float)((double)(rt_get_i16(f->data + 4)) * (0.01) + (0.0));
    m->seq = rt_get_u8(f->data + 6);
    m->flags = rt_get_u8(f->data + 7);
    return true;
}

/* IMU_STATUS  id 0x213  dlc 8  émetteur SAFETY  10 Hz  [bench]
 * Qualité et santé de l'IMU. `quat_accuracy` est l'estimation d'erreur en
 * radians fournie par le BNO085 ; c'est elle qui alimente la covariance
 * d'orientation publiée sur /imu/data. Une covariance nulle fait diverger
 * l'EKF (§I.2), donc ce champ n'est pas décoratif.
 *
 *   @0 quat_accuracy: u16 [rad]
 *   @2 status_rot: u8  0 unreliable, 1 low, 2 medium, 3 high
 *   @3 status_gyro: u8
 *   @4 status_accel: u8
 *   @5 reset_count: u8  Resets du BNO085 depuis le boot — doit rester à 0
 *   @6 dropped: u16  Échantillons perdus faute de place en file
 */
typedef struct {
    float    quat_accuracy;
    uint8_t  status_rot;
    uint8_t  status_gyro;
    uint8_t  status_accel;
    uint8_t  reset_count;
    uint16_t dropped;
} rt_imu_status_t;

static inline void rt_imu_status_pack(const rt_imu_status_t *m, rt_frame_t *f)
{
    f->id = RT_ID_IMU_STATUS;
    f->dlc = RT_DLC_IMU_STATUS;
    memset(f->data, 0, sizeof(f->data));
    double v_quat_accuracy = ((double)(m->quat_accuracy) - (0.0)) / (0.0001);
    if (v_quat_accuracy > 65535.0) v_quat_accuracy = 65535.0;
    if (v_quat_accuracy < 0.0) v_quat_accuracy = 0.0;
    uint16_t raw_quat_accuracy = (uint16_t)rt_lround(v_quat_accuracy);
    rt_put_u16(f->data + 0, raw_quat_accuracy);
    uint8_t raw_status_rot = (uint8_t)(m->status_rot);
    rt_put_u8(f->data + 2, raw_status_rot);
    uint8_t raw_status_gyro = (uint8_t)(m->status_gyro);
    rt_put_u8(f->data + 3, raw_status_gyro);
    uint8_t raw_status_accel = (uint8_t)(m->status_accel);
    rt_put_u8(f->data + 4, raw_status_accel);
    uint8_t raw_reset_count = (uint8_t)(m->reset_count);
    rt_put_u8(f->data + 5, raw_reset_count);
    uint16_t raw_dropped = (uint16_t)(m->dropped);
    rt_put_u16(f->data + 6, raw_dropped);
}

static inline bool rt_imu_status_unpack(const rt_frame_t *f, rt_imu_status_t *m)
{
    if (f->id != RT_ID_IMU_STATUS || f->dlc < RT_DLC_IMU_STATUS) return false;
    m->quat_accuracy = (float)((double)(rt_get_u16(f->data + 0)) * (0.0001) + (0.0));
    m->status_rot = rt_get_u8(f->data + 2);
    m->status_gyro = rt_get_u8(f->data + 3);
    m->status_accel = rt_get_u8(f->data + 4);
    m->reset_count = rt_get_u8(f->data + 5);
    m->dropped = rt_get_u16(f->data + 6);
    return true;
}

/* IMU_CAL  id 0x215  dlc 6  émetteur SAFETY  10 Hz  [bench]
 * État de l'étalonnage dynamique. Deux choses distinctes y sont dites, et
 * les confondre coûte une séance de banc : l'étalonnage est-il ACTIF — le
 * capteur corrige-t-il ses biais en ce moment — et a-t-il été SAUVEGARDÉ —
 * la correction survivra-t-elle à la mise hors tension. `saves` répond à la
 * seconde ; tant qu'il vaut 0, tout le travail d'étalonnage est en RAM.
 *
 *   @0 status_mag: u8  0 non fiable, 1 basse, 2 moyenne, 3 haute
 *   @1 enabled: u8  b0 accel b1 gyro b2 mag — masque réellement accepté par le capteur
 *   @2 saves: u8  Écritures DCD en flash depuis le démarrage
 *   @3 last_action: u8
 *   @4 last_result: i8  Code de retour SH-2 de la dernière commande. 0 = succès
 *   @5 flags: u8  b0 sauvegarde automatique du DCD active
 */
typedef struct {
    uint8_t  status_mag;
    uint8_t  enabled;
    uint8_t  saves;
    uint8_t  last_action;
    int8_t   last_result;
    uint8_t  flags;
} rt_imu_cal_t;

static inline void rt_imu_cal_pack(const rt_imu_cal_t *m, rt_frame_t *f)
{
    f->id = RT_ID_IMU_CAL;
    f->dlc = RT_DLC_IMU_CAL;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_status_mag = (uint8_t)(m->status_mag);
    rt_put_u8(f->data + 0, raw_status_mag);
    uint8_t raw_enabled = (uint8_t)(m->enabled);
    rt_put_u8(f->data + 1, raw_enabled);
    uint8_t raw_saves = (uint8_t)(m->saves);
    rt_put_u8(f->data + 2, raw_saves);
    uint8_t raw_last_action = (uint8_t)(m->last_action);
    rt_put_u8(f->data + 3, raw_last_action);
    int8_t raw_last_result = (int8_t)(m->last_result);
    rt_put_i8(f->data + 4, raw_last_result);
    uint8_t raw_flags = (uint8_t)(m->flags);
    rt_put_u8(f->data + 5, raw_flags);
}

static inline bool rt_imu_cal_unpack(const rt_frame_t *f, rt_imu_cal_t *m)
{
    if (f->id != RT_ID_IMU_CAL || f->dlc < RT_DLC_IMU_CAL) return false;
    m->status_mag = rt_get_u8(f->data + 0);
    m->enabled = rt_get_u8(f->data + 1);
    m->saves = rt_get_u8(f->data + 2);
    m->last_action = rt_get_u8(f->data + 3);
    m->last_result = rt_get_i8(f->data + 4);
    m->flags = rt_get_u8(f->data + 5);
    return true;
}

/* THERMAL  id 0x220  dlc 8  émetteur SAFETY  1 Hz  [planned]
 *
 *   @0 temp_a_c: i8 [degC]
 *   @1 temp_b_c: i8 [degC]
 *   @2 fan_a_pct: u8 [%]
 *   @3 fan_b_pct: u8 [%]
 *   @4 rpm_a: u16 [rpm]
 *   @6 rpm_b: u16 [rpm]
 */
typedef struct {
    int8_t   temp_a_c;
    int8_t   temp_b_c;
    uint8_t  fan_a_pct;
    uint8_t  fan_b_pct;
    uint16_t rpm_a;
    uint16_t rpm_b;
} rt_thermal_t;

static inline void rt_thermal_pack(const rt_thermal_t *m, rt_frame_t *f)
{
    f->id = RT_ID_THERMAL;
    f->dlc = RT_DLC_THERMAL;
    memset(f->data, 0, sizeof(f->data));
    int8_t raw_temp_a_c = (int8_t)(m->temp_a_c);
    rt_put_i8(f->data + 0, raw_temp_a_c);
    int8_t raw_temp_b_c = (int8_t)(m->temp_b_c);
    rt_put_i8(f->data + 1, raw_temp_b_c);
    uint8_t raw_fan_a_pct = (uint8_t)(m->fan_a_pct);
    rt_put_u8(f->data + 2, raw_fan_a_pct);
    uint8_t raw_fan_b_pct = (uint8_t)(m->fan_b_pct);
    rt_put_u8(f->data + 3, raw_fan_b_pct);
    uint16_t raw_rpm_a = (uint16_t)(m->rpm_a);
    rt_put_u16(f->data + 4, raw_rpm_a);
    uint16_t raw_rpm_b = (uint16_t)(m->rpm_b);
    rt_put_u16(f->data + 6, raw_rpm_b);
}

static inline bool rt_thermal_unpack(const rt_frame_t *f, rt_thermal_t *m)
{
    if (f->id != RT_ID_THERMAL || f->dlc < RT_DLC_THERMAL) return false;
    m->temp_a_c = rt_get_i8(f->data + 0);
    m->temp_b_c = rt_get_i8(f->data + 1);
    m->fan_a_pct = rt_get_u8(f->data + 2);
    m->fan_b_pct = rt_get_u8(f->data + 3);
    m->rpm_a = rt_get_u16(f->data + 4);
    m->rpm_b = rt_get_u16(f->data + 6);
    return true;
}

/* TIME_SYNC  id 0x300  dlc 8  émetteur HOST  1 Hz  [bench]
 * Horloge ROS émise par le calculateur. Chaque nœud mémorise le couple
 * (t_host, t_local) ; le calculateur entretient la régression linéaire t_ros
 * = a·t_local + b (§F.4).
 *
 *   @0 t_host_us: u64 [us]  rclcpp::Clock now() en microsecondes
 */
typedef struct {
    uint64_t t_host_us;
} rt_time_sync_t;

static inline void rt_time_sync_pack(const rt_time_sync_t *m, rt_frame_t *f)
{
    f->id = RT_ID_TIME_SYNC;
    f->dlc = RT_DLC_TIME_SYNC;
    memset(f->data, 0, sizeof(f->data));
    uint64_t raw_t_host_us = (uint64_t)(m->t_host_us);
    rt_put_u64(f->data + 0, raw_t_host_us);
}

static inline bool rt_time_sync_unpack(const rt_frame_t *f, rt_time_sync_t *m)
{
    if (f->id != RT_ID_TIME_SYNC || f->dlc < RT_DLC_TIME_SYNC) return false;
    m->t_host_us = rt_get_u64(f->data + 0);
    return true;
}

/* ARM_REQUEST  id 0x310  dlc 2  émetteur HOST  [planned]
 *
 *   @0 arm: u8  1 armer, 0 désarmer
 *   @1 magic: u8  0xA7
 */
typedef struct {
    uint8_t  arm;
    uint8_t  magic;
} rt_arm_request_t;

static inline void rt_arm_request_pack(const rt_arm_request_t *m, rt_frame_t *f)
{
    f->id = RT_ID_ARM_REQUEST;
    f->dlc = RT_DLC_ARM_REQUEST;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_arm = (uint8_t)(m->arm);
    rt_put_u8(f->data + 0, raw_arm);
    uint8_t raw_magic = (uint8_t)(m->magic);
    rt_put_u8(f->data + 1, raw_magic);
}

static inline bool rt_arm_request_unpack(const rt_frame_t *f, rt_arm_request_t *m)
{
    if (f->id != RT_ID_ARM_REQUEST || f->dlc < RT_DLC_ARM_REQUEST) return false;
    m->arm = rt_get_u8(f->data + 0);
    m->magic = rt_get_u8(f->data + 1);
    return true;
}

/* CONFIG  id 0x320  dlc 8  émetteur HOST  [planned]
 * Paramètres écrits une fois au démarrage. ⚠️ Une valeur reçue ici ne peut
 * que RÉDUIRE une limite de sécurité, jamais l'augmenter (§G.4 règle 3).
 *
 *   @0 key: u16
 *   @2 value: i32
 *   @6 seq: u8
 *   @7 magic: u8  0xC0
 */
typedef struct {
    uint16_t key;
    int32_t  value;
    uint8_t  seq;
    uint8_t  magic;
} rt_config_t;

static inline void rt_config_pack(const rt_config_t *m, rt_frame_t *f)
{
    f->id = RT_ID_CONFIG;
    f->dlc = RT_DLC_CONFIG;
    memset(f->data, 0, sizeof(f->data));
    uint16_t raw_key = (uint16_t)(m->key);
    rt_put_u16(f->data + 0, raw_key);
    int32_t raw_value = (int32_t)(m->value);
    rt_put_i32(f->data + 2, raw_value);
    uint8_t raw_seq = (uint8_t)(m->seq);
    rt_put_u8(f->data + 6, raw_seq);
    uint8_t raw_magic = (uint8_t)(m->magic);
    rt_put_u8(f->data + 7, raw_magic);
}

static inline bool rt_config_unpack(const rt_frame_t *f, rt_config_t *m)
{
    if (f->id != RT_ID_CONFIG || f->dlc < RT_DLC_CONFIG) return false;
    m->key = rt_get_u16(f->data + 0);
    m->value = rt_get_i32(f->data + 2);
    m->seq = rt_get_u8(f->data + 6);
    m->magic = rt_get_u8(f->data + 7);
    return true;
}

/* IMU_CAL_CMD  id 0x321  dlc 3  émetteur HOST  [bench]
 * Pilote l'étalonnage de l'IMU depuis le banc. ⚠️ `magic` vaut 0xCA, et le
 * nœud rejette la trame sans lui : effacer un DCD est irréversible et coûte
 * dix minutes de manipulations, ce n'est pas une chose qu'une trame
 * corrompue doit pouvoir déclencher.
 *
 *   @0 action: u8
 *   @1 sensors: u8  b0 accel b1 gyro b2 mag — n'a de sens que pour ENABLE
 *   @2 magic: u8  0xCA
 */
typedef struct {
    uint8_t  action;
    uint8_t  sensors;
    uint8_t  magic;
} rt_imu_cal_cmd_t;

static inline void rt_imu_cal_cmd_pack(const rt_imu_cal_cmd_t *m, rt_frame_t *f)
{
    f->id = RT_ID_IMU_CAL_CMD;
    f->dlc = RT_DLC_IMU_CAL_CMD;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_action = (uint8_t)(m->action);
    rt_put_u8(f->data + 0, raw_action);
    uint8_t raw_sensors = (uint8_t)(m->sensors);
    rt_put_u8(f->data + 1, raw_sensors);
    uint8_t raw_magic = (uint8_t)(m->magic);
    rt_put_u8(f->data + 2, raw_magic);
}

static inline bool rt_imu_cal_cmd_unpack(const rt_frame_t *f, rt_imu_cal_cmd_t *m)
{
    if (f->id != RT_ID_IMU_CAL_CMD || f->dlc < RT_DLC_IMU_CAL_CMD) return false;
    m->action = rt_get_u8(f->data + 0);
    m->sensors = rt_get_u8(f->data + 1);
    m->magic = rt_get_u8(f->data + 2);
    return true;
}

/* LINK_PING  id 0x330  dlc 7  émetteur HOST  [bench]
 * Mesure de latence aller-retour applicative (§Q niveau 2).
 *
 *   @0 target: u8
 *   @1 seq: u16
 *   @3 t_tx_us: u32 [us]
 */
typedef struct {
    uint8_t  target;
    uint16_t seq;
    uint32_t t_tx_us;
} rt_link_ping_t;

static inline void rt_link_ping_pack(const rt_link_ping_t *m, rt_frame_t *f)
{
    f->id = RT_ID_LINK_PING;
    f->dlc = RT_DLC_LINK_PING;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_target = (uint8_t)(m->target);
    rt_put_u8(f->data + 0, raw_target);
    uint16_t raw_seq = (uint16_t)(m->seq);
    rt_put_u16(f->data + 1, raw_seq);
    uint32_t raw_t_tx_us = (uint32_t)(m->t_tx_us);
    rt_put_u32(f->data + 3, raw_t_tx_us);
}

static inline bool rt_link_ping_unpack(const rt_frame_t *f, rt_link_ping_t *m)
{
    if (f->id != RT_ID_LINK_PING || f->dlc < RT_DLC_LINK_PING) return false;
    m->target = rt_get_u8(f->data + 0);
    m->seq = rt_get_u16(f->data + 1);
    m->t_tx_us = rt_get_u32(f->data + 3);
    return true;
}

/* LINK_PONG  id 0x331  dlc 7  émetteur SAFETY  [bench]
 * Écho de LINK_PING, renvoyé sans traitement dans la tâche de réception.
 *
 *   @0 source: u8
 *   @1 seq: u16
 *   @3 t_tx_us: u32 [us]  Recopié tel quel depuis le PING
 */
typedef struct {
    uint8_t  source;
    uint16_t seq;
    uint32_t t_tx_us;
} rt_link_pong_t;

static inline void rt_link_pong_pack(const rt_link_pong_t *m, rt_frame_t *f)
{
    f->id = RT_ID_LINK_PONG;
    f->dlc = RT_DLC_LINK_PONG;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_source = (uint8_t)(m->source);
    rt_put_u8(f->data + 0, raw_source);
    uint16_t raw_seq = (uint16_t)(m->seq);
    rt_put_u16(f->data + 1, raw_seq);
    uint32_t raw_t_tx_us = (uint32_t)(m->t_tx_us);
    rt_put_u32(f->data + 3, raw_t_tx_us);
}

static inline bool rt_link_pong_unpack(const rt_frame_t *f, rt_link_pong_t *m)
{
    if (f->id != RT_ID_LINK_PONG || f->dlc < RT_DLC_LINK_PONG) return false;
    m->source = rt_get_u8(f->data + 0);
    m->seq = rt_get_u16(f->data + 1);
    m->t_tx_us = rt_get_u32(f->data + 3);
    return true;
}

/* LOG  id 0x7F0  dlc 8  émetteur SAFETY  [bench]
 * Fragment de journal firmware tunnellisé sur la liaison (§AC.5). Permet de
 * garder UN SEUL câble sur le banc : la console ESP-IDF n'occupe plus
 * l'UART, et les journaux du firmware arrivent dans /rosout. ⚠️ Jamais
 * bloquant : un fragment qui ne rentre pas en file est perdu, et compté. Le
 * journal ne doit jamais retarder la boucle de contrôle.
 *
 *   @0 header: u8  b7..b5 niveau (log_level), b4 fin de ligne, b3..b0 longueur utile 0..7
 *   @1 c0: u8
 *   @2 c1: u8
 *   @3 c2: u8
 *   @4 c3: u8
 *   @5 c4: u8
 *   @6 c5: u8
 *   @7 c6: u8
 */
typedef struct {
    uint8_t  header;
    uint8_t  c0;
    uint8_t  c1;
    uint8_t  c2;
    uint8_t  c3;
    uint8_t  c4;
    uint8_t  c5;
    uint8_t  c6;
} rt_log_t;

static inline void rt_log_pack(const rt_log_t *m, rt_frame_t *f)
{
    f->id = RT_ID_LOG;
    f->dlc = RT_DLC_LOG;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_header = (uint8_t)(m->header);
    rt_put_u8(f->data + 0, raw_header);
    uint8_t raw_c0 = (uint8_t)(m->c0);
    rt_put_u8(f->data + 1, raw_c0);
    uint8_t raw_c1 = (uint8_t)(m->c1);
    rt_put_u8(f->data + 2, raw_c1);
    uint8_t raw_c2 = (uint8_t)(m->c2);
    rt_put_u8(f->data + 3, raw_c2);
    uint8_t raw_c3 = (uint8_t)(m->c3);
    rt_put_u8(f->data + 4, raw_c3);
    uint8_t raw_c4 = (uint8_t)(m->c4);
    rt_put_u8(f->data + 5, raw_c4);
    uint8_t raw_c5 = (uint8_t)(m->c5);
    rt_put_u8(f->data + 6, raw_c5);
    uint8_t raw_c6 = (uint8_t)(m->c6);
    rt_put_u8(f->data + 7, raw_c6);
}

static inline bool rt_log_unpack(const rt_frame_t *f, rt_log_t *m)
{
    if (f->id != RT_ID_LOG || f->dlc < RT_DLC_LOG) return false;
    m->header = rt_get_u8(f->data + 0);
    m->c0 = rt_get_u8(f->data + 1);
    m->c1 = rt_get_u8(f->data + 2);
    m->c2 = rt_get_u8(f->data + 3);
    m->c3 = rt_get_u8(f->data + 4);
    m->c4 = rt_get_u8(f->data + 5);
    m->c5 = rt_get_u8(f->data + 6);
    m->c6 = rt_get_u8(f->data + 7);
    return true;
}

/* HEARTBEAT_SAFETY  id 0x701  dlc 8  émetteur SAFETY  10 Hz  [bench]
 * ⚠️ Écart assumé par rapport au §F.3, qui annonçait DLC 4. Le §K.2 [P3]
 * exige que le heartbeat porte le hash du protocole ; quatre octets ne
 * suffisent pas à porter état + uptime + erreurs + hash. DLC 8.
 *
 *   @0 state: u8
 *   @1 uptime_s: u16 [s]
 *   @3 err_count: u8  Erreurs de liaison cumulées, saturé à 255
 *   @4 protocol_hash: u32  32 bits de tête du hash de protocol.yaml
 */
typedef struct {
    uint8_t  state;
    uint16_t uptime_s;
    uint8_t  err_count;
    uint32_t protocol_hash;
} rt_heartbeat_safety_t;

static inline void rt_heartbeat_safety_pack(const rt_heartbeat_safety_t *m, rt_frame_t *f)
{
    f->id = RT_ID_HEARTBEAT_SAFETY;
    f->dlc = RT_DLC_HEARTBEAT_SAFETY;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_state = (uint8_t)(m->state);
    rt_put_u8(f->data + 0, raw_state);
    uint16_t raw_uptime_s = (uint16_t)(m->uptime_s);
    rt_put_u16(f->data + 1, raw_uptime_s);
    uint8_t raw_err_count = (uint8_t)(m->err_count);
    rt_put_u8(f->data + 3, raw_err_count);
    uint32_t raw_protocol_hash = (uint32_t)(m->protocol_hash);
    rt_put_u32(f->data + 4, raw_protocol_hash);
}

static inline bool rt_heartbeat_safety_unpack(const rt_frame_t *f, rt_heartbeat_safety_t *m)
{
    if (f->id != RT_ID_HEARTBEAT_SAFETY || f->dlc < RT_DLC_HEARTBEAT_SAFETY) return false;
    m->state = rt_get_u8(f->data + 0);
    m->uptime_s = rt_get_u16(f->data + 1);
    m->err_count = rt_get_u8(f->data + 3);
    m->protocol_hash = rt_get_u32(f->data + 4);
    return true;
}

/* HEARTBEAT_MOTION_FRONT  id 0x702  dlc 8  émetteur MOTION_FRONT  10 Hz  [planned]
 * Même disposition que HEARTBEAT_SAFETY.
 *
 *   @0 state: u8
 *   @1 uptime_s: u16 [s]
 *   @3 err_count: u8  Erreurs de liaison cumulées, saturé à 255
 *   @4 protocol_hash: u32  32 bits de tête du hash de protocol.yaml
 */
typedef struct {
    uint8_t  state;
    uint16_t uptime_s;
    uint8_t  err_count;
    uint32_t protocol_hash;
} rt_heartbeat_motion_front_t;

static inline void rt_heartbeat_motion_front_pack(const rt_heartbeat_motion_front_t *m, rt_frame_t *f)
{
    f->id = RT_ID_HEARTBEAT_MOTION_FRONT;
    f->dlc = RT_DLC_HEARTBEAT_MOTION_FRONT;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_state = (uint8_t)(m->state);
    rt_put_u8(f->data + 0, raw_state);
    uint16_t raw_uptime_s = (uint16_t)(m->uptime_s);
    rt_put_u16(f->data + 1, raw_uptime_s);
    uint8_t raw_err_count = (uint8_t)(m->err_count);
    rt_put_u8(f->data + 3, raw_err_count);
    uint32_t raw_protocol_hash = (uint32_t)(m->protocol_hash);
    rt_put_u32(f->data + 4, raw_protocol_hash);
}

static inline bool rt_heartbeat_motion_front_unpack(const rt_frame_t *f, rt_heartbeat_motion_front_t *m)
{
    if (f->id != RT_ID_HEARTBEAT_MOTION_FRONT || f->dlc < RT_DLC_HEARTBEAT_MOTION_FRONT) return false;
    m->state = rt_get_u8(f->data + 0);
    m->uptime_s = rt_get_u16(f->data + 1);
    m->err_count = rt_get_u8(f->data + 3);
    m->protocol_hash = rt_get_u32(f->data + 4);
    return true;
}

/* HEARTBEAT_MOTION_REAR  id 0x703  dlc 8  émetteur MOTION_REAR  10 Hz  [planned]
 * Même disposition que HEARTBEAT_SAFETY.
 *
 *   @0 state: u8
 *   @1 uptime_s: u16 [s]
 *   @3 err_count: u8  Erreurs de liaison cumulées, saturé à 255
 *   @4 protocol_hash: u32  32 bits de tête du hash de protocol.yaml
 */
typedef struct {
    uint8_t  state;
    uint16_t uptime_s;
    uint8_t  err_count;
    uint32_t protocol_hash;
} rt_heartbeat_motion_rear_t;

static inline void rt_heartbeat_motion_rear_pack(const rt_heartbeat_motion_rear_t *m, rt_frame_t *f)
{
    f->id = RT_ID_HEARTBEAT_MOTION_REAR;
    f->dlc = RT_DLC_HEARTBEAT_MOTION_REAR;
    memset(f->data, 0, sizeof(f->data));
    uint8_t raw_state = (uint8_t)(m->state);
    rt_put_u8(f->data + 0, raw_state);
    uint16_t raw_uptime_s = (uint16_t)(m->uptime_s);
    rt_put_u16(f->data + 1, raw_uptime_s);
    uint8_t raw_err_count = (uint8_t)(m->err_count);
    rt_put_u8(f->data + 3, raw_err_count);
    uint32_t raw_protocol_hash = (uint32_t)(m->protocol_hash);
    rt_put_u32(f->data + 4, raw_protocol_hash);
}

static inline bool rt_heartbeat_motion_rear_unpack(const rt_frame_t *f, rt_heartbeat_motion_rear_t *m)
{
    if (f->id != RT_ID_HEARTBEAT_MOTION_REAR || f->dlc < RT_DLC_HEARTBEAT_MOTION_REAR) return false;
    m->state = rt_get_u8(f->data + 0);
    m->uptime_s = rt_get_u16(f->data + 1);
    m->err_count = rt_get_u8(f->data + 3);
    m->protocol_hash = rt_get_u32(f->data + 4);
    return true;
}

/* --- Table des trames (débogage et outils) --------------------------------- */
typedef struct { uint16_t id; uint8_t dlc; const char *name; } rt_frame_info_t;

static const rt_frame_info_t rt_frame_table[] = {
    { 0x010u, 8u, "SAFETY_STATE" },
    { 0x020u, 1u, "ESTOP_REQUEST" },
    { 0x100u, 6u, "CMD_WHEELS_FRONT" },
    { 0x101u, 6u, "CMD_WHEELS_REAR" },
    { 0x180u, 8u, "FB_WHEELS_FRONT" },
    { 0x181u, 8u, "FB_WHEELS_REAR" },
    { 0x190u, 4u, "MOT_STATUS_FRONT" },
    { 0x191u, 4u, "MOT_STATUS_REAR" },
    { 0x200u, 8u, "POWER" },
    { 0x201u, 8u, "BATTERY" },
    { 0x202u, 8u, "CELLS_A" },
    { 0x203u, 8u, "CELLS_B" },
    { 0x204u, 8u, "CELLS_C" },
    { 0x210u, 8u, "IMU_QUAT" },
    { 0x211u, 8u, "IMU_GYRO" },
    { 0x212u, 8u, "IMU_ACCEL" },
    { 0x214u, 8u, "IMU_MAG" },
    { 0x213u, 8u, "IMU_STATUS" },
    { 0x215u, 6u, "IMU_CAL" },
    { 0x220u, 8u, "THERMAL" },
    { 0x300u, 8u, "TIME_SYNC" },
    { 0x310u, 2u, "ARM_REQUEST" },
    { 0x320u, 8u, "CONFIG" },
    { 0x321u, 3u, "IMU_CAL_CMD" },
    { 0x330u, 7u, "LINK_PING" },
    { 0x331u, 7u, "LINK_PONG" },
    { 0x7F0u, 8u, "LOG" },
    { 0x701u, 8u, "HEARTBEAT_SAFETY" },
    { 0x702u, 8u, "HEARTBEAT_MOTION_FRONT" },
    { 0x703u, 8u, "HEARTBEAT_MOTION_REAR" },
};
#define RT_FRAME_COUNT 30u

static inline const char *rt_frame_name(uint16_t id)
{
    for (unsigned i = 0; i < RT_FRAME_COUNT; ++i)
        if (rt_frame_table[i].id == id) return rt_frame_table[i].name;
    return "UNKNOWN";
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* RETRIEVER_PROTOCOL_H */
