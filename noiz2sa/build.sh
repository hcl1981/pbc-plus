#!/usr/bin/env bash
#
# Baut noiz2sa für den PicoBoy Color Plus.
# abstrakter Kugelhagel-Schütze von Kenta Cho
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
# Das Ergebnis landet in ../dist/noiz2sa.uf2
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

[ "${1:-}" = clean ] && rm -rf "${HIER}/build"

pbc_head "noiz2sa — abstrakter Kugelhagel-Schütze von Kenta Cho"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk || exit 1

pbc_cmake_build "${HIER}" "${HIER}/build"  || exit 1
pbc_collect "${HIER}/build/noiz2sa.uf2" "noiz2sa.uf2" || exit 1

# Testfirmware für Display, Tasten, Ton und Bildzeit — hilft, wenn das Gerät
# schwarz bleibt und man erst die Hardware ausschließen will.
[ -f "${HIER}/build/noiz2sa_selftest.uf2" ] && pbc_collect "${HIER}/build/noiz2sa_selftest.uf2" "noiz2sa_selftest.uf2" selbsttest

pbc_verify_dist "noiz2sa.uf2" noiz2sa_selftest.uf2 || exit 1
pbc_hint_flash
