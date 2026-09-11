// ===========================================================================
//  test_imu_conversion.cpp — ce que /imu/data doit garantir
//
//      colcon test --packages-select retriever_link
//
//  Chaque test correspond à une façon documentée de casser une localisation.
//  Ce ne sont pas des tests de régression écrits après coup : ce sont les
//  contrats que le reste de la pile a le droit de supposer.
//
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#include <gtest/gtest.h>

#include <cmath>

#include "retriever_link/imu_conversion.hpp"

namespace
{

using retriever::link::ImuNoiseModel;
using retriever::link::ImuSample;
using retriever::link::to_imu_message;
using retriever::link::to_magnetic_field_message;

builtin_interfaces::msg::Time stamp_of(std::int32_t sec, std::uint32_t nsec)
{
  builtin_interfaces::msg::Time t;
  t.sec = sec;
  t.nanosec = nsec;
  return t;
}

ImuSample make_sample()
{
  ImuSample s;
  // Rotation de 90° autour de z, telle qu'elle sortirait du BNO085.
  s.quat.w = 0.70710678F;
  s.quat.x = 0.0F;
  s.quat.y = 0.0F;
  s.quat.z = 0.70710678F;
  s.has_quat = true;

  s.gyro.gx = 0.01F;
  s.gyro.gy = -0.02F;
  s.gyro.gz = 0.5F;
  s.has_gyro = true;

  // Au repos, à plat : toute la gravité sur z, vers le HAUT (REP-145).
  s.accel.ax = 0.0F;
  s.accel.ay = 0.0F;
  s.accel.az = 9.81F;
  s.has_accel = true;

  s.quat_accuracy_rad = 0.05F;
  s.status_rot = 3;
  return s;
}

}  // namespace

// ---------------------------------------------------------------------------
//  Quaternion
// ---------------------------------------------------------------------------

TEST(ImuConversion, QuaternionEstNormalise)
{
  ImuSample s = make_sample();
  // Une norme volontairement fausse, comme en produit la quantification Q14.
  s.quat.w = 0.7F;
  s.quat.x = 0.7F;
  s.quat.y = 0.0F;
  s.quat.z = 0.0F;

  const auto r = to_imu_message(s, ImuNoiseModel{}, "imu_link", stamp_of(1, 0));

  const double n = std::sqrt(
    r.msg.orientation.w * r.msg.orientation.w + r.msg.orientation.x * r.msg.orientation.x +
    r.msg.orientation.y * r.msg.orientation.y + r.msg.orientation.z * r.msg.orientation.z);
  EXPECT_NEAR(n, 1.0, 1e-9);
  EXPECT_TRUE(r.orientation_usable);
  // La norme d'entrée est rapportée telle quelle : c'est elle qui sert de
  // détecteur de corruption côté diagnostics.
  EXPECT_NEAR(r.quaternion_norm, std::sqrt(0.98), 1e-6);
}

TEST(ImuConversion, QuaternionNulEstRefuse)
{
  ImuSample s = make_sample();
  s.quat.w = 0.0F;
  s.quat.x = 0.0F;
  s.quat.y = 0.0F;
  s.quat.z = 0.0F;

  const auto r = to_imu_message(s, ImuNoiseModel{}, "imu_link", stamp_of(1, 0));
  EXPECT_FALSE(r.orientation_usable);
  // REP-145 : premier élément à -1 = orientation non fournie.
  EXPECT_DOUBLE_EQ(r.msg.orientation_covariance[0], -1.0);
}

// ---------------------------------------------------------------------------
//  Unités et repères
// ---------------------------------------------------------------------------

TEST(ImuConversion, UnitesEtChampsRecopies)
{
  const ImuSample s = make_sample();
  const auto r = to_imu_message(s, ImuNoiseModel{}, "imu_link", stamp_of(42, 500));

  EXPECT_EQ(r.msg.header.frame_id, "imu_link");
  EXPECT_EQ(r.msg.header.stamp.sec, 42);
  EXPECT_EQ(r.msg.header.stamp.nanosec, 500U);

  EXPECT_NEAR(r.msg.angular_velocity.x, 0.01, 1e-6);
  EXPECT_NEAR(r.msg.angular_velocity.y, -0.02, 1e-6);
  EXPECT_NEAR(r.msg.angular_velocity.z, 0.5, 1e-6);

  // ⚠️ Gravité COMPRISE. Si ce test tombe parce que az vaut 0 au repos, c'est
  // que le firmware a activé SH2_LINEAR_ACCELERATION au lieu de
  // SH2_ACCELEROMETER — une erreur qui ne se voit nulle part ailleurs avant que
  // l'EKF ne se mette à dériver.
  EXPECT_NEAR(r.msg.linear_acceleration.z, 9.81, 1e-6);
}

TEST(MagneticField, MicroteslasVersTeslas)
{
  const auto msg =
    to_magnetic_field_message(48.0, -12.0, 6.5, ImuNoiseModel{}, "imu_link", stamp_of(1, 0));

  // 48 µT, un champ terrestre plausible, doit sortir en 4,8e-5 T.
  EXPECT_NEAR(msg.magnetic_field.x, 48.0e-6, 1e-12);
  EXPECT_NEAR(msg.magnetic_field.y, -12.0e-6, 1e-12);
  EXPECT_NEAR(msg.magnetic_field.z, 6.5e-6, 1e-12);
  EXPECT_GT(msg.magnetic_field_covariance[0], 0.0);
}

// ---------------------------------------------------------------------------
//  Covariances — la partie qui fait diverger les EKF
// ---------------------------------------------------------------------------

TEST(Covariance, JamaisNulleQuandLaDonneeExiste)
{
  const ImuSample s = make_sample();
  const auto r = to_imu_message(s, ImuNoiseModel{}, "imu_link", stamp_of(1, 0));

  // Une covariance à zéro se lit « confiance infinie », pas « inconnue ».
  EXPECT_GT(r.msg.orientation_covariance[0], 0.0);
  EXPECT_GT(r.msg.orientation_covariance[4], 0.0);
  EXPECT_GT(r.msg.orientation_covariance[8], 0.0);
  EXPECT_GT(r.msg.angular_velocity_covariance[0], 0.0);
  EXPECT_GT(r.msg.linear_acceleration_covariance[0], 0.0);
}

TEST(Covariance, LePlancherSurLeLacetEstRespecte)
{
  ImuSample s = make_sample();
  // Le capteur s'annonce très précis : 0,001 rad. On ne le croit pas en
  // dessous du plancher, parce que la datasheet elle-même annonce 2° statiques.
  s.quat_accuracy_rad = 0.001F;

  ImuNoiseModel noise;
  noise.orientation_stddev_yaw_min = 0.0873;

  const auto r = to_imu_message(s, noise, "imu_link", stamp_of(1, 0));
  EXPECT_NEAR(r.msg.orientation_covariance[8], 0.0873 * 0.0873, 1e-9);
}

TEST(Covariance, LeCapteurPessimisteEstCru)
{
  ImuSample s = make_sample();
  s.quat_accuracy_rad = 0.3F;   // le capteur se sait perturbé

  const auto r = to_imu_message(s, ImuNoiseModel{}, "imu_link", stamp_of(1, 0));
  EXPECT_NEAR(r.msg.orientation_covariance[8], 0.3 * 0.3, 1e-6);
}

TEST(Covariance, SansEstimationLeLacetEstDeclareInconnu)
{
  ImuSample s = make_sample();
  // Cas du game rotation vector : aucune référence de cap, donc aucune
  // estimation d'erreur. Le lacet doit devenir inutilisable pour l'EKF, pas
  // paraître excellent.
  s.quat_accuracy_rad = 0.0F;

  ImuNoiseModel noise;
  noise.orientation_stddev_yaw_unreported = 1.0;

  const auto r = to_imu_message(s, noise, "imu_link", stamp_of(1, 0));
  EXPECT_NEAR(r.msg.orientation_covariance[8], 1.0, 1e-9);
  // Roulis et tangage restent bons : ils viennent de la gravité, pas du champ
  // magnétique.
  EXPECT_LT(r.msg.orientation_covariance[0], 0.01);
}

TEST(Covariance, LesMatricesSontDiagonales)
{
  const ImuSample s = make_sample();
  const auto r = to_imu_message(s, ImuNoiseModel{}, "imu_link", stamp_of(1, 0));

  for (std::size_t i = 0; i < 9; ++i) {
    if (i == 0 || i == 4 || i == 8) {
      continue;
    }
    EXPECT_DOUBLE_EQ(r.msg.orientation_covariance[i], 0.0) << "hors diagonale " << i;
    EXPECT_DOUBLE_EQ(r.msg.angular_velocity_covariance[i], 0.0) << "hors diagonale " << i;
    EXPECT_DOUBLE_EQ(r.msg.linear_acceleration_covariance[i], 0.0) << "hors diagonale " << i;
  }
}

TEST(Covariance, DonneeAbsenteMarqueeSelonRep145)
{
  ImuSample s = make_sample();
  s.has_gyro = false;
  s.has_accel = false;

  const auto r = to_imu_message(s, ImuNoiseModel{}, "imu_link", stamp_of(1, 0));
  EXPECT_DOUBLE_EQ(r.msg.angular_velocity_covariance[0], -1.0);
  EXPECT_DOUBLE_EQ(r.msg.linear_acceleration_covariance[0], -1.0);
  // L'orientation, elle, est toujours là.
  EXPECT_GT(r.msg.orientation_covariance[0], 0.0);
}
