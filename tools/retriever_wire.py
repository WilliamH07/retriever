"""Codec du protocole Retriever, en Python.

Troisième implémentation du même protocole, après le C du firmware et le C++
de ROS — mais la seule qui ne soit pas générée : elle lit `protocol.yaml`
DIRECTEMENT, à l'exécution. Elle ne peut donc pas se désynchroniser du YAML,
et `tools/check_protocol_sync.py` vérifie qu'elle s'accorde avec le C généré
sur des vecteurs de test.

Sert aux outils de banc, qui doivent marcher sans ROS et sans compilateur :
sur une machine où l'on veut juste voir ce qui sort de l'ESP32.

Copyright (c) 2026 William Hanczyk — Apache License 2.0
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import yaml

REPO = Path(__file__).resolve().parent.parent
PROTOCOL_YAML = REPO / "firmware" / "protocol" / "protocol.yaml"

DELIMITER = 0
PACKET_MAX = 12
WIRE_MAX = 14

_FMT = {
    "u8": "<B", "i8": "<b", "u16": "<H", "i16": "<h",
    "u32": "<I", "i32": "<i", "u64": "<Q",
}
_SIZE = {"u8": 1, "i8": 1, "u16": 2, "i16": 2, "u32": 4, "i32": 4, "u64": 8}
_LIMITS = {
    "u8": (0, 0xFF), "i8": (-0x80, 0x7F), "u16": (0, 0xFFFF), "i16": (-0x8000, 0x7FFF),
    "u32": (0, 0xFFFFFFFF), "i32": (-0x80000000, 0x7FFFFFFF), "u64": (0, 2**64 - 1),
}


# ---------------------------------------------------------------------------
#  CRC et COBS — mêmes algorithmes que rt_crc16.c et rt_cobs.c
# ---------------------------------------------------------------------------

def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE. CRC(b"123456789") == 0x29B1."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def cobs_encode(data: bytes) -> bytes:
    out = bytearray()
    code_at = 0
    out.append(0)
    code = 1
    for b in data:
        if b == 0:
            out[code_at] = code
            code_at = len(out)
            out.append(0)
            code = 1
        else:
            out.append(b)
            code += 1
            if code == 0xFF:
                out[code_at] = code
                code_at = len(out)
                out.append(0)
                code = 1
    out[code_at] = code
    return bytes(out)


def cobs_decode(data: bytes) -> bytes | None:
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        code = data[i]
        if code == 0:
            return None
        i += 1
        for _ in range(code - 1):
            if i >= n:
                return None
            out.append(data[i])
            i += 1
        if code != 0xFF and i < n:
            out.append(0)
    return bytes(out)


# ---------------------------------------------------------------------------
#  Modèle
# ---------------------------------------------------------------------------

@dataclass
class Field:
    name: str
    type: str
    scale: float | None
    offset: float
    unit: str | None
    doc: str | None
    byte: int


@dataclass
class FrameDef:
    name: str
    id: int
    sender: str
    rate_hz: float
    impl: str
    dlc: int
    fields: list[Field] = field(default_factory=list)

    def decode(self, payload: bytes) -> dict[str, Any]:
        out: dict[str, Any] = {}
        for f in self.fields:
            raw = struct.unpack_from(_FMT[f.type], payload, f.byte)[0]
            out[f.name] = raw * f.scale + f.offset if f.scale is not None else raw
        return out

    def encode(self, values: dict[str, Any]) -> bytes:
        payload = bytearray(self.dlc)
        for f in self.fields:
            v = values.get(f.name, 0)
            if f.scale is not None:
                lo, hi = _LIMITS[f.type]
                scaled = (float(v) - f.offset) / f.scale
                scaled = max(float(lo), min(float(hi), scaled))
                raw = int(scaled + (0.5 if scaled >= 0 else -0.5))
            else:
                raw = int(v)
            struct.pack_into(_FMT[f.type], payload, f.byte, raw)
        return bytes(payload)


class Protocol:
    def __init__(self, path: Path = PROTOCOL_YAML):
        doc = yaml.safe_load(path.read_text(encoding="utf-8"))
        self.meta = doc["meta"]
        self.enums = doc["enums"]
        self.frames: dict[int, FrameDef] = {}
        self.by_name: dict[str, FrameDef] = {}

        raw_fields: dict[str, list] = {}
        for spec in doc["frames"]:
            fields_spec = spec.get("fields")
            if fields_spec is None:
                fields_spec = raw_fields[spec["like"]]
            raw_fields[spec["name"]] = fields_spec

            offset = 0
            fields: list[Field] = []
            for fs in fields_spec:
                fields.append(
                    Field(
                        name=fs["name"],
                        type=fs["type"],
                        scale=fs.get("scale"),
                        offset=fs.get("offset", 0.0),
                        unit=fs.get("unit"),
                        doc=fs.get("doc"),
                        byte=offset,
                    )
                )
                offset += _SIZE[fs["type"]]

            fd = FrameDef(
                name=spec["name"],
                id=int(spec["id"]),
                sender=spec["sender"],
                rate_hz=float(spec.get("rate_hz", 0) or 0),
                impl=spec.get("impl", "planned"),
                dlc=offset,
                fields=fields,
            )
            self.frames[fd.id] = fd
            self.by_name[fd.name] = fd

    def name(self, frame_id: int) -> str:
        fd = self.frames.get(frame_id)
        return fd.name if fd else f"INCONNU_0x{frame_id:03X}"

    # --- cadrage ----------------------------------------------------------

    def encode_frame(self, frame_id: int, payload: bytes) -> bytes:
        header = bytes([frame_id & 0xFF, ((frame_id >> 8) & 0x07) | (len(payload) << 4)])
        body = header + payload
        crc = crc16(body)
        return cobs_encode(body + bytes([crc & 0xFF, (crc >> 8) & 0xFF])) + bytes([DELIMITER])

    def encode_named(self, name: str, **values: Any) -> bytes:
        fd = self.by_name[name]
        return self.encode_frame(fd.id, fd.encode(values))


@dataclass
class DecoderStats:
    frames_ok: int = 0
    crc_errors: int = 0
    format_errors: int = 0
    overflows: int = 0


class Decoder:
    """Décodeur incrémental, même comportement que rt_framing.c."""

    def __init__(self) -> None:
        self.buf = bytearray()
        self.overflow = False
        self.stats = DecoderStats()

    def feed(self, data: bytes) -> list[tuple[int, bytes]]:
        out: list[tuple[int, bytes]] = []
        for b in data:
            frame = self._push(b)
            if frame is not None:
                out.append(frame)
        return out

    def _push(self, b: int) -> tuple[int, bytes] | None:
        if b != DELIMITER:
            if len(self.buf) < WIRE_MAX:
                self.buf.append(b)
            else:
                self.overflow = True
            return None

        raw = bytes(self.buf)
        self.buf.clear()
        was_overflow = self.overflow
        self.overflow = False

        if was_overflow:
            self.stats.overflows += 1
            return None
        if not raw:
            return None

        pkt = cobs_decode(raw)
        if pkt is None or len(pkt) < 4:
            self.stats.format_errors += 1
            return None

        body, got = pkt[:-2], pkt[-2] | (pkt[-1] << 8)
        if got != crc16(body):
            self.stats.crc_errors += 1
            return None

        dlc = body[1] >> 4
        if dlc > 8 or dlc + 2 != len(body):
            self.stats.format_errors += 1
            return None

        frame_id = body[0] | ((body[1] & 0x07) << 8)
        self.stats.frames_ok += 1
        return frame_id, body[2:]


def open_port(device: str, baud: int, timeout: float = 0.005):
    """Ouvre le port série SANS redémarrer l'ESP32.

    ⚠️ Sur une DevKitC, DTR et RTS pilotent EN (reset) et IO0 (mode de
    démarrage) à travers le circuit d'auto-reset. pyserial les assertit à
    l'ouverture : la carte redémarre, et selon l'ordre des transitions elle peut
    rester dans le bootloader ROM — muette, sans la moindre erreur côté hôte.
    Symptôme observé au banc : la liaison marche après un rebranchement
    physique, puis plus rien dès qu'un second programme rouvre le port.

    `exclusive` est là pour une raison voisine : deux programmes lisant le même
    port se partagent les octets, et chacun voit un flux tronqué qu'il signale
    comme des erreurs de format. Mieux vaut un refus net.

    Importer pyserial ici et non en tête de module : les outils qui ne touchent
    pas au port (le générateur, les tests) n'ont pas à en dépendre.
    """
    import serial   # noqa: PLC0415

    port = serial.Serial()
    port.port = device
    port.baudrate = baud
    port.timeout = timeout
    port.dtr = False
    port.rts = False
    try:
        port.exclusive = True
    except (AttributeError, ValueError):
        pass        # plateformes sans verrou exclusif
    port.open()
    return port
