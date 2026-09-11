// ===========================================================================
//  socketcan_transport.cpp — voir socketcan_transport.hpp
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#include "retriever_link/socketcan_transport.hpp"

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace retriever::link
{

SocketCanTransport::SocketCanTransport(std::string interface)
: interface_(std::move(interface))
{
}

SocketCanTransport::~SocketCanTransport() { close(); }

std::string SocketCanTransport::describe() const { return "socketcan " + interface_; }

void SocketCanTransport::open()
{
  fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if (fd_ < 0) {
    throw std::runtime_error(std::string("socket CAN : ") + std::strerror(errno));
  }

  ifreq ifr{};
  std::strncpy(ifr.ifr_name, interface_.c_str(), IFNAMSIZ - 1);
  if (::ioctl(fd_, SIOCGIFINDEX, &ifr) < 0) {
    const std::string why = std::strerror(errno);
    close();
    throw std::runtime_error(
            "interface " + interface_ + " introuvable : " + why +
            "\nLa monter avec : sudo ip link set " + interface_ +
            " up type can bitrate 500000 restart-ms 100");
  }

  sockaddr_can addr{};
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;
  if (::bind(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    const std::string why = std::strerror(errno);
    close();
    throw std::runtime_error("bind " + interface_ + " : " + why);
  }

  stats_ = Stats{};
  stats_.connected = true;
}

void SocketCanTransport::close()
{
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  stats_.connected = false;
}

bool SocketCanTransport::receive(protocol::Frame & out, std::chrono::milliseconds timeout)
{
  if (fd_ < 0) {
    return false;
  }
  pollfd pfd{fd_, POLLIN, 0};
  if (::poll(&pfd, 1, static_cast<int>(timeout.count())) <= 0) {
    return false;
  }

  can_frame cf{};
  const ssize_t n = ::read(fd_, &cf, sizeof(cf));
  if (n != static_cast<ssize_t>(sizeof(cf))) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.read_errors++;
    return false;
  }
  if ((cf.can_id & CAN_ERR_FLAG) != 0) {
    // Trame d'erreur du contrôleur : elle ne porte pas de données applicatives
    // mais elle est précieuse en diagnostic.
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.format_errors++;
    return false;
  }
  if ((cf.can_id & CAN_EFF_FLAG) != 0 || (cf.can_id & CAN_RTR_FLAG) != 0) {
    return false;   // le protocole n'utilise que des trames de données 11 bits
  }

  out.id = static_cast<std::uint16_t>(cf.can_id & CAN_SFF_MASK);
  out.dlc = cf.can_dlc > RT_MAX_PAYLOAD ? RT_MAX_PAYLOAD : cf.can_dlc;
  std::memset(out.data, 0, sizeof(out.data));
  std::memcpy(out.data, cf.data, out.dlc);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.rx_frames++;
  }
  return true;
}

bool SocketCanTransport::send(const protocol::Frame & frame)
{
  if (fd_ < 0) {
    return false;
  }
  can_frame cf{};
  cf.can_id = frame.id & CAN_SFF_MASK;
  cf.can_dlc = frame.dlc;
  std::memcpy(cf.data, frame.data, frame.dlc);
  if (::write(fd_, &cf, sizeof(cf)) != static_cast<ssize_t>(sizeof(cf))) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.tx_frames++;
  }
  return true;
}



Stats SocketCanTransport::stats() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

}  // namespace retriever::link
