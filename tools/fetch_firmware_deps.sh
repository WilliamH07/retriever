#!/usr/bin/env bash
# Récupère les dépendances tierces du firmware.
#
# Une seule pour l'instant : la pile SH-2 de CEVA, épinglée sur la version
# 1.4.0. Apache 2.0, donc compatible avec la licence de ce dépôt ; voir
# CREDITS.md.
#
# Le script est idempotent, et il VÉRIFIE son propre résultat. C'est délibéré :
# `git submodule update --init` ne fait rien, et ne dit rien, quand le lien vers
# le sous-module a disparu de l'index — une erreur silencieuse qui ne se
# manifeste qu'au premier `idf.py build`, plusieurs minutes plus tard et
# plusieurs couches plus bas. Le repli par clonage direct existe pour ça.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SH2_DIR="$REPO/firmware/components/retriever_imu/vendor/sh2"
SH2_TAG="v1.4.0"
SH2_URL="https://github.com/ceva-dsp/sh2.git"

if [ -f "$SH2_DIR/sh2.c" ]; then
    echo "sh2 déjà présent — rien à faire"
    exit 0
fi

# 1. La voie normale : le sous-module est enregistré, on l'initialise.
if git -C "$REPO" ls-files --stage -- "$SH2_DIR" 2>/dev/null | grep -q '^160000'; then
    echo "initialisation du sous-module sh2"
    git -C "$REPO" submodule update --init --recursive -- "$SH2_DIR" || true
fi

# 2. Le repli : le lien manque, ou l'initialisation n'a rien produit.
if [ ! -f "$SH2_DIR/sh2.c" ]; then
    echo "sous-module indisponible — clonage direct de sh2 $SH2_TAG"
    rm -rf "$SH2_DIR"
    mkdir -p "$(dirname "$SH2_DIR")"
    git clone --quiet --depth 1 --branch "$SH2_TAG" "$SH2_URL" "$SH2_DIR"
fi

# 3. On ne rend pas la main sans avoir vérifié.
if [ ! -f "$SH2_DIR/sh2.c" ]; then
    echo "échec : sh2.c introuvable dans $SH2_DIR" >&2
    echo "Vérifier l'accès réseau à github.com, puis relancer." >&2
    exit 1
fi

echo "sh2 prêt dans $SH2_DIR"

# 4. Si le lien manque dans l'index, le dire — le build marchera, mais le
#    prochain clone du dépôt repartira sans la bibliothèque.
if ! git -C "$REPO" ls-files --stage -- "$SH2_DIR" 2>/dev/null | grep -q '^160000'; then
    cat >&2 <<'WARN'

⚠️  Le sous-module n'est PAS enregistré dans l'index git.
    La compilation va marcher ici, mais un clone neuf du dépôt — ou la CI —
    repartira sans sh2. Pour l'enregistrer une bonne fois :

      git add .gitmodules firmware/components/retriever_imu/vendor/sh2
      git commit -m "Enregistre le sous-module sh2"

WARN
fi
