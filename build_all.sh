#!/usr/bin/env bash
#
# build_all.sh — ruft die build.sh aller Projekte auf und sammelt die
# fertigen UF2-Dateien in dist/.
#
# Jedes Projekt lässt sich genauso gut einzeln bauen:
#
#     cd spout && ./build.sh
#
# Dieses Skript ist nur die Schleife darüber. Es entscheidet nichts selbst —
# was ein Projekt braucht und wie es gebaut wird, steht in seinem eigenen
# build.sh.
#
# Aufruf:
#   ./build_all.sh                  alles bauen
#   ./build_all.sh spout noiz2sa    nur diese Projekte
#   ./build_all.sh list             auflisten und aufhören
#   ./build_all.sh clean            alle Bauordner und dist/ löschen
#   ./build_all.sh -j4              mit vier Jobs statt aller Kerne
#   ./build_all.sh --stop-on-error  beim ersten Fehler abbrechen
#
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${ROOT}/tools/pbc_build.sh"

ALL=(noiz2sa spout stransball2 micropolis tyrian picoboygb doom jumpnbump i2collection micropython)

beschreibung() {
    case "$1" in
        noiz2sa)      echo "abstrakter Kugelhagel-Schütze (BSD)" ;;
        spout)        echo "Höhlenflug mit zerstörbarem Fels (MIT)" ;;
        stransball2)  echo "Super Transball 2, Thrust-artig (GPL-2.0)" ;;
        micropolis)   echo "der quelloffene SimCity-1-Kern (GPL-3.0)" ;;
        tyrian)       echo "OpenTyrian, Firmware + Spieldaten (GPL-2.0)" ;;
        picoboygb)    echo "Game-Boy-Emulator, ROMs per USB (MIT)" ;;
        doom)         echo "Doom mit Mehrspieler über USB-C (GPL-2.0)" ;;
        jumpnbump)    echo "Jump'n'Bump über USB-C (GPL-2.0, PlatformIO)" ;;
        i2collection) echo "Spielesammlung über USB-C (MIT, PlatformIO)" ;;
        micropython)  echo "MicroPython-Firmware, Board PBC_PLUS (MIT)" ;;
    esac
}

STOP=0; CLEAN=0; SELECTED=()
for arg in "$@"; do
    case "$arg" in
        -h|--help)       sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'; exit 0 ;;
        list)            for p in "${ALL[@]}"; do printf '  %-14s %s\n' "$p" "$(beschreibung "$p")"; done; exit 0 ;;
        clean)           CLEAN=1 ;;
        -j*)             export PBC_JOBS="${arg#-j}" ;;
        --stop-on-error) STOP=1 ;;
        -*)              pbc_err "Unbekannte Option: $arg"; exit 2 ;;
        *)               SELECTED+=("$arg") ;;
    esac
done
[ ${#SELECTED[@]} -eq 0 ] && SELECTED=("${ALL[@]}")

for p in "${SELECTED[@]}"; do
    if [ ! -x "${ROOT}/${p}/build.sh" ]; then
        pbc_err "Kein Projekt «${p}». Bekannt sind: ${ALL[*]}"; exit 2
    fi
done

if [ "${CLEAN}" = 1 ]; then
    # Hier werden bewusst NICHT die build.sh der Projekte aufgerufen: deren
    # `clean` löscht den Bauordner und baut anschließend gleich weiter. Zum
    # Abräumen genügt das Löschen, und es passiert der Reihe nach — sonst
    # räumen noch laufende Jobs einem gerade startenden Bau die Ordner weg.
    pbc_info "Alle Bauordner und dist/ löschen"
    rm -rf "${ROOT}/dist"
    for p in "${ALL[@]}"; do
        rm -rf "${ROOT}/${p}/build" "${ROOT}/${p}/.pio"
    done
    rm -rf "${ROOT}/tyrian/pbc-tyrian/build" "${ROOT}/tyrian/build-out" \
           "${ROOT}/doom/picoboy-doom/build-picoboy" \
           "${PBC_SDK_CACHE}/micropython/ports/rp2/build-PBC_PLUS"
    find "${ROOT}" -name '*.log' -delete
    pbc_ok "Abgeräumt. Der Zwischenspeicher in ${PBC_SDK_CACHE} bleibt bestehen."
    exit 0
fi

declare -a N_NAME N_STAT
FEHLER=0
for p in "${SELECTED[@]}"; do
    if "${ROOT}/${p}/build.sh"; then
        N_NAME+=("$p"); N_STAT+=(OK)
    else
        N_NAME+=("$p"); N_STAT+=(FEHLGESCHLAGEN); FEHLER=$((FEHLER+1))
        [ "${STOP}" = 1 ] && { pbc_err "Abbruch wegen --stop-on-error"; break; }
    fi
done

# ---------------------------------------------------------------------------
pbc_head "dist/ beschriften"
{
    echo "PBC+ — gebaute Firmware-Abbilder für den PicoBoy Color Plus"
    echo "(RP2350A, 16 MB Flash, ST7789 240x280)"
    echo
    echo "Flashen: BOOTSEL gedrückt halten, USB anstecken, die gewünschte"
    echo "         .uf2 auf das erscheinende Laufwerk kopieren."
    echo
    echo "Es passt immer nur EINE Firmware aufs Gerät."
    echo
    printf '%-38s %10s  %s\n' "DATEI" "GRÖSSE" "SHA256 (Anfang)"
    for f in "${ROOT}"/dist/*.uf2; do [ -e "$f" ] || continue
        printf '%-38s %10s  %s\n' "$(basename "$f")" "$(du -h "$f" | cut -f1)" "$(sha256sum "$f" | cut -c1-16)"
    done
    if compgen -G "${ROOT}/dist/selbsttest/*.uf2" >/dev/null; then
        echo; echo "selbsttest/ — Prüffirmware für Display, Tasten, Ton und Bildzeit,"
        echo "dazu Tyrian ohne die Spieldaten (nur die Firmware)."
        for f in "${ROOT}"/dist/selbsttest/*.uf2; do
            printf '%-38s %10s  %s\n' "$(basename "$f")" "$(du -h "$f" | cut -f1)" "$(sha256sum "$f" | cut -c1-16)"
        done
    fi
    echo
    echo "Doom braucht zusätzlich eine WAD an eigener Flash-Adresse."
    echo "Das Werkzeug dafür liegt im Repo unter doom/wad2uf2/. Die WADs selbst"
    echo "sind kommerzielle Daten von id Software, liegen nicht bei und"
    echo "dürfen nicht weitergegeben werden."
    echo
    echo "PicoBoyGB braucht ROMs: Mitte halten, RESET drücken, dann meldet"
    echo "sich das Gerät als USB-Stick zum Draufkopieren."
} > "${ROOT}/dist/INHALT.txt"
pbc_ok "dist/INHALT.txt geschrieben"

# Die WAD-Werkzeuge werden nicht nach dist/ kopiert, sondern liegen als
# doom/wad2uf2/ im Repo — sie sind Quelltext plus Binaries und ändern sich
# nicht beim Bauen. dist/INHALT.txt verweist darauf.

# ---------------------------------------------------------------------------
pbc_head "Ergebnis"
for i in "${!N_NAME[@]}"; do
    if [ "${N_STAT[$i]}" = OK ]; then c="$PBC_GN"; else c="$PBC_RD"; fi
    printf '  %s%-16s %-16s%s %s\n' "$c" "${N_NAME[$i]}" "${N_STAT[$i]}" "$PBC_N" "$(beschreibung "${N_NAME[$i]}")"
done
echo
echo "  $(find "${ROOT}/dist" -name '*.uf2' 2>/dev/null | wc -l) UF2-Dateien in ${ROOT}/dist"
if [ "${FEHLER}" -gt 0 ]; then
    echo
    pbc_err "${FEHLER} Projekt(e) fehlgeschlagen. Die Ursache steht jeweils oben,"
    echo "        die vollständige Ausgabe im genannten .log." >&2
    exit 1
fi
pbc_ok "Alles durch."
