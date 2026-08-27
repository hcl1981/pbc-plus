#!/usr/bin/env bash
#
# Baut i2collection für den PicoBoy Color Plus.
# Spielesammlung für zwei Geräte über USB-C
#
# Dieses Projekt baut als einziges NICHT über das Pico SDK, sondern über
# PlatformIO mit dem Arduino-Kern (arduino-pico von Earle Philhower).
#
# Aufruf:   ./build.sh          bauen
#           ./build.sh clean    Bauordner löschen und neu bauen
#
# Ergebnis: ../dist/i2collection.uf2
#
set -uo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${HIER}/../tools/pbc_build.sh"

[ "${1:-}" = clean ] && rm -rf "${HIER}/.pio"

pbc_head "i2collection — Spielesammlung für zwei Geräte über USB-C"

# PlatformIO bringt seine eigene Werkzeugkette mit; git braucht es aber, um
# die Platform-Definition zu holen.
pbc_need git || exit 1

# PlatformIO liegt selten im Systempfad; die üblichen Orte mit absuchen.
PIO=""
for c in "$(command -v pio 2>/dev/null)" "$(command -v platformio 2>/dev/null)" \
         "${HOME}/.platformio/penv/bin/pio" "${HOME}/.pio-venv/bin/pio"; do
    [ -n "${c}" ] && [ -x "${c}" ] && { PIO="${c}"; break; }
done

if [ -z "${PIO}" ]; then
    pbc_err "PlatformIO wurde nicht gefunden."
    cat >&2 <<'HILFE'

    i2collection baut über PlatformIO statt über das Pico SDK. PlatformIO ist ein
    Python-Programm und wird am besten in eine eigene Umgebung installiert,
    damit es nicht mit den Systempaketen kollidiert:

        python3 -m venv ~/.pio-venv
        ~/.pio-venv/bin/pip install platformio

    Danach entweder

        PATH=$HOME/.pio-venv/bin:$PATH ./build.sh

    oder dauerhaft in die eigene ~/.bashrc aufnehmen:

        export PATH="$HOME/.pio-venv/bin:$PATH"

    Beim ersten Lauf lädt PlatformIO den Arduino-Kern und die Werkzeugkette
    nach (einige hundert MB). Das dauert, passiert aber nur einmal.

HILFE
    exit 1
fi
pbc_ok "PlatformIO: ${PIO}"

pbc_info "Übersetzen (beim ersten Mal werden Kern und Werkzeugkette geholt)"
if ! ( cd "${HIER}" && "${PIO}" run ) >"${HIER}/pio.build.log" 2>&1; then
    pbc_err "PlatformIO konnte das Projekt nicht bauen."
    echo "  Vollständige Ausgabe: ${HIER}/pio.build.log" >&2
    echo >&2
    tail -25 "${HIER}/pio.build.log" >&2
    echo >&2
    echo "  Häufigste Ursache ohne Netzzugang: der Arduino-Kern konnte nicht" >&2
    echo "  geladen werden. Siehe docs/BAUEN.md, Abschnitt «PlatformIO»." >&2
    exit 1
fi

pbc_collect "${HIER}/.pio/build/picoboy_color_plus/firmware.uf2" "i2collection.uf2" || exit 1
pbc_verify_dist "i2collection.uf2" || exit 1
pbc_hint_flash
