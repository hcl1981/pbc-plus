#!/usr/bin/env bash
#
# Baut Rushtown für den PicoBoy Color Plus.
# Eine Stadt von oben: herumlaufen, Autos fahren, Aufträge erledigen.
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
# Das Ergebnis landet in ../dist/rushtown.uf2
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

[ "${1:-}" = clean ] && rm -rf "${HIER}/build"

pbc_head "rushtown — eine Stadt von oben: Autos, Aufträge, Polizei"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk || exit 1

pbc_cmake_build "${HIER}" "${HIER}/build" || exit 1
pbc_collect "${HIER}/build/rushtown.uf2" "rushtown.uf2" || exit 1

pbc_verify_dist "rushtown.uf2" || exit 1
pbc_hint_flash
