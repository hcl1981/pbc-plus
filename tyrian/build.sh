#!/usr/bin/env bash
#
# Baut OpenTyrian für den PicoBoy Color Plus.
#
# Zwei Schritte: erst die Firmware, dann das Datenarchiv aus den
# Tyrian-2.1-Daten unter data/. Beides wird danach zu einer einzigen UF2
# verschmolzen, weil Firmware und Daten an verschiedenen Flash-Adressen liegen
# (0x10000000 bzw. 0x10100000).
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
# Ergebnis: ../dist/tyrian-komplett.uf2   Firmware + Daten in einer Datei
#
# Wer nur an der Firmware arbeitet, flasht stattdessen das unverschmolzene
# pbc-tyrian/build/tyrian.uf2 — das Datenarchiv liegt an einer anderen
# Flash-Adresse und bleibt dabei unangetastet.
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

PROJ="${HIER}/pbc-tyrian"
OUT="${HIER}/build-out"

[ "${1:-}" = clean ] && rm -rf "${PROJ}/build" "${OUT}"

pbc_head "tyrian — OpenTyrian, Firmware + Spieldaten"
pbc_need cmake git python3 arm-none-eabi-gcc gcc || exit 1
pbc_sdk || exit 1

if [ ! -d "${HIER}/data/tyrian21" ]; then
    pbc_err "Die Tyrian-Daten fehlen: ${HIER}/data/tyrian21"
    echo "  Erwartet werden die Dateien der Tyrian-2.1-Freeware-Ausgabe" >&2
    echo "  (tyrian1.shp, tyrian.hdt, …). Ohne sie lässt sich nur die" >&2
    echo "  Firmware bauen, und die zeigt ohne Daten nichts an." >&2
    exit 1
fi

pbc_cmake_build "${PROJ}" "${PROJ}/build" || exit 1

mkdir -p "${OUT}"
pbc_info "Spieldaten ins Flash-Archiv packen"
if ! python3 "${PROJ}/tools/mkdata.py" "${HIER}/data/tyrian21" -o "${OUT}/tyrian-data.uf2" >/dev/null; then
    pbc_err "mkdata.py konnte die Spieldaten nicht packen."
    echo "  Prüfe, ob data/tyrian21 vollständig ist." >&2
    exit 1
fi

pbc_info "Firmware und Daten zu einer Datei verschmelzen"
if ! python3 "${PROJ}/tools/merge_uf2.py" "${PROJ}/build/tyrian.uf2" "${OUT}/tyrian-data.uf2" \
        -o "${OUT}/tyrian-komplett.uf2" >/dev/null; then
    pbc_err "merge_uf2.py ist fehlgeschlagen."
    exit 1
fi

pbc_collect "${OUT}/tyrian-komplett.uf2" "tyrian-komplett.uf2" || exit 1

pbc_verify_dist "tyrian-komplett.uf2" || exit 1
pbc_hint_flash
