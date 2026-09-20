#!/usr/bin/env python3
"""Étalonner le BNO085 depuis le banc, et voir où on en est.

    python3 tools/imu_cal.py --device /dev/ttyUSB0                 # observer
    python3 tools/imu_cal.py --device /dev/ttyUSB0 --enable
    python3 tools/imu_cal.py --device /dev/ttyUSB0 --save
    python3 tools/imu_cal.py --device /dev/ttyUSB0 --clear

Pourquoi cet outil existe. Le BNO085 corrige ses biais en fonctionnement, mais
deux choses distinctes doivent être vraies, et les confondre coûte une séance :

  ACTIF        le capteur corrige en ce moment            → champ `active`
  SAUVEGARDÉ   la correction survivra à l'extinction      → champ `sauvegardes`

Le firmware active l'étalonnage au démarrage et demande la sauvegarde
automatique. Cet outil sert à le vérifier, à forcer une écriture quand on veut
être sûr, et à repartir de zéro quand un étalonnage a mal tourné.

LA PROCÉDURE, une fois l'outil lancé en observation :

  1. accéléromètre — poser la carte sur ses six faces, immobile ~1 s chacune.
     `accel` monte à 3.
  2. gyromètre — laisser la carte parfaitement immobile ~3 s. `gyro` monte à 3.
  3. magnétomètre — un huit lent en l'air, loin de tout métal et de toute
     alimentation, une quinzaine de secondes. `mag` puis `orientation` montent.
  4. quand les quatre sont à 3, `--save`, et vérifier que `sauvegardes` incrémente.

⚠️ Un seul programme peut tenir le port série. Fermer link_monitor.py et le
nœud ROS avant de lancer celui-ci.

Dépendance : pyserial.

Copyright (c) 2026 William Hanczyk — Apache License 2.0
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from retriever_wire import Decoder, Protocol, open_port  # noqa: E402

try:
    import serial
except ImportError:  # pragma: no cover
    print("pyserial manquant :  pip install pyserial", file=sys.stderr)
    raise SystemExit(2)

MAGIC = 0xCA
QUALITE = {0: "nulle", 1: "basse", 2: "moyenne", 3: "haute"}


def diagnostic(stats, octets: int, vues: dict[int, int]) -> str:
    """Ce que l'outil reçoit vraiment.

    ⚠️ La première version ne montrait rien de tout ça : elle affichait « en
    attente d'une trame IMU_CAL » aussi bien devant un port muet que devant un
    flux valide dont elle ratait la trame. Un outil de diagnostic qui ne
    distingue pas ces deux cas fait perdre plus de temps qu'il n'en fait gagner.
    """
    erreurs = stats.crc_errors + stats.format_errors + stats.overflows
    ligne = (f"  liaison      {octets} octets   ·   {stats.frames_ok} trames valides"
             f"   ·   {erreurs} erreurs")
    if octets == 0:
        return ligne + "\n  ⚠ rien n'arrive sur ce port — mauvais port, ou le nœud ne parle pas"
    if stats.frames_ok == 0:
        return ligne + "\n  ⚠ des octets arrivent mais aucune trame valide — mauvais débit ?"
    if vues:
        noms = " ".join(f"0x{i:03X}×{n}" for i, n in sorted(vues.items()))
        return ligne + "\n  reçues       " + noms
    return ligne


def render(cal: dict | None, status: dict | None) -> str:
    if cal is None:
        return "  en attente d'une trame IMU_CAL…"

    enabled = int(cal["enabled"])
    actifs = [n for b, n in ((0x01, "accel"), (0x02, "gyro"), (0x04, "mag")) if enabled & b]
    saves = int(cal["saves"])
    res = int(cal["last_result"])

    lignes = [
        "  étalonnage   actif sur : " + (", ".join(actifs) if actifs else "RIEN")
        + f"   ·   sauvegarde auto : {'oui' if int(cal['flags']) & 1 else 'NON'}",
        f"  sauvegardes en flash : {saves}"
        + ("   ← tout est encore en RAM" if saves == 0 else "   ✔ conservé à l'extinction"),
    ]
    if res != 0:
        lignes.append(f"  ⚠ dernière commande : code SH-2 {res}")

    mag = QUALITE.get(int(cal["status_mag"]), "?")
    if status is not None:
        lignes.append(
            f"  qualité      orientation {QUALITE.get(int(status['status_rot']), '?'):<8}"
            f"gyro {QUALITE.get(int(status['status_gyro']), '?'):<8}"
            f"accel {QUALITE.get(int(status['status_accel']), '?'):<8}"
            f"mag {mag}"
        )
        lignes.append(f"  cap : ±{float(status['quat_accuracy']):.3f} rad annoncés par le capteur")
    else:
        lignes.append(f"  qualité      mag {mag}")
    return "\n".join(lignes)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--device", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=921600)
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--enable", action="store_true", help="activer l'étalonnage dynamique")
    g.add_argument("--disable", action="store_true", help="figer les corrections en cours")
    g.add_argument("--save", action="store_true", help="écrire le DCD en flash maintenant")
    g.add_argument("--clear", action="store_true",
                   help="EFFACER le DCD et redémarrer le capteur — irréversible")
    ap.add_argument("--sensors", default="accel,gyro,mag",
                    help="capteurs concernés par --enable (défaut : les trois)")
    ap.add_argument("--seconds", type=float, default=0.0,
                    help="durée d'observation ; 0 = sans fin (Ctrl-C pour sortir)")
    args = ap.parse_args()

    proto = Protocol()
    actions = proto.enums["imu_cal_action"]["values"]

    action = None
    if args.enable:
        action = actions["ENABLE"]
    elif args.disable:
        action = actions["DISABLE"]
    elif args.save:
        action = actions["SAVE"]
    elif args.clear:
        print("Effacer le DCD détruit l'étalonnage stocké en flash. Il faudra")
        print("refaire les six faces et le huit magnétique.")
        if input("Taper EFFACER pour confirmer : ").strip() != "EFFACER":
            print("annulé.")
            return 1
        action = actions["CLEAR"]

    mask = 0
    for nom, bit in (("accel", 0x01), ("gyro", 0x02), ("mag", 0x04)):
        if nom in args.sensors:
            mask |= bit

    port = open_port(args.device, args.baud)
    if action is not None:
        port.write(proto.encode_named("IMU_CAL_CMD", action=action,
                                      sensors=mask, magic=MAGIC))
        port.flush()

    decoder = Decoder()
    id_cal = proto.by_name["IMU_CAL"].id
    id_status = proto.by_name["IMU_STATUS"].id
    cal: dict | None = None
    status: dict | None = None
    octets = 0
    vues: dict[int, int] = {}

    debut = time.monotonic()
    prochain_rendu = 0.0
    try:
        while True:
            en_attente = port.in_waiting
            data = port.read(en_attente if en_attente else 1)
            octets += len(data)
            for frame_id, payload in decoder.feed(data):
                vues[frame_id] = vues.get(frame_id, 0) + 1
                fd = proto.frames.get(frame_id)
                if fd is None:
                    continue
                if frame_id == id_cal:
                    cal = fd.decode(payload)
                elif frame_id == id_status:
                    status = fd.decode(payload)

            maintenant = time.monotonic()
            if maintenant >= prochain_rendu:
                prochain_rendu = maintenant + 0.5
                sys.stdout.write("\033[2J\033[H")
                ecoule = maintenant - debut
                sys.stdout.write(
                    f"  {args.device}   ·   {ecoule:6.1f} s\n\n"
                    + diagnostic(decoder.stats, octets, vues)
                    + "\n\n"
                    + render(cal, status)
                    + "\n\n  six faces → accel · immobile → gyro · huit en l'air → mag\n"
                    + "  puis :  python3 tools/imu_cal.py --device "
                    + f"{args.device} --save\n"
                )
                sys.stdout.flush()

            if args.seconds and maintenant - debut >= args.seconds:
                return 0
    except KeyboardInterrupt:
        print()
        return 0


if __name__ == "__main__":
    sys.exit(main())
