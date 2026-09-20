// ===========================================================================
//  serial_transport.cpp — voir serial_transport.hpp
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#include "retriever_link/serial_transport.hpp"

#include "rclcpp/rclcpp.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>

namespace retriever::link
{
namespace
{

speed_t to_speed(int baud)
{
  switch (baud) {
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 500000: return B500000;
    case 576000: return B576000;
    case 921600: return B921600;
    case 1000000: return B1000000;
    case 1500000: return B1500000;
    case 2000000: return B2000000;
    default: return 0;
  }
}

}  // namespace

SerialTransport::SerialTransport(std::string device, int baudrate)
: device_(std::move(device)), baudrate_(baudrate)
{
  rt_frame_decoder_init(&decoder_);
}

SerialTransport::~SerialTransport() { close(); }

std::string SerialTransport::describe() const
{
  return "serie " + device_ + " a " + std::to_string(baudrate_) + " bauds";
}

void SerialTransport::open()
{
  const speed_t speed = to_speed(baudrate_);
  if (speed == 0) {
    throw std::runtime_error("debit non supporte : " + std::to_string(baudrate_));
  }

  fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    throw std::runtime_error(
            "ouverture de " + device_ + " impossible : " + std::strerror(errno) +
            "\nVerifier que le peripherique existe (ls /dev/ttyUSB*) et que "
            "l'utilisateur est dans le groupe dialout.");
  }

  termios tio{};
  if (tcgetattr(fd_, &tio) != 0) {
    const std::string why = std::strerror(errno);
    close();
    throw std::runtime_error("tcgetattr : " + why);
  }

  cfmakeraw(&tio);
  cfsetispeed(&tio, speed);
  cfsetospeed(&tio, speed);

  tio.c_cflag |= (CLOCAL | CREAD);
  tio.c_cflag &= ~CSTOPB;      // 1 bit de stop
  tio.c_cflag &= ~PARENB;      // pas de parité
  tio.c_cflag &= ~CRTSCTS;     // pas de contrôle de flux matériel

  // ⚠️ HUPCL fait abaisser DTR et RTS à la fermeture du port. Sur une DevKitC
  // ESP32, ces deux lignes ne sont pas décoratives : elles pilotent EN (reset)
  // et IO0 (mode démarrage) à travers le circuit d'auto-reset du montage. Les
  // laisser au noyau, c'est redémarrer la carte à chaque ouverture et, selon
  // l'ordre des transitions, la laisser dans le bootloader ROM — muette, sans
  // la moindre erreur côté hôte. Symptôme observé le 20 septembre 2026 : la
  // liaison marchait après un rebranchement physique, puis plus rien dès qu'un
  // second programme rouvrait le port.
  tio.c_cflag &= ~HUPCL;

  // Lecture non bloquante : l'attente se fait dans poll(), ce qui permet
  // d'annuler proprement à l'arrêt du nœud.
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 0;

  if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
    const std::string why = std::strerror(errno);
    close();
    throw std::runtime_error("tcsetattr : " + why);
  }

  // Et on désassertit explicitement les deux lignes : avec DTR et RTS au repos,
  // les deux transistors du circuit d'auto-reset sont bloqués, EN et IO0
  // remontent, et la carte tourne normalement. C'est ce que fait esptool quand
  // on lui demande de ne pas redémarrer la cible.
  int modem_bits = TIOCM_DTR | TIOCM_RTS;
  if (::ioctl(fd_, TIOCMBIC, &modem_bits) != 0) {
    // Certains pilotes ne l'implémentent pas. Ce n'est pas fatal : on le dit et
    // on continue, plutôt que de refuser d'ouvrir un port qui marche peut-être.
    RCLCPP_WARN(
      rclcpp::get_logger("retriever_link"),
      "impossible de relacher DTR/RTS sur %s (%s) — si la carte ne parle pas, "
      "c'est la premiere chose a regarder",
      device_.c_str(), std::strerror(errno));
  }

  // ⚠️ Sans cette purge, les octets accumulés pendant que le nœud était arrêté
  // sont lus au démarrage et produisent une bordée d'erreurs de cadrage qui
  // ressemble à un vrai problème de liaison.
  tcflush(fd_, TCIOFLUSH);

  rt_frame_decoder_init(&decoder_);
  pending_.clear();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Stats{};
    stats_.connected = true;
  }
}

void SerialTransport::close()
{
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  stats_.connected = false;
}

bool SerialTransport::receive(protocol::Frame & out, std::chrono::milliseconds timeout)
{
  if (!pending_.empty()) {
    out = pending_.front();
    pending_.pop_front();
    return true;
  }
  if (fd_ < 0) {
    return false;
  }

  pollfd pfd{fd_, POLLIN, 0};
  const int rc = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
  if (rc <= 0) {
    return false;
  }

  const ssize_t n = ::read(fd_, buffer_.data(), buffer_.size());
  if (n < 0) {
    if (errno != EAGAIN && errno != EINTR) {
      std::lock_guard<std::mutex> lock(mutex_);
      stats_.read_errors++;
    }
    return false;
  }
  if (n == 0) {
    return false;
  }

  rt_frame_t frame{};
  for (ssize_t i = 0; i < n; ++i) {
    if (rt_frame_decoder_push(&decoder_, buffer_[static_cast<std::size_t>(i)], &frame)) {
      pending_.push_back(frame);
    }
  }
  refresh_stats();

  if (pending_.empty()) {
    return false;
  }
  out = pending_.front();
  pending_.pop_front();
  return true;
}

bool SerialTransport::send(const protocol::Frame & frame)
{
  if (fd_ < 0) {
    return false;
  }
  std::array<std::uint8_t, RT_WIRE_MAX> wire{};
  const std::size_t n = rt_frame_encode(&frame, wire.data(), wire.size());
  if (n == 0) {
    return false;
  }
  // ⚠️ Le descripteur est non bloquant. Une boucle qui se contente de réessayer
  // sur EAGAIN tourne à 100 % de CPU, sans délai maximal — et comme send() est
  // appelée depuis le fil de l'exécuteur, elle gèlerait tout le nœud. On attend
  // que la sortie se libère, avec un plafond, puis on abandonne la trame.
  std::size_t written = 0;
  while (written < n) {
    const ssize_t w = ::write(fd_, wire.data() + written, n - written);
    if (w > 0) {
      written += static_cast<std::size_t>(w);
      continue;
    }
    if (w < 0 && (errno == EAGAIN || errno == EINTR)) {
      pollfd pfd{fd_, POLLOUT, 0};
      if (::poll(&pfd, 1, 20) <= 0) {
        return false;
      }
      continue;
    }
    // w == 0 : errno n'a pas été positionné par cet appel, on n'y lit rien.
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.tx_frames++;
  }
  return true;
}

void SerialTransport::refresh_stats()
{
  std::lock_guard<std::mutex> lock(mutex_);
  stats_.rx_frames = decoder_.stats.frames_ok;
  stats_.crc_errors = decoder_.stats.crc_errors;
  stats_.format_errors = decoder_.stats.format_errors;
  stats_.overflows = decoder_.stats.overflows;
}

Stats SerialTransport::stats() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

}  // namespace retriever::link
