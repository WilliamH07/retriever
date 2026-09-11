// ===========================================================================
//  socketcan_transport.hpp — transport CAN par SocketCAN
//
//  ⚠️ ÉTAT : écrit, jamais exécuté — aucun adaptateur USB-CAN n'est en service
//  et la carte de distribution n'est pas fabriquée. Comme link_twai.c côté
//  firmware, ce fichier existe pour mesurer ce que coûte réellement le passage
//  de la série au CAN : un fichier de chaque côté, et rien d'autre.
//
//  Mise en service, côté Linux (§F.5-L1) :
//      sudo ip link set can0 up type can bitrate 500000 restart-ms 100
//      sudo ip link set can0 txqueuelen 1000
//
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#pragma once

#include <mutex>
#include <string>

#include "retriever_link/transport.hpp"

namespace retriever::link
{

class SocketCanTransport : public Transport
{
public:
  explicit SocketCanTransport(std::string interface);
  ~SocketCanTransport() override;

  void open() override;
  void close() override;
  bool receive(protocol::Frame & out, std::chrono::milliseconds timeout) override;
  bool send(const protocol::Frame & frame) override;
  Stats stats() const override;
  std::string describe() const override;

private:
  std::string interface_;
  int fd_ = -1;
  mutable std::mutex mutex_;
  Stats stats_{};
};

}  // namespace retriever::link
