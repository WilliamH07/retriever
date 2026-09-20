#!/usr/bin/env python3
"""Moniteur de liaison — voir ce qui sort de l'ESP32, sans ROS.

    python3 tools/link_monitor.py --device /dev/ttyUSB0
    python3 tools/link_monitor.py --watch IMU_QUAT
    python3 tools/link_monitor.py --raw

Pourquoi cet outil existe : quand rien ne marche, il faut pouvoir répondre à
« l'ESP32 parle-t-il ? » sans avoir à démarrer ROS 2, et donc sans que la
réponse dépende d'une deuxième pile logicielle. C'est le premier outil à
lancer sur un banc neuf, et le seul dont on ait besoin pour prononcer les
quatre premiers points de la recette B1.

Il émet aussi TIME_SYNC et LINK_PING, ce qui permet de mesurer l'aller-retour
réel de la liaison — la mesure dont on a besoin pour régler
`link.latency_offset_ms` côté ROS plutôt que de le deviner.

Dépendance : pyserial (pip install pyserial).

Copyright (c) 2026 William Hanczyk — Apache License 2.0
"""

from __future__ import annotations

import argparse
import sys
import time
from collections import defaultdict, deque
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from retriever_wire import Decoder, Protocol, open_port  # noqa: E402

try:
    import serial
except ImportError:  # pragma: no cover
    print("pyserial manquant :  pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


class Monitor:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.proto = Protocol()
        self.decoder = Decoder()
        # ⚠️ 5 ms, et surtout pas 50.
        #
        # `read(n)` de pyserial rend la main quand il a n octets OU quand le
        # délai expire. En demandant 4096 octets avec un délai de 50 ms sur une
        # liaison peu chargée, la boucle attendait systématiquement les 50 ms
        # complètes — et un LINK_PONG arrivé juste après un retour attendait le
        # tour suivant. La mesure d'aller-retour ne mesurait donc pas la
        # liaison, elle mesurait ce délai.
        #
        # open_port() plutôt que serial.Serial() : voir sa docstring — il ne
        # faut pas que l'ouverture du port redémarre la carte.
        self.port = open_port(args.device, args.baud)

        self.counts: dict[int, int] = defaultdict(int)
        self.recent: dict[int, deque[float]] = defaultdict(lambda: deque(maxlen=200))
        self.last_values: dict[int, dict] = {}
        self.log_line = ""
        # ⚠️ Les journaux du firmware sont la donnée la plus utile du banc. La
        # première version les imprimait, puis le tableau repeignait l'écran
        # par-dessus toutes les 500 ms : ils étaient émis, reçus, décodés, et
        # invisibles. On les garde et on les affiche SOUS le tableau.
        self.log_lines: deque[str] = deque(maxlen=14)
        # Compteurs d'erreur il y a ~10 s, pour distinguer le bruit de
        # démarrage — le journal de boot à 115200 — d'une ligne qui va mal.
        self.err_history: deque[tuple[float, int]] = deque(maxlen=400)
        self.round_trip_ms: deque[float] = deque(maxlen=100)
        self.ping_seq = 0
        self.started = time.monotonic()
        self.node_hash: int | None = None

    # -- émission ---------------------------------------------------------

    def send_time_sync(self) -> None:
        self.port.write(
            self.proto.encode_named("TIME_SYNC", t_host_us=int(time.time() * 1e6))
        )

    def send_ping(self) -> None:
        self.ping_seq = (self.ping_seq + 1) & 0xFFFF
        t_us = int(time.monotonic() * 1e6) & 0xFFFFFFFF
        self.port.write(
            self.proto.encode_named(
                "LINK_PING",
                target=self.proto.enums["node_id"]["values"]["SAFETY"],
                seq=self.ping_seq,
                t_tx_us=t_us,
            )
        )

    # -- réception --------------------------------------------------------

    def handle(self, frame_id: int, payload: bytes) -> None:
        now = time.monotonic()
        self.counts[frame_id] += 1
        self.recent[frame_id].append(now)

        fd = self.proto.frames.get(frame_id)
        if fd is None:
            return
        values = fd.decode(payload)
        self.last_values[frame_id] = values

        if fd.name == "LOG":
            self.handle_log(values)
        elif fd.name == "LINK_PONG":
            elapsed = (int(time.monotonic() * 1e6) - int(values["t_tx_us"])) & 0xFFFFFFFF
            ms = elapsed / 1000.0
            if 0.0 <= ms < 1000.0:
                self.round_trip_ms.append(ms)
        elif fd.name == "HEARTBEAT_SAFETY":
            self.node_hash = int(values["protocol_hash"])

        if self.args.watch and fd.name == self.args.watch:
            parts = " ".join(
                f"{k}={v:.5g}" if isinstance(v, float) else f"{k}={v}"
                for k, v in values.items()
            )
            print(f"{fd.name:<22} {parts}")

    def handle_log(self, values: dict) -> None:
        header = int(values["header"])
        length = min(header & 0x0F, 7)
        eol = bool(header & 0x10)
        level = (header >> 5) & 0x07
        chars = bytes(int(values[f"c{i}"]) for i in range(length))
        self.log_line += chars.decode("utf-8", errors="replace")
        if eol or len(self.log_line) > 400:
            tag = {0: "ERREUR", 1: "ATTENTION", 2: "INFO", 3: "DEBUG"}.get(level, "INFO")
            stamp = time.strftime("%H:%M:%S")
            line = f"{stamp}  {tag:<9} {self.log_line}"
            if self.args.watch or self.args.raw:
                print(line)
            else:
                self.log_lines.append(line)
            self.log_line = ""

    # -- affichage --------------------------------------------------------

    def rate(self, frame_id: int) -> float:
        stamps = self.recent[frame_id]
        if len(stamps) < 2:
            return 0.0
        span = stamps[-1] - stamps[0]
        return (len(stamps) - 1) / span if span > 1e-6 else 0.0

    def recent_errors(self) -> tuple[int, float]:
        """Erreurs sur les dix dernières secondes, et la fenêtre réellement couverte."""
        s = self.decoder.stats
        total = s.crc_errors + s.format_errors + s.overflows
        now = time.monotonic()
        self.err_history.append((now, total))
        while len(self.err_history) > 1 and now - self.err_history[0][0] > 10.0:
            self.err_history.popleft()
        t0, e0 = self.err_history[0]
        return total - e0, now - t0

    def render(self) -> None:
        s = self.decoder.stats
        uptime = time.monotonic() - self.started
        recent_err, window = self.recent_errors()

        lines = [
            "\033[2J\033[H",
            f"  {self.args.device} à {self.args.baud} bauds"
            f"   ·   {uptime:6.1f} s   ·   protocole {self.proto.meta['version']}",
            "",
            f"  trames valides {s.frames_ok:<10} CRC faux {s.crc_errors:<8} "
            f"format {s.format_errors:<8} débordements {s.overflows}",
            f"  erreurs sur les {window:.0f} dernières secondes : {recent_err}"
            + ("   ← c'est ce chiffre qui compte" if recent_err == 0 else "   ⚠ la ligne a un problème"),
        ]

        if self.round_trip_ms:
            ordered = sorted(self.round_trip_ms)
            median = ordered[len(ordered) // 2]
            lines.append(
                f"  aller-retour  médiane {median:.2f} ms   max {ordered[-1]:.2f} ms"
                f"   →  latency_offset_ms ≈ {median / 2:.2f}"
            )
        else:
            lines.append("  aller-retour  pas encore de réponse au ping")

        if self.node_hash is not None:
            lines.append(f"  hash du protocole annoncé par le nœud : 0x{self.node_hash:08X}")

        imu = self.last_values.get(self.proto.by_name["IMU_STATUS"].id)
        if imu:
            qualite = {0: "nulle", 1: "basse", 2: "moyenne", 3: "haute"}
            lines.append(
                f"  IMU  orientation {qualite.get(int(imu['status_rot']), '?'):<8}"
                f"gyro {qualite.get(int(imu['status_gyro']), '?'):<8}"
                f"accel {qualite.get(int(imu['status_accel']), '?'):<8}"
                f"resets {int(imu['reset_count'])}   perdus {int(imu['dropped'])}"
            )

        lines += ["", f"  {'trame':<24}{'id':>6}{'reçues':>10}{'Hz':>9}{'attendu':>9}", ""]

        for frame_id in sorted(self.counts):
            fd = self.proto.frames.get(frame_id)
            name = fd.name if fd else f"INCONNU_0x{frame_id:03X}"
            expected = f"{fd.rate_hz:g}" if fd and fd.rate_hz else "—"
            measured = self.rate(frame_id)
            flag = ""
            if fd and fd.rate_hz and measured > 0 and measured < fd.rate_hz * 0.8:
                flag = "  ⚠ basse"
            lines.append(
                f"  {name:<24}0x{frame_id:03X}{self.counts[frame_id]:>10}"
                f"{measured:>9.1f}{expected:>9}{flag}"
            )

        if not self.counts:
            lines += [
                "",
                "  Rien ne vient. Dans l'ordre :",
                "    1. le bon port ?            ls -l /dev/serial/by-id/",
                "    2. le bon débit ?           le firmware est à 921600",
                "    3. le nœud tourne-t-il ?    la LED d'alimentation de la DevKitC",
                "    4. TX et RX croisés ?       sur une DevKitC par USB, rien à croiser",
            ]

        if self.log_lines:
            lines += ["", "  ── journaux du firmware " + "─" * 44, ""]
            lines += [f"  {l}" for l in self.log_lines]

        sys.stdout.write("\n".join(lines) + "\n")
        sys.stdout.flush()

    # -- boucle -----------------------------------------------------------

    def run(self) -> int:
        next_sync = 0.0
        next_ping = 0.0
        next_render = 0.0

        try:
            while True:
                # Tout ce qui est déjà arrivé, sinon un octet — qui borne
                # l'attente au délai du port, soit 5 ms.
                pending = self.port.in_waiting
                data = self.port.read(pending if pending else 1)
                if data:
                    if self.args.raw:
                        print(data.hex())
                    for frame_id, payload in self.decoder.feed(data):
                        self.handle(frame_id, payload)

                now = time.monotonic()
                if now >= next_sync:
                    next_sync = now + 1.0
                    self.send_time_sync()
                if now >= next_ping:
                    next_ping = now + 1.0
                    self.send_ping()
                if not self.args.watch and not self.args.raw and now >= next_render:
                    next_render = now + 0.5
                    self.render()
        except KeyboardInterrupt:
            print()
            return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--device", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--watch", help="afficher les champs décodés de cette trame")
    ap.add_argument("--raw", action="store_true", help="hexdump brut, sans décodage")
    return Monitor(ap.parse_args()).run()


if __name__ == "__main__":
    sys.exit(main())
