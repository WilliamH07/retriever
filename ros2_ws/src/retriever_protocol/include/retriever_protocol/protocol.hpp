// ===========================================================================
//  protocol.hpp — GÉNÉRÉ, NE PAS MODIFIER À LA MAIN
//
//  Source     : firmware/protocol/protocol.yaml
//  Générateur : firmware/protocol/generate.py
//  Version    : 0.1.0
//  Hash       : 0xC1214F10
//
//  Enveloppe C++17 au-dessus de l'en-tête C. Le code de sérialisation n'est
//  PAS dupliqué : ce fichier inclut retriever_protocol.h, exactement le même
//  que celui compilé dans le firmware. Un désaccord de sérialisation entre le
//  robot et le calculateur est donc structurellement impossible.
//
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "retriever_protocol/retriever_protocol.h"

namespace retriever::protocol
{

inline constexpr std::string_view kVersion = RT_PROTOCOL_VERSION;
inline constexpr std::uint32_t    kHash = RT_PROTOCOL_HASH;
inline constexpr std::size_t      kMaxPayload = RT_MAX_PAYLOAD;

using Frame = ::rt_frame_t;

/// Nom lisible d'un identifiant, ou "UNKNOWN".
inline std::string_view frame_name(std::uint16_t id) { return ::rt_frame_name(id); }

/// Longueur attendue d'une trame, ou nullopt si l'identifiant est inconnu.
inline std::optional<std::uint8_t> expected_dlc(std::uint16_t id)
{
    for (unsigned i = 0; i < RT_FRAME_COUNT; ++i) {
        if (rt_frame_table[i].id == id) return rt_frame_table[i].dlc;
    }
    return std::nullopt;
}

// --- Énumérations ---------------------------------------------------------
namespace enums
{
using NodeId = ::rt_node_id_e;
using NodeState = ::rt_node_state_e;
using SafetyState = ::rt_safety_state_e;
using FaultCause = ::rt_fault_cause_e;
using LogLevel = ::rt_log_level_e;
}  // namespace enums

// SAFETY_STATE — id 0x010, dlc 8, émetteur SAFETY [planned]
using SafetyState = ::rt_safety_state_t;
inline constexpr std::uint16_t kSafetyStateId = RT_ID_SAFETY_STATE;
inline constexpr std::uint8_t  kSafetyStateDlc = RT_DLC_SAFETY_STATE;
inline Frame pack(const SafetyState & m)
{
    Frame f{};
    ::rt_safety_state_pack(&m, &f);
    return f;
}
inline std::optional<SafetyState> unpack_safety_state(const Frame & f)
{
    SafetyState m{};
    if (!::rt_safety_state_unpack(&f, &m)) return std::nullopt;
    return m;
}

// ESTOP_REQUEST — id 0x020, dlc 1, émetteur HOST [planned]
using EstopRequest = ::rt_estop_request_t;
inline constexpr std::uint16_t kEstopRequestId = RT_ID_ESTOP_REQUEST;
inline constexpr std::uint8_t  kEstopRequestDlc = RT_DLC_ESTOP_REQUEST;
inline Frame pack(const EstopRequest & m)
{
    Frame f{};
    ::rt_estop_request_pack(&m, &f);
    return f;
}
inline std::optional<EstopRequest> unpack_estop_request(const Frame & f)
{
    EstopRequest m{};
    if (!::rt_estop_request_unpack(&f, &m)) return std::nullopt;
    return m;
}

// CMD_WHEELS_FRONT — id 0x100, dlc 6, émetteur HOST [planned]
using CmdWheelsFront = ::rt_cmd_wheels_front_t;
inline constexpr std::uint16_t kCmdWheelsFrontId = RT_ID_CMD_WHEELS_FRONT;
inline constexpr std::uint8_t  kCmdWheelsFrontDlc = RT_DLC_CMD_WHEELS_FRONT;
inline Frame pack(const CmdWheelsFront & m)
{
    Frame f{};
    ::rt_cmd_wheels_front_pack(&m, &f);
    return f;
}
inline std::optional<CmdWheelsFront> unpack_cmd_wheels_front(const Frame & f)
{
    CmdWheelsFront m{};
    if (!::rt_cmd_wheels_front_unpack(&f, &m)) return std::nullopt;
    return m;
}

// CMD_WHEELS_REAR — id 0x101, dlc 6, émetteur HOST [planned]
using CmdWheelsRear = ::rt_cmd_wheels_rear_t;
inline constexpr std::uint16_t kCmdWheelsRearId = RT_ID_CMD_WHEELS_REAR;
inline constexpr std::uint8_t  kCmdWheelsRearDlc = RT_DLC_CMD_WHEELS_REAR;
inline Frame pack(const CmdWheelsRear & m)
{
    Frame f{};
    ::rt_cmd_wheels_rear_pack(&m, &f);
    return f;
}
inline std::optional<CmdWheelsRear> unpack_cmd_wheels_rear(const Frame & f)
{
    CmdWheelsRear m{};
    if (!::rt_cmd_wheels_rear_unpack(&f, &m)) return std::nullopt;
    return m;
}

// FB_WHEELS_FRONT — id 0x180, dlc 8, émetteur MOTION_FRONT [planned]
using FbWheelsFront = ::rt_fb_wheels_front_t;
inline constexpr std::uint16_t kFbWheelsFrontId = RT_ID_FB_WHEELS_FRONT;
inline constexpr std::uint8_t  kFbWheelsFrontDlc = RT_DLC_FB_WHEELS_FRONT;
inline Frame pack(const FbWheelsFront & m)
{
    Frame f{};
    ::rt_fb_wheels_front_pack(&m, &f);
    return f;
}
inline std::optional<FbWheelsFront> unpack_fb_wheels_front(const Frame & f)
{
    FbWheelsFront m{};
    if (!::rt_fb_wheels_front_unpack(&f, &m)) return std::nullopt;
    return m;
}

// FB_WHEELS_REAR — id 0x181, dlc 8, émetteur MOTION_REAR [planned]
using FbWheelsRear = ::rt_fb_wheels_rear_t;
inline constexpr std::uint16_t kFbWheelsRearId = RT_ID_FB_WHEELS_REAR;
inline constexpr std::uint8_t  kFbWheelsRearDlc = RT_DLC_FB_WHEELS_REAR;
inline Frame pack(const FbWheelsRear & m)
{
    Frame f{};
    ::rt_fb_wheels_rear_pack(&m, &f);
    return f;
}
inline std::optional<FbWheelsRear> unpack_fb_wheels_rear(const Frame & f)
{
    FbWheelsRear m{};
    if (!::rt_fb_wheels_rear_unpack(&f, &m)) return std::nullopt;
    return m;
}

// MOT_STATUS_FRONT — id 0x190, dlc 4, émetteur MOTION_FRONT [planned]
using MotStatusFront = ::rt_mot_status_front_t;
inline constexpr std::uint16_t kMotStatusFrontId = RT_ID_MOT_STATUS_FRONT;
inline constexpr std::uint8_t  kMotStatusFrontDlc = RT_DLC_MOT_STATUS_FRONT;
inline Frame pack(const MotStatusFront & m)
{
    Frame f{};
    ::rt_mot_status_front_pack(&m, &f);
    return f;
}
inline std::optional<MotStatusFront> unpack_mot_status_front(const Frame & f)
{
    MotStatusFront m{};
    if (!::rt_mot_status_front_unpack(&f, &m)) return std::nullopt;
    return m;
}

// MOT_STATUS_REAR — id 0x191, dlc 4, émetteur MOTION_REAR [planned]
using MotStatusRear = ::rt_mot_status_rear_t;
inline constexpr std::uint16_t kMotStatusRearId = RT_ID_MOT_STATUS_REAR;
inline constexpr std::uint8_t  kMotStatusRearDlc = RT_DLC_MOT_STATUS_REAR;
inline Frame pack(const MotStatusRear & m)
{
    Frame f{};
    ::rt_mot_status_rear_pack(&m, &f);
    return f;
}
inline std::optional<MotStatusRear> unpack_mot_status_rear(const Frame & f)
{
    MotStatusRear m{};
    if (!::rt_mot_status_rear_unpack(&f, &m)) return std::nullopt;
    return m;
}

// POWER — id 0x200, dlc 8, émetteur SAFETY [planned]
using Power = ::rt_power_t;
inline constexpr std::uint16_t kPowerId = RT_ID_POWER;
inline constexpr std::uint8_t  kPowerDlc = RT_DLC_POWER;
inline Frame pack(const Power & m)
{
    Frame f{};
    ::rt_power_pack(&m, &f);
    return f;
}
inline std::optional<Power> unpack_power(const Frame & f)
{
    Power m{};
    if (!::rt_power_unpack(&f, &m)) return std::nullopt;
    return m;
}

// BATTERY — id 0x201, dlc 8, émetteur SAFETY [planned]
using Battery = ::rt_battery_t;
inline constexpr std::uint16_t kBatteryId = RT_ID_BATTERY;
inline constexpr std::uint8_t  kBatteryDlc = RT_DLC_BATTERY;
inline Frame pack(const Battery & m)
{
    Frame f{};
    ::rt_battery_pack(&m, &f);
    return f;
}
inline std::optional<Battery> unpack_battery(const Frame & f)
{
    Battery m{};
    if (!::rt_battery_unpack(&f, &m)) return std::nullopt;
    return m;
}

// CELLS_A — id 0x202, dlc 8, émetteur SAFETY [planned]
using CellsA = ::rt_cells_a_t;
inline constexpr std::uint16_t kCellsAId = RT_ID_CELLS_A;
inline constexpr std::uint8_t  kCellsADlc = RT_DLC_CELLS_A;
inline Frame pack(const CellsA & m)
{
    Frame f{};
    ::rt_cells_a_pack(&m, &f);
    return f;
}
inline std::optional<CellsA> unpack_cells_a(const Frame & f)
{
    CellsA m{};
    if (!::rt_cells_a_unpack(&f, &m)) return std::nullopt;
    return m;
}

// CELLS_B — id 0x203, dlc 8, émetteur SAFETY [planned]
using CellsB = ::rt_cells_b_t;
inline constexpr std::uint16_t kCellsBId = RT_ID_CELLS_B;
inline constexpr std::uint8_t  kCellsBDlc = RT_DLC_CELLS_B;
inline Frame pack(const CellsB & m)
{
    Frame f{};
    ::rt_cells_b_pack(&m, &f);
    return f;
}
inline std::optional<CellsB> unpack_cells_b(const Frame & f)
{
    CellsB m{};
    if (!::rt_cells_b_unpack(&f, &m)) return std::nullopt;
    return m;
}

// CELLS_C — id 0x204, dlc 8, émetteur SAFETY [planned]
using CellsC = ::rt_cells_c_t;
inline constexpr std::uint16_t kCellsCId = RT_ID_CELLS_C;
inline constexpr std::uint8_t  kCellsCDlc = RT_DLC_CELLS_C;
inline Frame pack(const CellsC & m)
{
    Frame f{};
    ::rt_cells_c_pack(&m, &f);
    return f;
}
inline std::optional<CellsC> unpack_cells_c(const Frame & f)
{
    CellsC m{};
    if (!::rt_cells_c_unpack(&f, &m)) return std::nullopt;
    return m;
}

// IMU_QUAT — id 0x210, dlc 8, émetteur SAFETY [bench]
using ImuQuat = ::rt_imu_quat_t;
inline constexpr std::uint16_t kImuQuatId = RT_ID_IMU_QUAT;
inline constexpr std::uint8_t  kImuQuatDlc = RT_DLC_IMU_QUAT;
inline Frame pack(const ImuQuat & m)
{
    Frame f{};
    ::rt_imu_quat_pack(&m, &f);
    return f;
}
inline std::optional<ImuQuat> unpack_imu_quat(const Frame & f)
{
    ImuQuat m{};
    if (!::rt_imu_quat_unpack(&f, &m)) return std::nullopt;
    return m;
}

// IMU_GYRO — id 0x211, dlc 8, émetteur SAFETY [bench]
using ImuGyro = ::rt_imu_gyro_t;
inline constexpr std::uint16_t kImuGyroId = RT_ID_IMU_GYRO;
inline constexpr std::uint8_t  kImuGyroDlc = RT_DLC_IMU_GYRO;
inline Frame pack(const ImuGyro & m)
{
    Frame f{};
    ::rt_imu_gyro_pack(&m, &f);
    return f;
}
inline std::optional<ImuGyro> unpack_imu_gyro(const Frame & f)
{
    ImuGyro m{};
    if (!::rt_imu_gyro_unpack(&f, &m)) return std::nullopt;
    return m;
}

// IMU_ACCEL — id 0x212, dlc 8, émetteur SAFETY [bench]
using ImuAccel = ::rt_imu_accel_t;
inline constexpr std::uint16_t kImuAccelId = RT_ID_IMU_ACCEL;
inline constexpr std::uint8_t  kImuAccelDlc = RT_DLC_IMU_ACCEL;
inline Frame pack(const ImuAccel & m)
{
    Frame f{};
    ::rt_imu_accel_pack(&m, &f);
    return f;
}
inline std::optional<ImuAccel> unpack_imu_accel(const Frame & f)
{
    ImuAccel m{};
    if (!::rt_imu_accel_unpack(&f, &m)) return std::nullopt;
    return m;
}

// IMU_MAG — id 0x214, dlc 8, émetteur SAFETY [bench]
using ImuMag = ::rt_imu_mag_t;
inline constexpr std::uint16_t kImuMagId = RT_ID_IMU_MAG;
inline constexpr std::uint8_t  kImuMagDlc = RT_DLC_IMU_MAG;
inline Frame pack(const ImuMag & m)
{
    Frame f{};
    ::rt_imu_mag_pack(&m, &f);
    return f;
}
inline std::optional<ImuMag> unpack_imu_mag(const Frame & f)
{
    ImuMag m{};
    if (!::rt_imu_mag_unpack(&f, &m)) return std::nullopt;
    return m;
}

// IMU_STATUS — id 0x213, dlc 8, émetteur SAFETY [bench]
using ImuStatus = ::rt_imu_status_t;
inline constexpr std::uint16_t kImuStatusId = RT_ID_IMU_STATUS;
inline constexpr std::uint8_t  kImuStatusDlc = RT_DLC_IMU_STATUS;
inline Frame pack(const ImuStatus & m)
{
    Frame f{};
    ::rt_imu_status_pack(&m, &f);
    return f;
}
inline std::optional<ImuStatus> unpack_imu_status(const Frame & f)
{
    ImuStatus m{};
    if (!::rt_imu_status_unpack(&f, &m)) return std::nullopt;
    return m;
}

// THERMAL — id 0x220, dlc 8, émetteur SAFETY [planned]
using Thermal = ::rt_thermal_t;
inline constexpr std::uint16_t kThermalId = RT_ID_THERMAL;
inline constexpr std::uint8_t  kThermalDlc = RT_DLC_THERMAL;
inline Frame pack(const Thermal & m)
{
    Frame f{};
    ::rt_thermal_pack(&m, &f);
    return f;
}
inline std::optional<Thermal> unpack_thermal(const Frame & f)
{
    Thermal m{};
    if (!::rt_thermal_unpack(&f, &m)) return std::nullopt;
    return m;
}

// TIME_SYNC — id 0x300, dlc 8, émetteur HOST [bench]
using TimeSync = ::rt_time_sync_t;
inline constexpr std::uint16_t kTimeSyncId = RT_ID_TIME_SYNC;
inline constexpr std::uint8_t  kTimeSyncDlc = RT_DLC_TIME_SYNC;
inline Frame pack(const TimeSync & m)
{
    Frame f{};
    ::rt_time_sync_pack(&m, &f);
    return f;
}
inline std::optional<TimeSync> unpack_time_sync(const Frame & f)
{
    TimeSync m{};
    if (!::rt_time_sync_unpack(&f, &m)) return std::nullopt;
    return m;
}

// ARM_REQUEST — id 0x310, dlc 2, émetteur HOST [planned]
using ArmRequest = ::rt_arm_request_t;
inline constexpr std::uint16_t kArmRequestId = RT_ID_ARM_REQUEST;
inline constexpr std::uint8_t  kArmRequestDlc = RT_DLC_ARM_REQUEST;
inline Frame pack(const ArmRequest & m)
{
    Frame f{};
    ::rt_arm_request_pack(&m, &f);
    return f;
}
inline std::optional<ArmRequest> unpack_arm_request(const Frame & f)
{
    ArmRequest m{};
    if (!::rt_arm_request_unpack(&f, &m)) return std::nullopt;
    return m;
}

// CONFIG — id 0x320, dlc 8, émetteur HOST [planned]
using Config = ::rt_config_t;
inline constexpr std::uint16_t kConfigId = RT_ID_CONFIG;
inline constexpr std::uint8_t  kConfigDlc = RT_DLC_CONFIG;
inline Frame pack(const Config & m)
{
    Frame f{};
    ::rt_config_pack(&m, &f);
    return f;
}
inline std::optional<Config> unpack_config(const Frame & f)
{
    Config m{};
    if (!::rt_config_unpack(&f, &m)) return std::nullopt;
    return m;
}

// LINK_PING — id 0x330, dlc 7, émetteur HOST [bench]
using LinkPing = ::rt_link_ping_t;
inline constexpr std::uint16_t kLinkPingId = RT_ID_LINK_PING;
inline constexpr std::uint8_t  kLinkPingDlc = RT_DLC_LINK_PING;
inline Frame pack(const LinkPing & m)
{
    Frame f{};
    ::rt_link_ping_pack(&m, &f);
    return f;
}
inline std::optional<LinkPing> unpack_link_ping(const Frame & f)
{
    LinkPing m{};
    if (!::rt_link_ping_unpack(&f, &m)) return std::nullopt;
    return m;
}

// LINK_PONG — id 0x331, dlc 7, émetteur SAFETY [bench]
using LinkPong = ::rt_link_pong_t;
inline constexpr std::uint16_t kLinkPongId = RT_ID_LINK_PONG;
inline constexpr std::uint8_t  kLinkPongDlc = RT_DLC_LINK_PONG;
inline Frame pack(const LinkPong & m)
{
    Frame f{};
    ::rt_link_pong_pack(&m, &f);
    return f;
}
inline std::optional<LinkPong> unpack_link_pong(const Frame & f)
{
    LinkPong m{};
    if (!::rt_link_pong_unpack(&f, &m)) return std::nullopt;
    return m;
}

// LOG — id 0x7F0, dlc 8, émetteur SAFETY [bench]
using Log = ::rt_log_t;
inline constexpr std::uint16_t kLogId = RT_ID_LOG;
inline constexpr std::uint8_t  kLogDlc = RT_DLC_LOG;
inline Frame pack(const Log & m)
{
    Frame f{};
    ::rt_log_pack(&m, &f);
    return f;
}
inline std::optional<Log> unpack_log(const Frame & f)
{
    Log m{};
    if (!::rt_log_unpack(&f, &m)) return std::nullopt;
    return m;
}

// HEARTBEAT_SAFETY — id 0x701, dlc 8, émetteur SAFETY [bench]
using HeartbeatSafety = ::rt_heartbeat_safety_t;
inline constexpr std::uint16_t kHeartbeatSafetyId = RT_ID_HEARTBEAT_SAFETY;
inline constexpr std::uint8_t  kHeartbeatSafetyDlc = RT_DLC_HEARTBEAT_SAFETY;
inline Frame pack(const HeartbeatSafety & m)
{
    Frame f{};
    ::rt_heartbeat_safety_pack(&m, &f);
    return f;
}
inline std::optional<HeartbeatSafety> unpack_heartbeat_safety(const Frame & f)
{
    HeartbeatSafety m{};
    if (!::rt_heartbeat_safety_unpack(&f, &m)) return std::nullopt;
    return m;
}

// HEARTBEAT_MOTION_FRONT — id 0x702, dlc 8, émetteur MOTION_FRONT [planned]
using HeartbeatMotionFront = ::rt_heartbeat_motion_front_t;
inline constexpr std::uint16_t kHeartbeatMotionFrontId = RT_ID_HEARTBEAT_MOTION_FRONT;
inline constexpr std::uint8_t  kHeartbeatMotionFrontDlc = RT_DLC_HEARTBEAT_MOTION_FRONT;
inline Frame pack(const HeartbeatMotionFront & m)
{
    Frame f{};
    ::rt_heartbeat_motion_front_pack(&m, &f);
    return f;
}
inline std::optional<HeartbeatMotionFront> unpack_heartbeat_motion_front(const Frame & f)
{
    HeartbeatMotionFront m{};
    if (!::rt_heartbeat_motion_front_unpack(&f, &m)) return std::nullopt;
    return m;
}

// HEARTBEAT_MOTION_REAR — id 0x703, dlc 8, émetteur MOTION_REAR [planned]
using HeartbeatMotionRear = ::rt_heartbeat_motion_rear_t;
inline constexpr std::uint16_t kHeartbeatMotionRearId = RT_ID_HEARTBEAT_MOTION_REAR;
inline constexpr std::uint8_t  kHeartbeatMotionRearDlc = RT_DLC_HEARTBEAT_MOTION_REAR;
inline Frame pack(const HeartbeatMotionRear & m)
{
    Frame f{};
    ::rt_heartbeat_motion_rear_pack(&m, &f);
    return f;
}
inline std::optional<HeartbeatMotionRear> unpack_heartbeat_motion_rear(const Frame & f)
{
    HeartbeatMotionRear m{};
    if (!::rt_heartbeat_motion_rear_unpack(&f, &m)) return std::nullopt;
    return m;
}

}  // namespace retriever::protocol
