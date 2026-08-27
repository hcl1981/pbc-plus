#!/usr/bin/env bash
#
# Baut spout für den PicoBoy Color Plus.
# Höhlenflug, der Sandstrahl zerstört den Fels
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
# Das Ergebnis landet in ../dist/spout.uf2
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

[ "${1:-}" = clean ] && rm -rf "${HIER}/build"

pbc_head "spout — Höhlenflug, der Sandstrahl zerstört den Fels"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk || exit 1

pbc_cmake_build "${HIER}" "${HIER}/build"  || exit 1
pbc_collect "${HIER}/build/spout.uf2" "spout.uf2" || exit 1


pbc_verify_dist "spout.uf2" || exit 1
pbc_hint_flash
