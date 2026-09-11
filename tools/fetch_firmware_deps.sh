#!/usr/bin/env bash
# Récupère les dépendances tierces du firmware.
#
# Une seule pour l'instant : la pile SH-2 de CEVA, épinglée sur la version
# 1.4.0. Apache 2.0, donc compatible avec la licence de ce dépôt ; voir
# CREDITS.md.
#
# Le script est idempotent et ne fait rien si tout est déjà là.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SH2_DIR="$REPO/firmware/components/retriever_imu/vendor/sh2"
SH2_TAG="v1.4.0"
SH2_URL="https://github.com/ceva-dsp/sh2.git"

if [ -f "$SH2_DIR/sh2.c" ]; then
    echo "sh2 déjà présent — rien à faire"
    exit 0
fi

if git -C "$REPO" submodule status "$SH2_DIR" >/dev/null 2>&1; then
    echo "initialisation du sous-module sh2"
    git -C "$REPO" submodule update --init --recursive "$SH2_DIR"
else
    echo "clonage de sh2 $SH2_TAG"
    git clone --depth 1 --branch "$SH2_TAG" "$SH2_URL" "$SH2_DIR"
fi

test -f "$SH2_DIR/sh2.c" || { echo "échec : sh2.c introuvable" >&2; exit 1; }
echo "sh2 prêt dans $SH2_DIR"
