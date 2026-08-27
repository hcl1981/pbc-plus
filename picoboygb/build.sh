#!/usr/bin/env bash
#
# Baut picoboygb für den PicoBoy Color Plus.
# Game-Boy-Emulator, ROMs per USB-Stick-Modus
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
# Das Ergebnis landet in ../dist/picoboygb.uf2
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

[ "${1:-}" = clean ] && rm -rf "${HIER}/build"

pbc_head "picoboygb — Game-Boy-Emulator, ROMs per USB-Stick-Modus"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk || exit 1

pbc_cmake_build "${HIER}" "${HIER}/build" -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2 || exit 1
pbc_collect "${HIER}/build/picoboygb.uf2" "picoboygb.uf2" || exit 1

pbc_verify_dist "picoboygb.uf2" -.uf2 || exit 1
pbc_hint_flash
