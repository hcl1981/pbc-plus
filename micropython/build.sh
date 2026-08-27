#!/usr/bin/env bash
#
# Baut die MicroPython-Firmware für den PicoBoy Color Plus.
#
# In diesem Ordner liegt nur die Board-Portierung `PBC_PLUS/` — 38 Dateien.
# MicroPython selbst ist unverändertes Upstream und wird deshalb nicht
# mitgeliefert, sondern beim ersten Lauf geholt (rund 500 MB samt
# Untermodulen, nach ~/.cache/pbcp-sdks/micropython). Das hält dieses Repo
# klein und macht sofort sichtbar, was an der Portierung wirklich eigen ist.
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
# Ergebnis: ../dist/micropython-PBC_PLUS.uf2
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

MPY="${PBC_SDK_CACHE}/micropython"
RP2="${MPY}/ports/rp2"
BOARD=PBC_PLUS

pbc_head "micropython — Firmware für das Board ${BOARD}"
pbc_need cmake git python3 make arm-none-eabi-gcc gcc || exit 1

# ---------------------------------------------------------------------------
# MicroPython besorgen
# ---------------------------------------------------------------------------
mkdir -p "${PBC_SDK_CACHE}"
if [ -n "${MICROPYTHON_PATH:-}" ] && [ -f "${MICROPYTHON_PATH}/ports/rp2/Makefile" ]; then
    MPY="${MICROPYTHON_PATH}"; RP2="${MPY}/ports/rp2"
    pbc_ok "benutze vorgegebenes MICROPYTHON_PATH=${MPY}"
else
    pbc_clone https://github.com/micropython/micropython.git "${PBC_MPY_TAG}" \
              "${MPY}" "MicroPython ${PBC_MPY_TAG}" --ohne-submodule || exit 1
fi

if [ ! -f "${RP2}/Makefile" ]; then
    pbc_err "In ${MPY} steckt kein rp2-Port. Der Ordner scheint unvollständig."
    echo "  Abhilfe: einmal löschen und neu holen lassen —" >&2
    echo "      rm -rf ${MPY} && ./build.sh" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Board-Portierung einhängen
# ---------------------------------------------------------------------------
# Kopiert statt verlinkt: das Bausystem legt Erzeugnisse im Boardordner ab,
# und die sollen nicht in diesem Repo landen.
pbc_info "Board-Portierung nach ${RP2}/boards/${BOARD} spiegeln"
rm -rf "${RP2}/boards/${BOARD}"
cp -a "${HIER}/${BOARD}" "${RP2}/boards/${BOARD}" || exit 1

[ "${1:-}" = clean ] && rm -rf "${RP2}/build-${BOARD}"

# ---------------------------------------------------------------------------
# Bauen
# ---------------------------------------------------------------------------
pbc_info "mpy-cross bauen (der Übersetzer für vorkompilierte Python-Module)"
if ! make -C "${MPY}/mpy-cross" -j"${PBC_JOBS}" >"${MPY}/mpy-cross.log" 2>&1; then
    pbc_err "mpy-cross ließ sich nicht bauen."
    echo "  Vollständige Ausgabe: ${MPY}/mpy-cross.log" >&2; echo >&2
    tail -20 "${MPY}/mpy-cross.log" >&2
    exit 1
fi

pbc_info "Untermodule des rp2-Ports holen (Pico SDK, TinyUSB, lwIP …)"
if ! make -C "${RP2}" BOARD="${BOARD}" submodules >"${RP2}/submodules.log" 2>&1; then
    pbc_err "Die Untermodule konnten nicht geholt werden."
    echo "  Vollständige Ausgabe: ${RP2}/submodules.log" >&2
    echo "  Meist fehlt schlicht der Netzzugang." >&2; echo >&2
    tail -20 "${RP2}/submodules.log" >&2
    exit 1
fi

pbc_info "Firmware bauen"
if ! make -C "${RP2}" BOARD="${BOARD}" -j"${PBC_JOBS}" >"${RP2}/build.log" 2>&1; then
    pbc_err "Die Firmware ließ sich nicht bauen."
    echo "  Vollständige Ausgabe: ${RP2}/build.log" >&2; echo >&2
    tail -25 "${RP2}/build.log" >&2
    exit 1
fi

pbc_collect "${RP2}/build-${BOARD}/firmware.uf2" "micropython-PBC_PLUS.uf2" || exit 1
pbc_verify_dist micropython-PBC_PLUS.uf2 || exit 1
pbc_hint_flash
echo
echo "  Danach meldet sich das Gerät als serielle Konsole (REPL)."
echo "  Die eigenen Module sind als 'import pbc' erreichbar — siehe"
echo "  PBC_PLUS/README.md."
