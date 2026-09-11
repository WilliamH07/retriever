// ===========================================================================
//  serial_transport.hpp — transport série, cadrage COBS
//
//  Le cadrage n'est PAS réimplémenté ici : ce fichier appelle rt_framing.c,
//  exactement le même fichier C que compile le firmware. Une seule
//  implémentation, donc aucune divergence possible entre les deux bouts du
//  câble — c'est le principe §P.2-1 appliqué au plus bas niveau.
//
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#pragma once

#include <array>
#include <deque>
#include <mutex>
#include <string>

#include "retriever_link/transport.hpp"

namespace retriever::link
{

class SerialTransport : public Transport
{
public:
  SerialTransport(std::string device, int baudrate);
  ~SerialTransport() override;

  void open() override;
  void close() override;
  bool receive(protocol::Frame & out, std::chrono::milliseconds timeout) override;
  bool send(const protocol::Frame & frame) override;
  Stats stats() const override;
  std::string describe() const override;

private:
  void refresh_stats();

  std::string device_;
  int baudrate_;
  int fd_ = -1;

  rt_frame_decoder_t decoder_{};
  std::deque<protocol::Frame> pending_;
  std::array<std::uint8_t, 1024> buffer_{};
  mutable std::mutex mutex_;
  Stats stats_{};
};

}  // namespace retriever::link
