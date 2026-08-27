#!/usr/bin/env bash
#
# Baut Doom für den PicoBoy Color Plus.
#
#   picoboy-doom  ->  ../dist/doom-usb-mp.uf2
#
# Mehrspieler läuft über ein USB-C-Kabel zwischen zwei Geräten (usblink.c).
# Die Änderungen an der Bildausgabe, die die vollen 35 Tics je Sekunde halten,
# sind Bestandteil der Portierung — begründet in picoboy-doom/TEMPO.md.
#
# Gebaut wird das Target `doom_tiny_nost` — die Vollversion mit drei Episoden,
# WAD-Bereich ab 0x10048000. `doom_tiny` wäre die Shareware-Variante.
#
# NICHT gebaut werden doom_tiny_usb und doom_tiny_nost_usb. Deren USB_SUPPORT
# ist die USB-Tastaturunterstützung aus rp2040-doom; sie braucht einen
# USB-Host, den diese Portierung entfernt hat, weil der PicoBoy keinen
# USB-A-Anschluss besitzt (siehe picoboy-doom/src/pico/CMakeLists.txt). Die
# Targets ließen sich hier gar nicht übersetzen — `tusb.h` fehlt. Mit dem
# USB-Mehrspieler haben sie nichts zu tun.
#
# WICHTIG: Die Firmware allein spielt nichts. Die Level kommen aus einer
# WAD-Datei, die als zweite UF2 an eine eigene Flash-Adresse geht. Das
# Werkzeug dafür liegt in wad2uf2/ — siehe FLASHEN.md.
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

PROJ="${HIER}/picoboy-doom"
BDIR="${PROJ}/build-picoboy"

[ "${1:-}" = clean ] && rm -rf "${BDIR}"

pbc_head "doom — Doom mit Mehrspieler über USB-C"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk extras || exit 1

PBC_TARGET=doom_tiny_nost \
pbc_cmake_build "${PROJ}" "${BDIR}" \
    -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2 \
    -DPICO_FLASH_SIZE_BYTES=16777216 -DPICO_EXTRAS_PATH="${PICO_EXTRAS_PATH}" || exit 1

UF2="$(find "${BDIR}" -name 'doom_tiny_nost.uf2' -print -quit)"
if [ -z "${UF2}" ]; then
    pbc_err "doom_tiny_nost.uf2 wurde nicht erzeugt."
    echo "  Vollständige Ausgabe: ${BDIR}.build.log" >&2
    exit 1
fi

pbc_collect "${UF2}" "doom-usb-mp.uf2" || exit 1
pbc_verify_dist doom-usb-mp.uf2 || exit 1

cat <<'EOF'

  Doom braucht zusätzlich eine WAD an einer eigenen Flash-Adresse.
  Aus der eigenen WAD eine flashfertige UF2 machen:

      wad2uf2/bin/whd_gen-linux-x86_64  DOOM.WAD

  (unter Windows die .exe doppelklicken oder die WAD darauf ziehen)

  Danach beide Dateien flashen — Reihenfolge und Adressen stehen
  in FLASHEN.md.
EOF
pbc_hint_flash
