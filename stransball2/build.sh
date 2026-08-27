#!/usr/bin/env bash
#
# Baut stransball2 für den PicoBoy Color Plus.
# Super Transball 2 — Thrust-artig mit Traktorstrahl
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
# Das Ergebnis landet in ../dist/stransball2.uf2
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

[ "${1:-}" = clean ] && rm -rf "${HIER}/build"

pbc_head "stransball2 — Super Transball 2, Thrust-artig mit Traktorstrahl"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk || exit 1

pbc_cmake_build "${HIER}" "${HIER}/build"  || exit 1
pbc_collect "${HIER}/build/stransball2.uf2" "stransball2.uf2" || exit 1


pbc_verify_dist "stransball2.uf2" || exit 1
pbc_hint_flash
