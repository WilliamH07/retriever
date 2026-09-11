#!/usr/bin/env python3
"""Vérifie que les trois implémentations du protocole s'accordent.

    python3 tools/check_protocol_sync.py

Trois vérifications, dans cet ordre :

  1. Les fichiers générés correspondent-ils au YAML ?
     Sinon, quelqu'un a édité le YAML sans relancer le générateur, ou bien a
     édité un fichier généré à la main.

  2. Le C et le Python produisent-ils les mêmes octets sur le fil ?
     Le C est généré depuis le YAML, le Python le lit à l'exécution. Un
     désaccord signale une erreur dans l'un des deux, pas dans le YAML.

  3. Les vecteurs d'or tiennent-ils toujours ?
     Le format du fil est figé : un enregistrement d'aujourd'hui doit rester
     lisible dans six mois.

À lancer en CI, et avant tout flashage. Le mode de défaillance que ça évite —
un nœud avec un firmware d'une version et un calculateur d'une autre — ne
produit aucune erreur franche : il produit des valeurs plausibles et fausses.

Copyright (c) 2026 William Hanczyk — Apache License 2.0
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

from retriever_wire import Protocol, crc16  # noqa: E402

FAILURES: list[str] = []


def step(label: str) -> None:
    print(f"\n=== {label} ===")


def fail(msg: str) -> None:
    FAILURES.append(msg)
    print(f"  ÉCHEC {msg}")


def check_generated() -> None:
    step("fichiers générés à jour")
    rc = subprocess.run(
        [sys.executable, str(REPO / "firmware" / "protocol" / "generate.py"), "--check"],
        cwd=REPO,
    ).returncode
    if rc != 0:
        fail("les fichiers générés ne correspondent plus à protocol.yaml")
    else:
        print("  ok")


def check_cross_language() -> None:
    step("accord C ↔ Python sur les octets du fil")

    if shutil.which("make") is None or shutil.which("gcc") is None:
        print("  ignoré : make ou gcc absent")
        return

    result = subprocess.run(
        ["make", "-s", "vectors"],
        cwd=REPO / "firmware" / "test",
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        fail(f"compilation des vecteurs impossible :\n{result.stderr}")
        return

    proto = Protocol()
    checked = 0
    for line in result.stdout.strip().splitlines():
        name, frame_id, dlc, hexwire = line.split("\t")
        frame_id, dlc = int(frame_id), int(dlc)

        # Même motif déterministe que emit_vectors.c.
        index = list(proto.frames).index(frame_id) if frame_id in proto.frames else -1
        if index < 0:
            fail(f"{name} : identifiant 0x{frame_id:03X} absent du YAML")
            continue
        payload = bytes(
            0x00 if b == 2 else (0xFF if b == 5 else (index * 7 + b) & 0xFF)
            for b in range(dlc)
        )

        mine = proto.encode_frame(frame_id, payload).hex()
        if mine != hexwire:
            fail(f"{name} : C={hexwire} Python={mine}")
        checked += 1

    print(f"  {checked} trames comparées")


def check_golden() -> None:
    step("vecteurs d'or")

    if crc16(b"123456789") != 0x29B1:
        fail("CRC-16/CCITT-FALSE : la variante a changé")

    proto = Protocol()

    # Quaternion d'une rotation de 90° autour de z, en Q14.
    quat = proto.by_name["IMU_QUAT"].encode({"w": 0.70710678, "x": 0, "y": 0, "z": 0.70710678})
    if quat.hex() != "412d00000000412d":
        fail(f"IMU_QUAT : {quat.hex()} au lieu de 412d00000000412d")

    gyro = proto.by_name["IMU_GYRO"].encode(
        {"gx": 0.5, "gy": -1.25, "gz": 0.0, "seq": 42, "flags": 0x07}
    )
    if gyro.hex() != "e8033cf600002a07":
        fail(f"IMU_GYRO : {gyro.hex()} au lieu de e8033cf600002a07")

    if not FAILURES:
        print("  ok")


def main() -> int:
    check_generated()
    check_cross_language()
    check_golden()

    print()
    if FAILURES:
        print(f"{len(FAILURES)} échec(s)")
        return 1
    print("protocole cohérent sur les trois implémentations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
