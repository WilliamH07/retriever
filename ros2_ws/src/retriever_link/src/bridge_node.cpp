// ===========================================================================
//  bridge_node.cpp — le pont entre la liaison et le graphe ROS 2
//
//  Un seul nœud, deux responsabilités qui ne doivent pas se mélanger :
//
//    1. lire des trames et en faire des messages ROS PROPRES — unités SI,
//       repères REP-103, covariances renseignées, horodatage cohérent ;
//    2. dire si la liaison va bien, et le dire assez fort pour qu'on le voie
//       avant que ça n'ait des conséquences.
//
//  Ce que ce nœud ne fait PAS, volontairement : il ne filtre pas, ne fusionne
//  pas, ne corrige pas. Un pont qui « améliore » les données est un pont dans
//  lequel on ne peut plus avoir confiance quand quelque chose cloche. Le
//  filtrage est le travail de robot_localization, en aval, et il est fait une
//  fois pour toutes les sources.
//
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "diagnostic_updater/diagnostic_status_wrapper.hpp"
#include "diagnostic_updater/diagnostic_updater.hpp"
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/magnetic_field.hpp"
#include "tf2_ros/transform_broadcaster.hpp"
#include "visualization_msgs/msg/marker.hpp"

#include "retriever_link/imu_conversion.hpp"
#include "retriever_link/serial_transport.hpp"
#include "retriever_link/socketcan_transport.hpp"
#include "retriever_msgs/msg/imu_status.hpp"
#include "retriever_msgs/msg/link_status.hpp"
#include "retriever_msgs/msg/node_status.hpp"

namespace retriever::link
{

using namespace std::chrono_literals;

class BridgeNode : public rclcpp::Node
{
public:
  BridgeNode()
  : rclcpp::Node("retriever_link_bridge"), diagnostics_(this)
  {
    declare_parameters();
    build_transport();
    build_publishers();
    build_diagnostics();

    running_ = true;
    reader_ = std::thread(&BridgeNode::reader_loop, this);

    if (time_sync_enabled_) {
      time_sync_timer_ = create_wall_timer(1s, [this] { send_time_sync(); });
    }
    if (ping_period_s_ > 0.0) {
      ping_timer_ = create_wall_timer(
        std::chrono::duration<double>(ping_period_s_), [this] { send_ping(); });
    }
    status_timer_ = create_wall_timer(500ms, [this] { publish_status(); });

    RCLCPP_INFO(
      get_logger(), "pont pret sur %s, protocole %s (0x%08X)",
      transport_->describe().c_str(), std::string(protocol::kVersion).c_str(),
      protocol::kHash);
  }

  ~BridgeNode() override
  {
    running_ = false;
    if (reader_.joinable()) {
      reader_.join();
    }
    if (transport_) {
      transport_->close();
    }
  }

private:
  // -----------------------------------------------------------------------
  //  Paramètres
  // -----------------------------------------------------------------------
  void declare_parameters()
  {
    transport_kind_ = declare_parameter<std::string>("transport", "serial");
    serial_device_ = declare_parameter<std::string>("serial.device", "/dev/ttyUSB0");
    serial_baud_ = declare_parameter<int>("serial.baudrate", 921600);
    // Voir SerialTransport : sur une DevKitC, ouvrir le port peut faire
    // démarrer la carte en mode téléchargement, où elle est muette. Le nœud la
    // remet donc en mode exécution à l'ouverture. À passer à false sur un
    // montage où redémarrer le microcontrôleur au démarrage du nœud n'est pas
    // acceptable — sur CAN, la question ne se pose pas.
    serial_reset_on_open_ = declare_parameter<bool>("serial.reset_on_open", true);
    can_interface_ = declare_parameter<std::string>("can.interface", "can0");

    frame_id_ = declare_parameter<std::string>("frame_id", "imu_link");

    // ⚠️ Le protocole ne transporte pas d'horodatage : les huit octets d'une
    // trame sont pleins. On date donc à la réception, moins une latence
    // constante. Cette constante N'EST PAS une devinette : elle se mesure, avec
    // LINK_PING, et la recette B1 dit comment. La valeur par défaut est un
    // ordre de grandeur 📐 tant que la mesure n'est pas faite.
    latency_offset_ms_ = declare_parameter<double>("link.latency_offset_ms", 1.5);
    ping_period_s_ = declare_parameter<double>("link.ping_period_s", 1.0);
    time_sync_enabled_ = declare_parameter<bool>("link.time_sync", true);

    noise_.orientation_stddev_rp =
      declare_parameter<double>("imu.orientation_stddev_rp", noise_.orientation_stddev_rp);
    noise_.orientation_stddev_yaw_min = declare_parameter<double>(
      "imu.orientation_stddev_yaw_min", noise_.orientation_stddev_yaw_min);
    noise_.orientation_stddev_yaw_unreported = declare_parameter<double>(
      "imu.orientation_stddev_yaw_unreported", noise_.orientation_stddev_yaw_unreported);
    noise_.angular_velocity_stddev =
      declare_parameter<double>("imu.angular_velocity_stddev", noise_.angular_velocity_stddev);
    noise_.linear_acceleration_stddev = declare_parameter<double>(
      "imu.linear_acceleration_stddev", noise_.linear_acceleration_stddev);
    noise_.magnetic_field_stddev =
      declare_parameter<double>("imu.magnetic_field_stddev", noise_.magnetic_field_stddev);
    noise_.use_reported_accuracy =
      declare_parameter<bool>("imu.use_reported_accuracy", true);

    expected_rate_hz_ = declare_parameter<double>("imu.expected_rate_hz", 100.0);

    // Aides de banc. Elles ne servent qu'à voir quelque chose dans Foxglove
    // avant que le robot n'ait un arbre TF : sur le robot, c'est l'EKF qui
    // publie les transformations, et publier ceci EN PLUS serait une deuxième
    // source sur la même arête — la faute classique (§I.3).
    bench_tf_ = declare_parameter<bool>("bench.publish_tf", false);
    bench_parent_frame_ = declare_parameter<std::string>("bench.tf_parent", "imu_world");
    bench_marker_ = declare_parameter<bool>("bench.publish_marker", false);
  }

  void build_transport()
  {
    if (transport_kind_ == "socketcan") {
      transport_ = std::make_unique<SocketCanTransport>(can_interface_);
    } else {
      transport_ = std::make_unique<SerialTransport>(
        serial_device_, serial_baud_, serial_reset_on_open_);
    }
    transport_->open();
  }

  void build_publishers()
  {
    // QoS capteur : BEST_EFFORT, profondeur faible. À 100 Hz, retransmettre un
    // échantillon périmé n'a aucun intérêt, et sur un lien Wi-Fi c'est même
    // nuisible (§I.4).
    const auto sensor_qos = rclcpp::SensorDataQoS();
    imu_pub_ = create_publisher<sensor_msgs::msg::Imu>("imu/data", sensor_qos);
    mag_pub_ = create_publisher<sensor_msgs::msg::MagneticField>("imu/mag", sensor_qos);

    // TRANSIENT_LOCAL sur les états : un panneau Foxglove ouvert après coup
    // doit connaître l'état immédiatement, pas au prochain rafraîchissement.
    const auto latched = rclcpp::QoS(1).transient_local().reliable();
    imu_status_pub_ =
      create_publisher<retriever_msgs::msg::ImuStatus>("retriever/imu_status", latched);
    link_status_pub_ =
      create_publisher<retriever_msgs::msg::LinkStatus>("retriever/link_status", latched);
    node_status_pub_ =
      create_publisher<retriever_msgs::msg::NodeStatus>("retriever/node_status", latched);

    if (bench_tf_) {
      tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
      RCLCPP_WARN(
        get_logger(),
        "bench.publish_tf actif : %s -> %s est publie depuis l'IMU. "
        "A DESACTIVER sur le robot, ou l'EKF aura un concurrent sur cette arete.",
        bench_parent_frame_.c_str(), frame_id_.c_str());
    }
    if (bench_marker_) {
      marker_pub_ = create_publisher<visualization_msgs::msg::Marker>(
        "retriever/imu_marker", rclcpp::QoS(1).transient_local().reliable());
      publish_marker();
    }
  }

  void build_diagnostics()
  {
    diagnostics_.setHardwareID("retriever-safety");
    diagnostics_.add("Liaison", this, &BridgeNode::diagnose_link);
    diagnostics_.add("Capteur inertiel", this, &BridgeNode::diagnose_imu);
    diagnostics_.add("Noeud SAFETY", this, &BridgeNode::diagnose_node);
  }

  // -----------------------------------------------------------------------
  //  Boucle de lecture
  // -----------------------------------------------------------------------
  void reader_loop()
  {
    protocol::Frame frame{};
    while (running_ && rclcpp::ok()) {
      if (!transport_->receive(frame, 20ms)) {
        continue;
      }
      dispatch(frame);
    }
  }

  void dispatch(const protocol::Frame & f)
  {
    switch (f.id) {
      case protocol::kImuQuatId: on_quat(f); return;
      case protocol::kImuGyroId: on_gyro(f); return;
      case protocol::kImuAccelId: on_accel(f); return;
      case protocol::kImuMagId: on_mag(f); return;
      case protocol::kImuStatusId: on_imu_status(f); return;
      case protocol::kImuCalId: on_imu_cal(f); return;
      case protocol::kHeartbeatSafetyId: on_heartbeat(f); return;
      case protocol::kLinkPongId: on_pong(f); return;
      case protocol::kLogId: on_log(f); return;
      default: break;
    }
    // Un identifiant inconnu n'est pas une erreur de transmission : c'est un
    // firmware plus récent, ou plus ancien, que ce nœud. On le compte, on ne
    // s'en alarme pas ici — c'est le hash du protocole qui tranche.
    unknown_frames_++;
  }

  // -----------------------------------------------------------------------
  //  IMU
  // -----------------------------------------------------------------------
  rclcpp::Time stamp_now() const
  {
    return now() - rclcpp::Duration::from_seconds(latency_offset_ms_ * 1e-3);
  }

  void on_quat(const protocol::Frame & f)
  {
    const auto v = protocol::unpack_imu_quat(f);
    if (!v) {
      return;
    }
    std::lock_guard<std::mutex> lock(sample_mutex_);
    // Un nouveau quaternion ouvre un nouveau triplet. S'il en restait un
    // incomplet, c'est qu'une trame a été perdue : on le compte plutôt que de
    // mélanger deux échantillons.
    if (pending_.has_quat && !(pending_.has_gyro && pending_.has_accel)) {
      dropped_link_++;
    }
    pending_ = ImuSample{};
    pending_.quat = *v;
    pending_.has_quat = true;
    pending_.quat_accuracy_rad = last_accuracy_rad_.load();
    pending_.status_rot = last_status_rot_.load();
    pending_stamp_ = stamp_now();
  }

  void on_gyro(const protocol::Frame & f)
  {
    const auto v = protocol::unpack_imu_gyro(f);
    if (!v) {
      return;
    }
    std::lock_guard<std::mutex> lock(sample_mutex_);
    pending_.gyro = *v;
    pending_.has_gyro = true;
    pending_seq_ = v->seq;
  }

  void on_accel(const protocol::Frame & f)
  {
    const auto v = protocol::unpack_imu_accel(f);
    if (!v) {
      return;
    }
    ImuSample sample;
    rclcpp::Time stamp;
    {
      std::lock_guard<std::mutex> lock(sample_mutex_);
      pending_.accel = *v;
      pending_.has_accel = true;
      // L'ordre de ces deux tests compte : sans le gyromètre, `pending_seq_`
      // est celui du triplet PRÉCÉDENT, et comparer à lui n'aurait aucun sens.
      if (!pending_.has_quat || !pending_.has_gyro) {
        dropped_link_++;
        pending_ = ImuSample{};
        return;
      }
      // ⚠️ IMU_QUAT n'a PAS de compteur : ses huit octets sont pleins. Le
      // contrôle de `seq` ne rapproche donc que GYRO et ACCEL, et un
      // quaternion vieux d'un ou plusieurs cycles pourrait passer avec eux si
      // les QUAT suivants étaient perdus. On borne son âge.
      const double max_age = 0.5 / (expected_rate_hz_ > 1.0 ? expected_rate_hz_ : 1.0);
      if ((stamp_now() - pending_stamp_).seconds() > max_age) {
        dropped_link_++;
        pending_ = ImuSample{};
        return;
      }
      // Le firmware annonce lui-même ce qu'il a pu mesurer : s'il dit que le
      // quaternion n'était pas valide, on ne le publie pas.
      if ((v->flags & 0x01U) == 0U) {
        dropped_link_++;
        pending_ = ImuSample{};
        return;
      }
      if (v->seq != pending_seq_) {
        // Le compteur ne correspond pas : on est en train d'assembler deux
        // échantillons différents. Publier ce mélange serait pire que de ne
        // rien publier — un quaternion et une accélération qui ne sont pas du
        // même instant, c'est exactement ce qu'un EKF ne sait pas détecter.
        dropped_link_++;
        pending_ = ImuSample{};
        return;
      }
      sample = pending_;
      stamp = pending_stamp_;
      pending_ = ImuSample{};
    }

    const auto result = to_imu_message(sample, noise_, frame_id_, stamp);
    imu_pub_->publish(result.msg);

    last_norm_error_.store(std::abs(result.quaternion_norm - 1.0));
    note_imu_rate(stamp);

    if (bench_tf_ && result.orientation_usable) {
      publish_bench_tf(result.msg, stamp);
    }
  }

  void on_mag(const protocol::Frame & f)
  {
    const auto v = protocol::unpack_imu_mag(f);
    if (!v) {
      return;
    }
    last_status_mag_.store(static_cast<std::uint8_t>(v->flags & 0x03U));
    mag_pub_->publish(
      to_magnetic_field_message(v->mx, v->my, v->mz, noise_, frame_id_, stamp_now()));
  }

  void on_imu_cal(const protocol::Frame & f)
  {
    const auto v = protocol::unpack_imu_cal(f);
    if (!v) {
      return;
    }
    // ⚠️ C'est cette trame, et non IMU_MAG, qui fait autorité sur la qualité du
    // magnétomètre : elle arrive même quand le magnétomètre est désactivé, et
    // c'est précisément dans ce cas qu'on veut savoir pourquoi le cap est mauvais.
    last_status_mag_.store(v->status_mag);
    cal_enabled_.store(v->enabled);
    cal_saves_.store(v->saves);
    cal_autosave_.store((v->flags & 0x01U) != 0U);
    cal_last_result_.store(v->last_result);
  }

  void on_imu_status(const protocol::Frame & f)
  {
    const auto v = protocol::unpack_imu_status(f);
    if (!v) {
      return;
    }
    last_accuracy_rad_.store(v->quat_accuracy);
    last_status_rot_.store(v->status_rot);
    last_status_gyro_.store(v->status_gyro);
    last_status_accel_.store(v->status_accel);
    sensor_resets_.store(v->reset_count);
    dropped_node_.store(v->dropped);

    retriever_msgs::msg::ImuStatus msg;
    msg.header.stamp = now();
    msg.header.frame_id = frame_id_;
    msg.status_orientation = v->status_rot;
    msg.status_gyro = v->status_gyro;
    msg.status_accel = v->status_accel;
    msg.status_mag = last_status_mag_.load();
    msg.orientation_accuracy = v->quat_accuracy;
    msg.sensor_resets = v->reset_count;
    msg.samples_dropped_node = v->dropped;
    msg.samples_dropped_link = dropped_link_;
    msg.rate_hz = static_cast<float>(measured_rate_hz_.load());
    msg.quaternion_norm_error = static_cast<float>(last_norm_error_.load());
    msg.cal_enabled = cal_enabled_.load();
    msg.cal_saves = cal_saves_.load();
    msg.cal_autosave = cal_autosave_.load();
    msg.cal_last_result = cal_last_result_.load();
    imu_status_pub_->publish(msg);
  }

  void note_imu_rate(const rclcpp::Time & stamp)
  {
    if (last_imu_stamp_.nanoseconds() != 0) {
      const double dt = (stamp - last_imu_stamp_).seconds();
      if (dt > 1e-6) {
        // Moyenne glissante exponentielle : suffisante pour un diagnostic, et
        // elle ne coûte ni tampon ni allocation.
        const double inst = 1.0 / dt;
        const double previous = measured_rate_hz_.load();
        measured_rate_hz_.store(previous <= 0.0 ? inst : 0.98 * previous + 0.02 * inst);
      }
    }
    last_imu_stamp_ = stamp;
    last_imu_stamp_ns_.store(stamp.nanoseconds(), std::memory_order_relaxed);
  }

  // -----------------------------------------------------------------------
  //  Service
  // -----------------------------------------------------------------------
  void on_heartbeat(const protocol::Frame & f)
  {
    const auto v = protocol::unpack_heartbeat_safety(f);
    if (!v) {
      return;
    }
    const rclcpp::Time received = now();
    last_heartbeat_ns_.store(received.nanoseconds(), std::memory_order_relaxed);
    node_hash_.store(v->protocol_hash);

    if (v->protocol_hash != protocol::kHash && !hash_reported_) {
      hash_reported_ = true;
      // Bruyant et explicite : c'est le mode de défaillance qui fait perdre le
      // plus de temps, et il est parfaitement diagnosticable en une ligne.
      RCLCPP_ERROR(
        get_logger(),
        "DIVERGENCE DE PROTOCOLE — noeud 0x%08X, calculateur 0x%08X. "
        "Le firmware et ce paquet ne viennent pas du meme protocol.yaml. "
        "Reflasher le noeud, ou rebatir le workspace.",
        v->protocol_hash, protocol::kHash);
    }

    retriever_msgs::msg::NodeStatus msg;
    msg.header.stamp = received;
    msg.node_id = retriever_msgs::msg::NodeStatus::NODE_SAFETY;
    msg.state = v->state;
    msg.uptime_s = v->uptime_s;
    msg.error_count = v->err_count;
    msg.protocol_hash = v->protocol_hash;
    msg.heartbeat_age_s = 0.0F;
    {
      std::lock_guard<std::mutex> lock(node_status_mutex_);
      last_node_status_ = msg;
      have_node_status_ = true;
    }
    node_status_pub_->publish(msg);
  }

  void on_pong(const protocol::Frame & f)
  {
    const auto v = protocol::unpack_link_pong(f);
    if (!v) {
      return;
    }
    const auto elapsed_us = static_cast<std::uint32_t>(host_micros() - v->t_tx_us);
    const double ms = static_cast<double>(elapsed_us) * 1e-3;
    // Un aller-retour aberrant vient d'un débordement du compteur 32 bits
    // (toutes les 71 minutes) : on le jette plutôt que de polluer la mesure.
    if (ms >= 0.0 && ms < 1000.0) {
      round_trip_ms_.store(ms);
      round_trip_max_ms_.store(std::max(round_trip_max_ms_.load(), ms));
    }
  }

  void on_log(const protocol::Frame & f)
  {
    const auto v = protocol::unpack_log(f);
    if (!v) {
      return;
    }
    const std::uint8_t level = static_cast<std::uint8_t>((v->header >> 5) & 0x07U);
    const bool eol = (v->header & 0x10U) != 0U;
    const std::size_t len = std::min<std::size_t>(v->header & 0x0FU, 7U);

    const std::uint8_t chars[7] = {v->c0, v->c1, v->c2, v->c3, v->c4, v->c5, v->c6};
    for (std::size_t i = 0; i < len; ++i) {
      log_line_.push_back(static_cast<char>(chars[i]));
    }
    // Une ligne plus longue que ça vient d'un fragment de fin perdu : on la
    // sort plutôt que de laisser le tampon grandir sans fin.
    if (!eol && log_line_.size() < 512) {
      return;
    }

    // Le préfixe rappelle que la ligne vient du microcontrôleur et non du
    // calculateur — sinon, dans /rosout, les deux se confondent.
    const std::string text = "[esp32] " + log_line_;
    log_line_.clear();

    switch (level) {
      case 0: RCLCPP_ERROR(get_logger(), "%s", text.c_str()); break;
      case 1: RCLCPP_WARN(get_logger(), "%s", text.c_str()); break;
      case 3: RCLCPP_DEBUG(get_logger(), "%s", text.c_str()); break;
      default: RCLCPP_INFO(get_logger(), "%s", text.c_str()); break;
    }
  }

  std::uint32_t host_micros() const
  {
    return static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count());
  }

  void send_time_sync()
  {
    protocol::TimeSync ts{};
    ts.t_host_us = static_cast<std::uint64_t>(now().nanoseconds() / 1000);
    transport_->send(protocol::pack(ts));
  }

  void send_ping()
  {
    protocol::LinkPing ping{};
    ping.target = RT_NODE_ID_SAFETY;
    ping.seq = ping_seq_++;
    ping.t_tx_us = host_micros();
    transport_->send(protocol::pack(ping));
  }

  // -----------------------------------------------------------------------
  //  Sorties de banc
  // -----------------------------------------------------------------------
  void publish_bench_tf(const sensor_msgs::msg::Imu & imu, const rclcpp::Time & stamp)
  {
    geometry_msgs::msg::TransformStamped tf;
    tf.header.stamp = stamp;
    tf.header.frame_id = bench_parent_frame_;
    tf.child_frame_id = frame_id_;
    tf.transform.translation.x = 0.0;
    tf.transform.translation.y = 0.0;
    tf.transform.translation.z = 0.0;
    tf.transform.rotation = imu.orientation;
    tf_broadcaster_->sendTransform(tf);
  }

  void publish_marker()
  {
    // Foxglove n'affiche pas sensor_msgs/Imu en 3D : il affiche des repères TF
    // et des marqueurs. Sans géométrie, le panneau 3D montre un repère
    // d'axes qui tourne, ce qui est lisible mais austère. Ce pavé donne un
    // objet à regarder, orienté comme le capteur.
    visualization_msgs::msg::Marker m;
    m.header.frame_id = frame_id_;
    m.header.stamp = now();
    m.ns = "retriever";
    m.id = 0;
    m.type = visualization_msgs::msg::Marker::CUBE;
    m.action = visualization_msgs::msg::Marker::ADD;
    m.pose.orientation.w = 1.0;
    // ⚠️ Sans ceci, le marqueur est transformé UNE FOIS, à l'horodatage de sa
    // publication, et reste figé là où il a été placé au démarrage. Verrouillé
    // au repère, il suit imu_link à chaque mise à jour de la TF — ce qui est
    // tout l'intérêt de l'afficher.
    m.frame_locked = true;
    m.scale.x = 0.05;   // une carte BNO085 fait à peu près ça
    m.scale.y = 0.03;
    m.scale.z = 0.008;
    m.color.r = 0.2F;
    m.color.g = 0.6F;
    m.color.b = 0.9F;
    m.color.a = 0.9F;
    marker_pub_->publish(m);
  }

  // -----------------------------------------------------------------------
  //  État et diagnostics
  // -----------------------------------------------------------------------
  void publish_status()
  {
    const auto s = transport_->stats();
    retriever_msgs::msg::LinkStatus msg;
    msg.header.stamp = now();
    msg.transport = transport_->describe();
    msg.connected = s.connected;
    msg.rx_frames = s.rx_frames;
    msg.tx_frames = s.tx_frames;
    msg.crc_errors = s.crc_errors;
    msg.format_errors = s.format_errors;
    msg.overflows = s.overflows;
    msg.unknown_frames = unknown_frames_;
    msg.round_trip_ms = static_cast<float>(round_trip_ms_.load());
    msg.round_trip_ms_max = static_cast<float>(round_trip_max_ms_.load());
    msg.protocol_hash_host = protocol::kHash;
    msg.protocol_hash_node = node_hash_.load();
    msg.protocol_match = (node_hash_.load() != 0U) && (node_hash_.load() == protocol::kHash);
    link_status_pub_->publish(msg);

    // ⚠️ NodeStatus était publié uniquement à la réception d'un battement, donc
    // toujours avec un âge nul — et le publieur étant latché, un panneau ouvert
    // après une panne voyait un nœud en parfaite santé. On le republie ici avec
    // l'âge réel, pour que le seuil des 300 ms du §J.2 soit visible sur le topic
    // et pas seulement dans les diagnostics.
    retriever_msgs::msg::NodeStatus node;
    bool have = false;
    {
      std::lock_guard<std::mutex> lock(node_status_mutex_);
      node = last_node_status_;
      have = have_node_status_;
    }
    if (have) {
      const std::int64_t hb_ns = last_heartbeat_ns_.load(std::memory_order_relaxed);
      node.header.stamp = now();
      node.heartbeat_age_s =
        static_cast<float>(static_cast<double>(now().nanoseconds() - hb_ns) * 1e-9);
      node_status_pub_->publish(node);
    }

    diagnostics_.force_update();
  }

  void diagnose_link(diagnostic_updater::DiagnosticStatusWrapper & st)
  {
    const auto s = transport_->stats();
    st.add("transport", transport_->describe());
    st.add("trames recues", s.rx_frames);
    st.add("trames emises", s.tx_frames);
    st.add("erreurs CRC", s.crc_errors);
    st.add("erreurs de format", s.format_errors);
    st.add("debordements", s.overflows);
    st.add("trames inconnues", unknown_frames_.load());
    st.add("aller-retour ms", round_trip_ms_.load());
    st.add("aller-retour max ms", round_trip_max_ms_.load());

    const auto errors = s.crc_errors + s.format_errors + s.overflows;
    const double error_rate = s.rx_frames > 0
                                ? static_cast<double>(errors) / static_cast<double>(s.rx_frames)
                                : 0.0;

    if (!s.connected) {
      st.summary(diagnostic_msgs::msg::DiagnosticStatus::ERROR, "liaison fermee");
    } else if (node_hash_.load() != 0U && node_hash_.load() != protocol::kHash) {
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::ERROR,
        "divergence de protocole entre le firmware et le calculateur");
    } else if (s.rx_frames == 0) {
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::ERROR,
        "aucune trame recue — noeud muet ou mauvais peripherique");
    } else if (error_rate > 0.01) {
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::WARN, "taux d'erreur superieur a 1 %");
    } else {
      st.summary(diagnostic_msgs::msg::DiagnosticStatus::OK, "liaison saine");
    }
  }

  void diagnose_imu(diagnostic_updater::DiagnosticStatusWrapper & st)
  {
    const double rate = measured_rate_hz_.load();
    const double norm_error = last_norm_error_.load();
    const std::uint32_t resets = sensor_resets_.load();
    st.add("cadence Hz", rate);
    st.add("cadence attendue Hz", expected_rate_hz_);
    st.add("qualite orientation", static_cast<int>(last_status_rot_.load()));
    st.add("qualite gyro", static_cast<int>(last_status_gyro_.load()));
    st.add("qualite accel", static_cast<int>(last_status_accel_.load()));
    st.add("precision orientation rad", last_accuracy_rad_.load());
    st.add("resets du capteur", resets);
    st.add("echantillons perdus (noeud)", dropped_node_.load());
    st.add("echantillons perdus (lien)", dropped_link_.load());
    st.add("ecart de norme du quaternion", norm_error);
    st.add("etalonnage actif (masque)", static_cast<int>(cal_enabled_.load()));
    st.add("sauvegardes du DCD", static_cast<int>(cal_saves_.load()));
    st.add("sauvegarde auto du DCD", cal_autosave_.load());

    const std::int64_t imu_ns = last_imu_stamp_ns_.load(std::memory_order_relaxed);
    const double age =
      imu_ns == 0 ? 1e9 : static_cast<double>(now().nanoseconds() - imu_ns) * 1e-9;

    if (age > 1.0) {
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::ERROR,
        "aucun echantillon depuis plus d'une seconde");
    } else if (resets > 0) {
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::ERROR,
        "le capteur se reinitialise — alimentation ou cablage SPI");
    } else if (norm_error > 0.01) {
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::ERROR,
        "quaternion non unitaire — corruption ou erreur d'echelle");
    } else if (rate < expected_rate_hz_ * 0.8) {
      st.summary(diagnostic_msgs::msg::DiagnosticStatus::WARN, "cadence basse");
    } else if (cal_enabled_.load() == 0U) {
      // Sans étalonnage dynamique, le BNO085 sort ses valeurs d'usine : biais
      // accéléromètre d'un demi m/s², et un cap dont le capteur annonce
      // lui-même 180° d'incertitude. Ça ne ressemble pas à une panne dans les
      // données, et c'est bien le problème.
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::WARN,
        "etalonnage dynamique inactif — les biais ne sont pas corriges");
    } else if (cal_saves_.load() == 0U) {
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::WARN,
        "etalonnage non sauvegarde — il sera perdu a l'extinction "
        "(tools/imu_cal.py --save)");
    } else if (last_status_rot_.load() < 2) {
      // Le §P4 du pipeline d'auto-test attend une orientation fiable ; une
      // qualité basse au démarrage est normale, elle doit monter après
      // quelques mouvements.
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::WARN,
        "orientation peu fiable — etalonnage en cours");
    } else {
      st.summary(diagnostic_msgs::msg::DiagnosticStatus::OK, "capteur sain");
    }
  }

  void diagnose_node(diagnostic_updater::DiagnosticStatusWrapper & st)
  {
    const std::int64_t hb_ns = last_heartbeat_ns_.load(std::memory_order_relaxed);
    const double age =
      hb_ns == 0 ? 1e9 : static_cast<double>(now().nanoseconds() - hb_ns) * 1e-9;
    st.add("age du battement s", age);
    st.addf("hash du noeud", "0x%08X", node_hash_.load());
    st.addf("hash du calculateur", "0x%08X", protocol::kHash);

    if (age > 0.3) {
      // 300 ms : le seuil du §J.2 pour déclarer un nœud perdu. Le même partout.
      st.summary(
        diagnostic_msgs::msg::DiagnosticStatus::ERROR, "battement de coeur perdu");
    } else {
      st.summary(diagnostic_msgs::msg::DiagnosticStatus::OK, "noeud present");
    }
  }

  // -----------------------------------------------------------------------
  std::string transport_kind_;
  std::string serial_device_;
  int serial_baud_ = 921600;
  bool serial_reset_on_open_{true};
  std::string can_interface_;
  std::string frame_id_;
  double latency_offset_ms_ = 1.5;
  double ping_period_s_ = 1.0;
  bool time_sync_enabled_ = true;
  double expected_rate_hz_ = 100.0;
  bool bench_tf_ = false;
  std::string bench_parent_frame_;
  bool bench_marker_ = false;

  ImuNoiseModel noise_{};
  std::unique_ptr<Transport> transport_;

  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp::Publisher<sensor_msgs::msg::MagneticField>::SharedPtr mag_pub_;
  rclcpp::Publisher<retriever_msgs::msg::ImuStatus>::SharedPtr imu_status_pub_;
  rclcpp::Publisher<retriever_msgs::msg::LinkStatus>::SharedPtr link_status_pub_;
  rclcpp::Publisher<retriever_msgs::msg::NodeStatus>::SharedPtr node_status_pub_;
  rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr marker_pub_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;

  rclcpp::TimerBase::SharedPtr time_sync_timer_;
  rclcpp::TimerBase::SharedPtr ping_timer_;
  rclcpp::TimerBase::SharedPtr status_timer_;
  diagnostic_updater::Updater diagnostics_;

  std::thread reader_;
  std::atomic<bool> running_{false};

  std::mutex sample_mutex_;
  ImuSample pending_{};
  rclcpp::Time pending_stamp_{0, 0, RCL_ROS_TIME};
  std::uint8_t pending_seq_ = 0;

  // ⚠️ Écrits par le fil de lecture, lus par les rappels de minuterie.
  // rclcpp::Time fait 16 octets : une lecture déchirée donne un « âge »
  // aberrant, donc un diagnostic ERROR fantôme. Atomiques en nanosecondes.
  std::atomic<std::int64_t> last_imu_stamp_ns_{0};
  std::atomic<std::int64_t> last_heartbeat_ns_{0};
  // Copie privée au fil de lecture, pour le calcul de cadence.
  rclcpp::Time last_imu_stamp_{0, 0, RCL_ROS_TIME};
  std::atomic<double> measured_rate_hz_{0.0};
  std::atomic<double> last_norm_error_{0.0};
  std::atomic<double> round_trip_ms_{0.0};
  std::atomic<double> round_trip_max_ms_{0.0};
  std::atomic<std::uint32_t> dropped_link_{0};
  std::atomic<std::uint64_t> unknown_frames_{0};

  // Étalonnage, alimenté par IMU_CAL.
  std::atomic<std::uint8_t> cal_enabled_{0};
  std::atomic<std::uint8_t> cal_saves_{0};
  std::atomic<bool> cal_autosave_{false};
  std::atomic<std::int8_t> cal_last_result_{0};
  std::atomic<std::uint32_t> dropped_node_{0};
  std::atomic<std::uint32_t> sensor_resets_{0};
  std::atomic<std::uint32_t> node_hash_{0};
  bool hash_reported_ = false;
  std::atomic<float> last_accuracy_rad_{0.0F};
  std::atomic<std::uint8_t> last_status_rot_{0};
  std::atomic<std::uint8_t> last_status_gyro_{0};
  std::atomic<std::uint8_t> last_status_accel_{0};
  std::atomic<std::uint8_t> last_status_mag_{0};
  std::uint16_t ping_seq_ = 0;
  std::string log_line_;
  std::mutex node_status_mutex_;
  retriever_msgs::msg::NodeStatus last_node_status_;
  bool have_node_status_ = false;
};

}  // namespace retriever::link

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<retriever::link::BridgeNode>());
  } catch (const std::exception & e) {
    RCLCPP_FATAL(rclcpp::get_logger("retriever_link_bridge"), "%s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
