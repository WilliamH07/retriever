// ===========================================================================
//  imu_conversion.cpp — voir imu_conversion.hpp
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#include "retriever_link/imu_conversion.hpp"

#include <array>
#include <cmath>

namespace retriever::link
{
namespace
{

/// Remplit une covariance 3×3 diagonale, rangée par lignes.
void fill_diagonal(std::array<double, 9> & cov, double vx, double vy, double vz)
{
  cov.fill(0.0);
  cov[0] = vx;
  cov[4] = vy;
  cov[8] = vz;
}

constexpr double square(double v) { return v * v; }

}  // namespace

ConversionResult to_imu_message(
  const ImuSample & sample, const ImuNoiseModel & noise, const std::string & frame_id,
  const builtin_interfaces::msg::Time & stamp)
{
  ConversionResult result;
  auto & msg = result.msg;

  msg.header.stamp = stamp;
  msg.header.frame_id = frame_id;

  // --- Orientation -------------------------------------------------------
  if (sample.has_quat) {
    const double w = sample.quat.w;
    const double x = sample.quat.x;
    const double y = sample.quat.y;
    const double z = sample.quat.z;
    const double norm = std::sqrt(square(w) + square(x) + square(y) + square(z));
    result.quaternion_norm = norm;

    if (norm > 1.0e-6) {
      msg.orientation.w = w / norm;
      msg.orientation.x = x / norm;
      msg.orientation.y = y / norm;
      msg.orientation.z = z / norm;
      result.orientation_usable = true;
    }
  }

  std::array<double, 9> orientation_cov{};
  if (result.orientation_usable) {
    // Lacet : on prend l'estimation du capteur si elle existe, jamais en
    // dessous du plancher. Si elle n'existe pas — game rotation vector —, on
    // déclare le lacet inconnu plutôt que de laisser croire qu'il est bon.
    double yaw_stddev = noise.orientation_stddev_yaw_unreported;
    if (noise.use_reported_accuracy && sample.quat_accuracy_rad > 0.0F) {
      yaw_stddev = std::max(
        static_cast<double>(sample.quat_accuracy_rad), noise.orientation_stddev_yaw_min);
    } else if (!noise.use_reported_accuracy) {
      yaw_stddev = noise.orientation_stddev_yaw_min;
    }

    // ⚠️ 📐 Hypothèse assumée : la covariance est diagonale dans les axes du
    // capteur, et l'incertitude de cap est portée par l'axe z du capteur. Ce
    // n'est exact que si le capteur est à peu près horizontal, puisque
    // l'incertitude de cap est en réalité autour de la verticale du lieu. Sur
    // un robot de terrain plat, l'écart est négligeable ; sur une pente forte,
    // il ne l'est plus. Le jour où ça compte, la correction consiste à tourner
    // diag(σ_rp², σ_rp², σ_lacet²) du monde vers le capteur — et pas à
    // bricoler les écarts types.
    fill_diagonal(
      orientation_cov, square(noise.orientation_stddev_rp),
      square(noise.orientation_stddev_rp), square(yaw_stddev));
  } else {
    // REP-145 : le premier élément à -1 signifie « orientation non fournie ».
    // C'est la seule manière normalisée de le dire ; laisser des zéros
    // signifierait « parfaitement connue ».
    orientation_cov.fill(0.0);
    orientation_cov[0] = -1.0;
  }
  std::copy(orientation_cov.begin(), orientation_cov.end(), msg.orientation_covariance.begin());

  // --- Vitesse angulaire -------------------------------------------------
  std::array<double, 9> gyro_cov{};
  if (sample.has_gyro) {
    msg.angular_velocity.x = sample.gyro.gx;
    msg.angular_velocity.y = sample.gyro.gy;
    msg.angular_velocity.z = sample.gyro.gz;
    const double v = square(noise.angular_velocity_stddev);
    fill_diagonal(gyro_cov, v, v, v);
  } else {
    gyro_cov.fill(0.0);
    gyro_cov[0] = -1.0;
  }
  std::copy(gyro_cov.begin(), gyro_cov.end(), msg.angular_velocity_covariance.begin());

  // --- Accélération ------------------------------------------------------
  std::array<double, 9> accel_cov{};
  if (sample.has_accel) {
    // Gravité comprise : c'est SH2_ACCELEROMETER et non
    // SH2_LINEAR_ACCELERATION qui est activé côté firmware, conformément à
    // REP-145 (+g au repos, axe z vers le haut).
    msg.linear_acceleration.x = sample.accel.ax;
    msg.linear_acceleration.y = sample.accel.ay;
    msg.linear_acceleration.z = sample.accel.az;
    const double v = square(noise.linear_acceleration_stddev);
    fill_diagonal(accel_cov, v, v, v);
  } else {
    accel_cov.fill(0.0);
    accel_cov[0] = -1.0;
  }
  std::copy(accel_cov.begin(), accel_cov.end(), msg.linear_acceleration_covariance.begin());

  return result;
}

sensor_msgs::msg::MagneticField to_magnetic_field_message(
  double x_ut, double y_ut, double z_ut, const ImuNoiseModel & noise,
  const std::string & frame_id, const builtin_interfaces::msg::Time & stamp)
{
  sensor_msgs::msg::MagneticField msg;
  msg.header.stamp = stamp;
  msg.header.frame_id = frame_id;

  // Le BNO085 donne des microteslas, sensor_msgs/MagneticField attend des
  // teslas. L'oubli de ce facteur donne un champ un million de fois trop fort,
  // ce qui passe inaperçu tant qu'on ne regarde que la direction.
  constexpr double kMicroTeslaToTesla = 1.0e-6;
  msg.magnetic_field.x = x_ut * kMicroTeslaToTesla;
  msg.magnetic_field.y = y_ut * kMicroTeslaToTesla;
  msg.magnetic_field.z = z_ut * kMicroTeslaToTesla;

  std::array<double, 9> cov{};
  const double v = square(noise.magnetic_field_stddev);
  fill_diagonal(cov, v, v, v);
  std::copy(cov.begin(), cov.end(), msg.magnetic_field_covariance.begin());

  return msg;
}

}  // namespace retriever::link
