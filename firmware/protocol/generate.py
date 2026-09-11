#!/usr/bin/env python3
"""Générateur du protocole Retriever.

Lit protocol.yaml et produit les en-têtes C et C++, plus une table de
documentation. Les fichiers générés sont versionnés dans le dépôt : la CI
vérifie qu'ils correspondent au YAML (tools/check_protocol_sync.py).

    python3 firmware/protocol/generate.py            # écrit les fichiers
    python3 firmware/protocol/generate.py --check    # code de retour 1 si divergence
    python3 firmware/protocol/generate.py --hash     # imprime le hash et sort

Le hash est calculé sur le MODÈLE SÉMANTIQUE, pas sur le texte du fichier :
corriger une faute dans un commentaire ne force pas à reflasher les trois
nœuds, alors que changer une échelle ou un identifiant le force.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

import yaml

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent

OUT_C = HERE / "generated" / "retriever_protocol.h"
# Copie dans le paquet ROS 2 : colcon ne sait pas sortir du répertoire du paquet,
# et un lien symbolique ne survit pas à toutes les chaînes de build. C'est le même
# fichier, écrit deux fois par le générateur, vérifié par la CI. Il n'est jamais
# édité à la main, donc les deux copies ne peuvent pas diverger.
OUT_C_ROS = (
    REPO
    / "ros2_ws"
    / "src"
    / "retriever_protocol"
    / "include"
    / "retriever_protocol"
    / "retriever_protocol.h"
)
OUT_CPP = (
    REPO
    / "ros2_ws"
    / "src"
    / "retriever_protocol"
    / "include"
    / "retriever_protocol"
    / "protocol.hpp"
)
OUT_DOC = REPO / "docs" / "architecture" / "generated" / "protocol-tables.md"

# -----------------------------------------------------------------------------
#  Modèle
# -----------------------------------------------------------------------------

SIZES = {"u8": 1, "i8": 1, "u16": 2, "i16": 2, "u32": 4, "i32": 4, "u64": 8}

C_RAW = {
    "u8": "uint8_t",
    "i8": "int8_t",
    "u16": "uint16_t",
    "i16": "int16_t",
    "u32": "uint32_t",
    "i32": "int32_t",
    "u64": "uint64_t",
}

LIMITS = {
    "u8": (0, 0xFF),
    "i8": (-0x80, 0x7F),
    "u16": (0, 0xFFFF),
    "i16": (-0x8000, 0x7FFF),
    "u32": (0, 0xFFFFFFFF),
    "i32": (-0x80000000, 0x7FFFFFFF),
    "u64": (0, 0xFFFFFFFFFFFFFFFF),
}


class Field:
    def __init__(self, spec: dict, offset: int):
        self.name = spec["name"]
        self.type = spec["type"]
        if self.type not in SIZES:
            raise ValueError(f"type inconnu: {self.type}")
        self.scale = spec.get("scale")
        self.offset_phys = spec.get("offset", 0.0)
        self.unit = spec.get("unit")
        self.enum = spec.get("enum")
        self.doc = spec.get("doc")
        self.byte = offset
        self.size = SIZES[self.type]

    @property
    def is_scaled(self) -> bool:
        return self.scale is not None

    def c_type(self) -> str:
        return "float" if self.is_scaled else C_RAW[self.type]

    def semantic(self) -> dict:
        return {
            "name": self.name,
            "type": self.type,
            "byte": self.byte,
            "scale": self.scale,
            "offset": self.offset_phys,
            "enum": self.enum,
        }


class Frame:
    def __init__(self, spec: dict, by_name: dict):
        self.name = spec["name"]
        self.id = int(spec["id"])
        self.sender = spec["sender"]
        self.rate_hz = spec.get("rate_hz", 0)
        self.impl = spec.get("impl", "planned")
        self.doc = spec.get("doc")

        raw_fields = spec.get("fields")
        if raw_fields is None:
            parent = spec.get("like")
            if parent is None:
                raise ValueError(f"{self.name}: ni 'fields' ni 'like'")
            if parent not in by_name:
                raise ValueError(f"{self.name}: 'like: {parent}' inconnu ou défini plus bas")
            raw_fields = by_name[parent]
            if self.doc is None:
                self.doc = f"Même disposition que {parent}."
        self.raw_fields = raw_fields

        self.fields: list[Field] = []
        off = 0
        for fs in raw_fields:
            f = Field(fs, off)
            off += f.size
            self.fields.append(f)
        self.dlc = off
        if self.dlc > 8:
            raise ValueError(f"{self.name}: {self.dlc} octets, la limite est 8")

    def semantic(self) -> dict:
        return {
            "name": self.name,
            "id": self.id,
            "sender": self.sender,
            "dlc": self.dlc,
            "fields": [f.semantic() for f in self.fields],
        }


class Model:
    def __init__(self, doc: dict):
        self.meta = doc["meta"]
        self.enums = doc["enums"]
        by_name: dict[str, list] = {}
        self.frames: list[Frame] = []
        for spec in doc["frames"]:
            fr = Frame(spec, by_name)
            by_name[fr.name] = fr.raw_fields
            self.frames.append(fr)

        seen: dict[int, str] = {}
        for fr in self.frames:
            if fr.id in seen:
                raise ValueError(
                    f"identifiant 0x{fr.id:03X} utilisé par {seen[fr.id]} et {fr.name}"
                )
            seen[fr.id] = fr.name
            if fr.id >= (1 << int(self.meta["id_bits"])):
                raise ValueError(f"{fr.name}: 0x{fr.id:03X} dépasse {self.meta['id_bits']} bits")

    def semantic(self) -> dict:
        return {
            "version": self.meta["version"],
            "endianness": self.meta["endianness"],
            "id_bits": self.meta["id_bits"],
            "max_payload": self.meta["max_payload"],
            "enums": {
                k: {n: int(v) for n, v in e["values"].items()} for k, e in self.enums.items()
            },
            "frames": [f.semantic() for f in self.frames],
        }

    def hash32(self) -> int:
        blob = json.dumps(self.semantic(), sort_keys=True, separators=(",", ":")).encode()
        return int.from_bytes(hashlib.sha256(blob).digest()[:4], "big")

    def hash_hex(self) -> str:
        blob = json.dumps(self.semantic(), sort_keys=True, separators=(",", ":")).encode()
        return hashlib.sha256(blob).hexdigest()


# -----------------------------------------------------------------------------
#  Fabrique d'expressions
# -----------------------------------------------------------------------------


def enum_prefix(name: str) -> str:
    return "RT_" + name.upper()


def pack_expr(f: Field, src: str) -> tuple[str, str]:
    """Retourne (déclaration de la variable brute, nom de la variable)."""
    var = f"raw_{f.name}"
    lo, hi = LIMITS[f.type]
    if f.is_scaled:
        body = (
            f"    double v_{f.name} = ((double)({src}) - ({f.offset_phys!r})) / ({f.scale!r});\n"
            f"    if (v_{f.name} > {hi}.0) v_{f.name} = {hi}.0;\n"
            f"    if (v_{f.name} < {lo}.0) v_{f.name} = {lo}.0;\n"
            f"    {C_RAW[f.type]} {var} = ({C_RAW[f.type]})rt_lround(v_{f.name});\n"
        )
    else:
        body = f"    {C_RAW[f.type]} {var} = ({C_RAW[f.type]})({src});\n"
    return body, var


def unpack_expr(f: Field, dst: str) -> str:
    getter = f"rt_get_{f.type}(f->data + {f.byte})"
    if f.is_scaled:
        return f"    {dst} = (float)((double)({getter}) * ({f.scale!r}) + ({f.offset_phys!r}));\n"
    return f"    {dst} = {getter};\n"


# -----------------------------------------------------------------------------
#  En-tête C
# -----------------------------------------------------------------------------

C_PRELUDE = """\
/* ===========================================================================
 *  retriever_protocol.h — GÉNÉRÉ, NE PAS MODIFIER À LA MAIN
 *
 *  Source      : firmware/protocol/protocol.yaml
 *  Générateur  : firmware/protocol/generate.py
 *  Version     : {version}
 *  Hash        : 0x{hash32:08X}  ({hash_hex})
 *
 *  Toute modification doit se faire dans le YAML puis passer par le
 *  générateur. La CI (tools/check_protocol_sync.py) échoue sinon.
 *
 *  Représentation : entiers petit-boutiste, charge utile de {max_payload} octets
 *  au plus. Identique en CAN 2.0A et sur le transport série : la couche de
 *  liaison ne voit qu'une trame canonique rt_frame_t.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RETRIEVER_PROTOCOL_H
#define RETRIEVER_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {{
#endif

#define RT_PROTOCOL_VERSION   "{version}"
#define RT_PROTOCOL_HASH      0x{hash32:08X}u
#define RT_PROTOCOL_HASH_FULL "{hash_hex}"
#define RT_MAX_PAYLOAD        {max_payload}u
#define RT_ID_BITS            {id_bits}u
#define RT_ID_MASK            0x{id_mask:03X}u

/* Trame canonique. Le seul type que voit le code applicatif : le transport
 * (CAN ou série) est en dessous et n'apparaît nulle part au-dessus. */
typedef struct {{
    uint16_t id;                    /* identifiant sur RT_ID_BITS bits */
    uint8_t  dlc;                   /* 0..RT_MAX_PAYLOAD */
    uint8_t  data[RT_MAX_PAYLOAD];
}} rt_frame_t;

/* --- Accès petit-boutiste, sans hypothèse d'alignement --------------------- */

static inline long rt_lround(double v)
{{
    return (long)(v >= 0.0 ? v + 0.5 : v - 0.5);
}}

static inline void rt_put_u8(uint8_t *p, uint8_t v)   {{ p[0] = v; }}
static inline void rt_put_i8(uint8_t *p, int8_t v)    {{ p[0] = (uint8_t)v; }}
static inline void rt_put_u16(uint8_t *p, uint16_t v) {{ p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8); }}
static inline void rt_put_i16(uint8_t *p, int16_t v)  {{ rt_put_u16(p, (uint16_t)v); }}
static inline void rt_put_u32(uint8_t *p, uint32_t v)
{{
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}}
static inline void rt_put_i32(uint8_t *p, int32_t v)  {{ rt_put_u32(p, (uint32_t)v); }}
static inline void rt_put_u64(uint8_t *p, uint64_t v)
{{
    rt_put_u32(p, (uint32_t)(v & 0xFFFFFFFFu));
    rt_put_u32(p + 4, (uint32_t)(v >> 32));
}}

static inline uint8_t  rt_get_u8(const uint8_t *p)  {{ return p[0]; }}
static inline int8_t   rt_get_i8(const uint8_t *p)  {{ return (int8_t)p[0]; }}
static inline uint16_t rt_get_u16(const uint8_t *p) {{ return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }}
static inline int16_t  rt_get_i16(const uint8_t *p) {{ return (int16_t)rt_get_u16(p); }}
static inline uint32_t rt_get_u32(const uint8_t *p)
{{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}}
static inline int32_t  rt_get_i32(const uint8_t *p) {{ return (int32_t)rt_get_u32(p); }}
static inline uint64_t rt_get_u64(const uint8_t *p)
{{
    return (uint64_t)rt_get_u32(p) | ((uint64_t)rt_get_u32(p + 4) << 32);
}}
"""

C_EPILOGUE = """
#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* RETRIEVER_PROTOCOL_H */
"""


def gen_c(m: Model) -> str:
    out = [
        C_PRELUDE.format(
            version=m.meta["version"],
            hash32=m.hash32(),
            hash_hex=m.hash_hex(),
            max_payload=m.meta["max_payload"],
            id_bits=m.meta["id_bits"],
            id_mask=(1 << int(m.meta["id_bits"])) - 1,
        )
    ]

    out.append("\n/* --- Énumérations ---------------------------------------------------------- */\n")
    for ename, espec in m.enums.items():
        if espec.get("doc"):
            out.append(f"\n/* {' '.join(espec['doc'].split())} */\n")
        if espec.get("bitfield"):
            for vname, val in espec["values"].items():
                out.append(f"#define {enum_prefix(ename)}_{vname} 0x{int(val):02X}u\n")
            continue
        out.append(f"typedef enum {{\n")
        for vname, val in espec["values"].items():
            out.append(f"    {enum_prefix(ename)}_{vname} = {int(val)},\n")
        out.append(f"}} rt_{ename}_e;\n")

    out.append("\n/* --- Identifiants ---------------------------------------------------------- */\n")
    for fr in m.frames:
        out.append(f"#define RT_ID_{fr.name:<24} 0x{fr.id:03X}u\n")
    out.append("\n")
    for fr in m.frames:
        out.append(f"#define RT_DLC_{fr.name:<23} {fr.dlc}u\n")

    out.append("\n/* --- Trames ---------------------------------------------------------------- */\n")
    for fr in m.frames:
        lname = fr.name.lower()
        out.append(f"\n/* {fr.name}  id 0x{fr.id:03X}  dlc {fr.dlc}  émetteur {fr.sender}")
        if fr.rate_hz:
            out.append(f"  {fr.rate_hz} Hz")
        out.append(f"  [{fr.impl}]\n")
        if fr.doc:
            for line in _wrap(" ".join(fr.doc.split()), 74):
                out.append(f" * {line}\n")
        out.append(" *\n")
        for f in fr.fields:
            unit = f" [{f.unit}]" if f.unit else ""
            extra = f"  {' '.join(f.doc.split())}" if f.doc else ""
            out.append(f" *   @{f.byte} {f.name}: {f.type}{unit}{extra}\n")
        out.append(" */\n")

        out.append(f"typedef struct {{\n")
        for f in fr.fields:
            out.append(f"    {f.c_type():<8} {f.name};\n")
        out.append(f"}} rt_{lname}_t;\n\n")

        # pack
        out.append(f"static inline void rt_{lname}_pack(const rt_{lname}_t *m, rt_frame_t *f)\n{{\n")
        out.append(f"    f->id = RT_ID_{fr.name};\n")
        out.append(f"    f->dlc = RT_DLC_{fr.name};\n")
        out.append(f"    memset(f->data, 0, sizeof(f->data));\n")
        for f in fr.fields:
            body, var = pack_expr(f, f"m->{f.name}")
            out.append(body)
            out.append(f"    rt_put_{f.type}(f->data + {f.byte}, {var});\n")
        out.append("}\n\n")

        # unpack
        out.append(f"static inline bool rt_{lname}_unpack(const rt_frame_t *f, rt_{lname}_t *m)\n{{\n")
        out.append(f"    if (f->id != RT_ID_{fr.name} || f->dlc < RT_DLC_{fr.name}) return false;\n")
        for f in fr.fields:
            out.append(unpack_expr(f, f"m->{f.name}"))
        out.append("    return true;\n}\n")

    # table de noms, utile en débogage et pour les outils
    out.append("\n/* --- Table des trames (débogage et outils) --------------------------------- */\n")
    out.append("typedef struct { uint16_t id; uint8_t dlc; const char *name; } rt_frame_info_t;\n\n")
    out.append("static const rt_frame_info_t rt_frame_table[] = {\n")
    for fr in m.frames:
        out.append(f'    {{ 0x{fr.id:03X}u, {fr.dlc}u, "{fr.name}" }},\n')
    out.append("};\n")
    out.append(f"#define RT_FRAME_COUNT {len(m.frames)}u\n\n")
    out.append(
        "static inline const char *rt_frame_name(uint16_t id)\n"
        "{\n"
        "    for (unsigned i = 0; i < RT_FRAME_COUNT; ++i)\n"
        "        if (rt_frame_table[i].id == id) return rt_frame_table[i].name;\n"
        '    return "UNKNOWN";\n'
        "}\n"
    )

    out.append(C_EPILOGUE)
    return "".join(out)


# -----------------------------------------------------------------------------
#  En-tête C++
# -----------------------------------------------------------------------------

CPP_PRELUDE = """\
// ===========================================================================
//  protocol.hpp — GÉNÉRÉ, NE PAS MODIFIER À LA MAIN
//
//  Source     : firmware/protocol/protocol.yaml
//  Générateur : firmware/protocol/generate.py
//  Version    : {version}
//  Hash       : 0x{hash32:08X}
//
//  Enveloppe C++17 au-dessus de l'en-tête C. Le code de sérialisation n'est
//  PAS dupliqué : ce fichier inclut retriever_protocol.h, exactement le même
//  que celui compilé dans le firmware. Un désaccord de sérialisation entre le
//  robot et le calculateur est donc structurellement impossible.
//
//  Copyright (c) 2026 William Hanczyk — Apache License 2.0
// ===========================================================================

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "retriever_protocol/retriever_protocol.h"

namespace retriever::protocol
{{

inline constexpr std::string_view kVersion = RT_PROTOCOL_VERSION;
inline constexpr std::uint32_t    kHash = RT_PROTOCOL_HASH;
inline constexpr std::size_t      kMaxPayload = RT_MAX_PAYLOAD;

using Frame = ::rt_frame_t;

/// Nom lisible d'un identifiant, ou "UNKNOWN".
inline std::string_view frame_name(std::uint16_t id) {{ return ::rt_frame_name(id); }}

/// Longueur attendue d'une trame, ou nullopt si l'identifiant est inconnu.
inline std::optional<std::uint8_t> expected_dlc(std::uint16_t id)
{{
    for (unsigned i = 0; i < RT_FRAME_COUNT; ++i) {{
        if (rt_frame_table[i].id == id) return rt_frame_table[i].dlc;
    }}
    return std::nullopt;
}}
"""

CPP_EPILOGUE = """
}  // namespace retriever::protocol
"""


def gen_cpp(m: Model) -> str:
    out = [
        CPP_PRELUDE.format(
            version=m.meta["version"],
            hash32=m.hash32(),
        )
    ]
    out.append("\n// --- Énumérations ---------------------------------------------------------\n")
    out.append("namespace enums\n{\n")
    for ename in m.enums:
        if m.enums[ename].get("bitfield"):
            continue
        out.append(f"using {_camel(ename)} = ::rt_{ename}_e;\n")
    out.append("}  // namespace enums\n")
    for fr in m.frames:
        lname = fr.name.lower()
        cname = _camel(fr.name)
        out.append(f"\n// {fr.name} — id 0x{fr.id:03X}, dlc {fr.dlc}, émetteur {fr.sender} [{fr.impl}]\n")
        out.append(f"using {cname} = ::rt_{lname}_t;\n")
        out.append(f"inline constexpr std::uint16_t k{cname}Id = RT_ID_{fr.name};\n")
        out.append(f"inline constexpr std::uint8_t  k{cname}Dlc = RT_DLC_{fr.name};\n")
        out.append(f"inline Frame pack(const {cname} & m)\n{{\n")
        out.append(f"    Frame f{{}};\n    ::rt_{lname}_pack(&m, &f);\n    return f;\n}}\n")
        out.append(f"inline std::optional<{cname}> unpack_{lname}(const Frame & f)\n{{\n")
        out.append(f"    {cname} m{{}};\n")
        out.append(f"    if (!::rt_{lname}_unpack(&f, &m)) return std::nullopt;\n")
        out.append("    return m;\n}\n")
    out.append(CPP_EPILOGUE)
    return "".join(out)


# -----------------------------------------------------------------------------
#  Documentation
# -----------------------------------------------------------------------------


def gen_doc(m: Model) -> str:
    out = [
        "<!-- GÉNÉRÉ depuis firmware/protocol/protocol.yaml — ne pas modifier -->\n\n",
        "# Table des trames — protocole Retriever\n\n",
        f"Version **{m.meta['version']}** · hash `0x{m.hash32():08X}` · "
        f"charge utile ≤ {m.meta['max_payload']} octets · entiers petit-boutiste.\n\n",
        "| ID | Trame | Émetteur | DLC | Hz | État | Contenu |\n",
        "|---|---|---|:-:|:-:|:-:|---|\n",
    ]
    for fr in m.frames:
        fields = ", ".join(
            f"{f.name}:{f.type}" + (f"×{f.scale}" if f.is_scaled else "") for f in fr.fields
        )
        rate = f"{fr.rate_hz:g}" if fr.rate_hz else "évt"
        out.append(
            f"| `0x{fr.id:03X}` | `{fr.name}` | {fr.sender} | {fr.dlc} | {rate} | {fr.impl} | {fields} |\n"
        )

    out.append("\n## Charge du lien\n\n")
    total_frames = sum(fr.rate_hz for fr in m.frames if fr.rate_hz)
    can_bits = sum((fr.rate_hz or 0) * (47 + 8 * fr.dlc + 3) for fr in m.frames)
    ser_bytes = sum((fr.rate_hz or 0) * (fr.dlc + 6) for fr in m.frames)
    out.append(
        f"Calculé sur les cadences déclarées ci-dessus, hors trames événementielles.\n\n"
        f"- **{total_frames:.1f} trames/s** au total.\n"
        f"- **CAN 500 kbit/s** : ≈ {can_bits / 1000:.1f} kbit/s, soit **{can_bits / 500000 * 100:.1f} %** "
        f"du bus (bourrage de bits non compté, majorer d'environ 15 %).\n"
        f"- **Série** : ≈ {ser_bytes / 1000:.1f} ko/s de charge utile encadrée. "
        f"À 921 600 bauds 8N1 (92 160 o/s) → **{ser_bytes / 92160 * 100:.1f} %**. "
        f"À 115 200 bauds → **{ser_bytes / 11520 * 100:.1f} %**.\n\n"
        "Le second chiffre est la raison pour laquelle le banc tourne à 921 600 et "
        "non à 115 200 : voir §AC.3.\n"
    )
    return "".join(out)


# -----------------------------------------------------------------------------


def _camel(s: str) -> str:
    return "".join(p.capitalize() for p in s.split("_"))


def _wrap(text: str, width: int) -> list[str]:
    words, lines, cur = text.split(), [], ""
    for w in words:
        if cur and len(cur) + 1 + len(w) > width:
            lines.append(cur)
            cur = w
        else:
            cur = f"{cur} {w}" if cur else w
    if cur:
        lines.append(cur)
    return lines


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true", help="ne rien écrire, signaler les divergences")
    ap.add_argument("--hash", action="store_true", help="imprimer le hash et sortir")
    args = ap.parse_args()

    doc = yaml.safe_load((HERE / "protocol.yaml").read_text(encoding="utf-8"))
    m = Model(doc)

    if args.hash:
        print(f"0x{m.hash32():08X}  {m.hash_hex()}")
        return 0

    c_header = gen_c(m)
    targets = {
        OUT_C: c_header,
        OUT_C_ROS: c_header,
        OUT_CPP: gen_cpp(m),
        OUT_DOC: gen_doc(m),
    }

    stale = []
    for path, content in targets.items():
        current = path.read_text(encoding="utf-8") if path.exists() else None
        if current == content:
            continue
        stale.append(path)
        if not args.check:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")

    if args.check:
        if stale:
            print("Fichiers générés obsolètes :", file=sys.stderr)
            for p in stale:
                print(f"  {p.relative_to(REPO)}", file=sys.stderr)
            print(
                "\nRelancer : python3 firmware/protocol/generate.py",
                file=sys.stderr,
            )
            return 1
        print(f"protocole à jour — hash 0x{m.hash32():08X}")
        return 0

    for p in targets:
        print(f"écrit  {p.relative_to(REPO)}")
    print(f"hash   0x{m.hash32():08X}")
    print(f"trames {len(m.frames)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
