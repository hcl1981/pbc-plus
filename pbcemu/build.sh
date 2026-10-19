#!/usr/bin/env bash
#
# Baut PBCemu — alle Emulatoren hinter einem Starter, als eine Firmware.
#
# Aufruf:   ./build.sh              bauen
#           ./build.sh clean        Bauordner löschen und neu bauen
#
# Ablauf: Jeder Emulator wird für den Verbund gebaut (eigener Bauordner
# build-multi/, gelinkt an seine Flash-Adresse, siehe common/pbc_multi.cmake).
# Die Adressen ergeben sich der Reihe nach aus den Größen, auf 4 KB gerundet,
# beginnend hinter den 256 KB des Starters. Dann wird der Starter mit der
# Adressliste (build/gen/pbc_apps.h) gebaut und alles zu ../dist/pbcemu.uf2 verpackt.
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

LAUNCHER_SIZE=0x40000
APPS_END=0x200000                  # ab hier: ROM-Bereich (5 MB), ab 0x700000 FatFs
# Reihenfolge im Flash; Name im Starter (APP_*), Projektordner, Programmname
APPS="MD:megadrive:pbc_megadrive SMS:sms:pbc_sms NES:nes:pbc_nes PCE:pce:pbc_pce GBC:gbc:pbc_gbc C64:c64:pbc_c64"

if [ "${1:-}" = clean ]; then
    rm -rf "${HIER}/build"
    for a in ${APPS}; do rm -rf "${HIER}/../$(echo "$a" | cut -d: -f2)/build-multi"; done
    shift
fi

pbc_head "PBCemu — Starter mit allen Emulatoren"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk || exit 1

mkdir -p "${HIER}/build/gen"
APPS_H="${HIER}/build/gen/pbc_apps.h"
{
    echo "/* pbc_apps.h -- written by pbcemu/build.sh */"
    echo "#ifndef PBC_APPS_H"
    echo "#define PBC_APPS_H"
} > "${APPS_H}.tmp"

offset=$((LAUNCHER_SIZE))
PACK=()
for a in ${APPS}; do
    key=$(echo "$a" | cut -d: -f1); dir=$(echo "$a" | cut -d: -f2); prog=$(echo "$a" | cut -d: -f3)
    proj="${HIER}/../${dir}"
    room=$((APPS_END - offset))
    pbc_info "${dir}: ab $(printf '0x%06x' ${offset})"
    # Release wie die Einzel-Firmwares; ohne fps-Anzeige
    pbc_cmake_build "${proj}" "${proj}/build-multi" -DCMAKE_BUILD_TYPE=Release \
        -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2 -DSHOW_FPS=0 \
        -DPBC_APP_OFFSET=$(printf '0x%x' ${offset}) -DPBC_APP_SIZE=$(printf '0x%x' ${room}) \
        || exit 1
    bin="${proj}/build-multi/${prog}.bin"
    [ -f "${bin}" ] || { pbc_err "${bin} fehlt"; exit 1; }
    # Linked to the right address? The reset vector must point just behind
    # the vector table at this offset -- an image linked elsewhere would
    # jump into nowhere.
    if ! python3 - "${bin}" "${offset}" <<'PY'
import struct, sys
sp, reset = struct.unpack('<2I', open(sys.argv[1], 'rb').read(8))
base = 0x10000000 + int(sys.argv[2])
sys.exit(0 if base < reset < base + 0x400 else 1)
PY
    then
        pbc_err "${dir}: nicht an $(printf '0x%x' ${offset}) gelinkt (alter Bauordner?) -- ./build.sh clean"
        exit 1
    fi
    size=$(stat -c %s "${bin}")
    echo "#define PBC_APP_${key} $(printf '0x%06xu' ${offset})" >> "${APPS_H}.tmp"
    PACK+=("${bin}@$(printf '0x%x' ${offset})")
    pbc_ok "${dir}: $((size / 1024)) KB"
    offset=$(( (offset + size + 0xFFF) & ~0xFFF ))
done
if [ ${offset} -gt $((APPS_END)) ]; then
    pbc_err "Die Emulatoren brauchen $(printf '0x%x' ${offset}) Byte Flash, Platz ist bis $(printf '0x%x' ${APPS_END})."
    exit 1
fi
echo "#endif" >> "${APPS_H}.tmp"
mv "${APPS_H}.tmp" "${APPS_H}"
pbc_ok "Emulatoren belegen $(( (offset - LAUNCHER_SIZE) / 1024 )) KB, frei bis 2 MB: $(( (APPS_END - offset) / 1024 )) KB"

pbc_cmake_build "${HIER}" "${HIER}/build" -DCMAKE_BUILD_TYPE=Release \
    -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2 \
    -DPBC_APPS_DIR="${HIER}/build/gen" -DPBC_LAUNCHER_SIZE=${LAUNCHER_SIZE} || exit 1

mkdir -p "${PBC_DIST}"
python3 "${HIER}/tools/pack_uf2.py" "${PBC_DIST}/pbcemu.uf2" \
    "${HIER}/build/pbcemu.uf2" "${PACK[@]}" || exit 1
pbc_ok "dist/pbcemu.uf2"
pbc_verify_dist "pbcemu.uf2" || exit 1
pbc_hint_flash
