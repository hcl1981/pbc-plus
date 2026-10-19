#!/usr/bin/env bash
#
# Baut sms für den PicoBoy Color Plus.
# Master-System- und Game-Gear-Emulator, ROMs per USB-Stick-Modus
#
# Aufruf:   ./build.sh                  bauen
#           ./build.sh clean            Bauordner löschen und neu bauen
#           ./build.sh -DCPU_MHZ=336    CMake-Optionen durchreichen
#
# Das Ergebnis landet in ../dist/sms.uf2
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

if [ "${1:-}" = clean ]; then
    rm -rf "${HIER}/build"
    shift
fi

pbc_head "sms — Master-System- und Game-Gear-Emulator, ROMs per USB-Stick-Modus"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk || exit 1

# Release statt der MinSizeRel-Vorgabe von pbc_cmake_build (die spätere
# Angabe gewinnt): der Emulator braucht jede Taktrunde, und Platz ist im
# ersten MB des Flash reichlich.
pbc_cmake_build "${HIER}" "${HIER}/build" -DCMAKE_BUILD_TYPE=Release \
    -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2 "$@" || exit 1
pbc_collect "${HIER}/build/pbc_sms.uf2" "sms.uf2" || exit 1

pbc_verify_dist "sms.uf2" || exit 1
pbc_hint_flash
