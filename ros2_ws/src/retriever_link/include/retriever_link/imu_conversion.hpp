// ===========================================================================
//  imu_conversion.hpp — trames → sensor_msgs/Imu, proprement
//
//  Tout ce qui touche aux unités, aux repères et aux covariances est ici, dans
//  une unité de compilation sans ROS ni matériel, et testée à part. C'est
//  volontaire : ce sont les conversions silencieuses qui font les EKF qui
//  divergent, et elles se relisent mieux seules qu'au milieu d'un nœud.
//
//  CE QUI EST GARANTI EN SORTIE
//
//  1. Unités conformes à REP-145 ✅
//       orientation          quaternion unitaire, repère capteur dans le monde
//       angular_velocity     rad/s, repère capteur
//       linear_acceleration  m/s², GRAVITÉ COMPRISE
//       magnetic_field       tesla (le capteur donne des µT : facteur 1e-6)
//
//  2. Repères conformes à REP-103 ✅
//       Le repère monde du BNO085 est référencé au nord magnétique et à la
//       gravité, soit ENU — exactement la convention monde de ROS. Aucune
//       conversion de repère monde n'est appliquée, parce qu'il n'y en a pas à
//       faire, et en faire une serait une erreur.
//       Le repère capteur est celui du boîtier (repère Android, direct). Le
//       passage vers le robot est décrit dans l'URDF, PAS ici.
//
//  3. Quaternion normalisé. Le transport quantifie en Q14, donc |q| s'écarte
//     de 1 de quelques 1e-4. Un quaternion non normalisé est refusé par
//     certains consommateurs et dégrade les autres en silence.
//
//  4. Covariances renseignées, jamais nulles. ⚠️ Une matrice de covariance à
//     zéro n'est pas « pas d'information » : pour un EKF, c'est « confiance
//     infinie ». C'est la façon la plus efficace de faire diverger une
//     localisation, et elle ne produit aucun message d'erreur.
//
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#pragma once

#include <cstdint>
#include <string>

#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/magnetic_field.hpp"

#include "retriever_protocol/protocol.hpp"

namespace retriever::link
{

/// Écarts types utilisés pour construire les covariances. Tous en unités SI.
struct ImuNoiseModel
{
  /// Roulis et tangage. Ceux-là sont observés par la gravité, donc bons et
  /// stables. Défaut : 3,5° — l'erreur dynamique annoncée par la datasheet ✅.
  double orientation_stddev_rp = 0.0611;

  /// Plancher sur le lacet, même quand le capteur s'annonce meilleur. Défaut :
  /// 5° — la valeur « en pratique » retenue au §I.2 du dossier ✅, plus
  /// prudente que les 2° statiques de la datasheet.
  double orientation_stddev_yaw_min = 0.0873;

  /// Lacet quand le capteur ne fournit PAS d'estimation — cas du game rotation
  /// vector, qui n'a aucune référence de cap. Défaut : 1 rad, c'est-à-dire
  /// « inconnu », pour que l'EKF cesse d'écouter ce lacet plutôt que de le
  /// croire. 📐
  double orientation_stddev_yaw_unreported = 1.0;

  /// 📐 À REMPLACER par la mesure du banc : 60 s à l'arrêt, écart type de
  /// chaque axe. La procédure est dans la recette B1. La valeur par défaut est
  /// un ordre de grandeur, pas une spécification.
  double angular_velocity_stddev = 0.01;

  /// 📐 Idem.
  double linear_acceleration_stddev = 0.1;

  /// 📐 Idem, en tesla.
  double magnetic_field_stddev = 2.0e-6;

  /// Utiliser l'estimation d'erreur renvoyée par le capteur quand elle existe.
  bool use_reported_accuracy = true;
};

/// Un échantillon complet, tel que reconstitué depuis les trames.
struct ImuSample
{
  protocol::ImuQuat quat{};
  protocol::ImuGyro gyro{};
  protocol::ImuAccel accel{};
  float quat_accuracy_rad = 0.0F;
  std::uint8_t status_rot = 0;
  bool has_quat = false;
  bool has_gyro = false;
  bool has_accel = false;
};

struct ConversionResult
{
  sensor_msgs::msg::Imu msg;
  /// Norme du quaternion AVANT normalisation. S'en écarter de plus de 1e-2
  /// signale une corruption sur le lien ou une erreur d'échelle.
  double quaternion_norm = 0.0;
  bool orientation_usable = false;
};

/// Construit le message. `stamp` est déjà corrigé de la latence de liaison.
ConversionResult to_imu_message(
  const ImuSample & sample, const ImuNoiseModel & noise, const std::string & frame_id,
  const builtin_interfaces::msg::Time & stamp);

/// Champ magnétique : µT → T, et covariance diagonale.
sensor_msgs::msg::MagneticField to_magnetic_field_message(
  double x_ut, double y_ut, double z_ut, const ImuNoiseModel & noise,
  const std::string & frame_id, const builtin_interfaces::msg::Time & stamp);

}  // namespace retriever::link
