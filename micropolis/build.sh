#!/usr/bin/env bash
#
# Baut micropolis für den PicoBoy Color Plus.
# Micropolis, der quelloffene SimCity-1-Kern
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
# Das Ergebnis landet in ../dist/micropolis.uf2
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

[ "${1:-}" = clean ] && rm -rf "${HIER}/build"

pbc_head "micropolis — der quelloffene SimCity-1-Kern"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk || exit 1

pbc_cmake_build "${HIER}" "${HIER}/build" -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2 || exit 1
pbc_collect "${HIER}/build/micropolis.uf2" "micropolis.uf2" || exit 1

pbc_verify_dist "micropolis.uf2" -.uf2 || exit 1
pbc_hint_flash
