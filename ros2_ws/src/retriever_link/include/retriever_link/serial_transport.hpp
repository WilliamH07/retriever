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
  /**
   * @param reset_on_open  Remet la carte en mode EXÉCUTION à l'ouverture du
   *   port, par une impulsion sur EN avec IO0 maintenu haut.
   *
   *   ⚠️ Indispensable sur une DevKitC. Les lignes DTR et RTS pilotent IO0 et
   *   EN à travers le circuit d'auto-reset ; l'ouverture d'un port les fait
   *   bouger, et selon l'ordre des transitions la carte démarre en mode
   *   TÉLÉCHARGEMENT. Elle attend alors un téléversement, à 115200, et reste
   *   donc muette pour un hôte qui lit à 921600 — sans qu'aucune erreur ne
   *   soit levée nulle part. Observé au banc le 20 septembre 2026 : la liaison
   *   marchait après un rebranchement physique, puis plus rien à chaque
   *   relance du nœud.
   *
   *   À passer à false le jour où ce transport servira à autre chose qu'un
   *   banc : redémarrer un nœud de sécurité parce que le calculateur ouvre un
   *   port est une mauvaise idée sur un robot. Sur CAN la question disparaît.
   */
  SerialTransport(std::string device, int baudrate, bool reset_on_open = true);
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
  bool reset_on_open_;
  int fd_ = -1;

  rt_frame_decoder_t decoder_{};
  std::deque<protocol::Frame> pending_;
  std::array<std::uint8_t, 1024> buffer_{};
  mutable std::mutex mutex_;
  Stats stats_{};
};

}  // namespace retriever::link
