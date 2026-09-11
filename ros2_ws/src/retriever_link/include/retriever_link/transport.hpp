// ===========================================================================
//  transport.hpp — la même abstraction que côté firmware, côté calculateur
//
//  Symétrie voulue : le nœud ROS ne sait pas plus que le firmware par quel
//  câble arrivent les trames. Deux implémentations, une interface, et le
//  passage de la série au CAN est un paramètre de lancement.
//
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "retriever_protocol/protocol.hpp"

extern "C" {
#include "rt_framing.h"
}

namespace retriever::link
{

struct Stats
{
  std::uint64_t rx_frames = 0;
  std::uint64_t tx_frames = 0;
  std::uint64_t crc_errors = 0;
  std::uint64_t format_errors = 0;
  std::uint64_t overflows = 0;
  std::uint64_t read_errors = 0;
  bool connected = false;
};

/// Un transport. Trois opérations, comme côté firmware.
class Transport
{
public:
  virtual ~Transport() = default;

  /// Ouvre le lien. Lève std::runtime_error avec un message exploitable.
  virtual void open() = 0;
  virtual void close() = 0;

  /// Attend une trame. `false` sur expiration du délai — pas une erreur.
  virtual bool receive(protocol::Frame & out, std::chrono::milliseconds timeout) = 0;

  virtual bool send(const protocol::Frame & frame) = 0;

  /// Copie des compteurs. Par valeur, parce que le fil de lecture les met a
  /// jour pendant que le fil des minuteries les lit.
  virtual Stats stats() const = 0;

  /// Nom lisible, pour les journaux et les diagnostics.
  virtual std::string describe() const = 0;
};

}  // namespace retriever::link
