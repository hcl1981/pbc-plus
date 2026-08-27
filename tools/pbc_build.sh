#!/usr/bin/env bash
#
# pbc_build.sh — gemeinsame Bausteine für alle Projekte dieses Repos.
#
# Wird nicht direkt aufgerufen. Jedes Projekt hat ein eigenes `build.sh`, das
# diese Datei einbindet:
#
#     . "$(dirname "${BASH_SOURCE[0]}")/../tools/pbc_build.sh"
#     pbc_need cmake ninja arm-none-eabi-gcc
#     pbc_sdk
#     pbc_cmake_build "${PROJ}" "${PROJ}/build" -DPICO_BOARD=pico2
#     pbc_collect "${PROJ}/build/spiel.uf2" spiel.uf2
#
# Zielgerät durchgehend: PicoBoy Color Plus — RP2350A, 16 MB Flash,
# ST7789 240x280. Ergebnisse landen in `dist/` im Wurzelverzeichnis.
#
set -uo pipefail

PBC_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PBC_DIST="${PBC_ROOT}/dist"
PBC_JOBS="${PBC_JOBS:-$(nproc 2>/dev/null || echo 2)}"

PBC_SDK_CACHE="${HOME}/.cache/pbcp-sdks"
PBC_SDK_TAG="2.1.0"
PBC_EXTRAS_BRANCH="sdk-2.1.0"
PBC_MPY_TAG="v1.28.0"

# ---------------------------------------------------------------------------
# Ausgabe
# ---------------------------------------------------------------------------
if [ -t 1 ]; then
    PBC_B=$'\033[1m'; PBC_BL=$'\033[1;34m'; PBC_GN=$'\033[1;32m'
    PBC_RD=$'\033[1;31m'; PBC_YL=$'\033[1;33m'; PBC_N=$'\033[0m'
else PBC_B=; PBC_BL=; PBC_GN=; PBC_RD=; PBC_YL=; PBC_N=; fi

pbc_info() { printf '%s==>%s %s\n' "$PBC_BL" "$PBC_N" "$*"; }
pbc_ok()   { printf '%s OK%s %s\n' "$PBC_GN" "$PBC_N" "$*"; }
pbc_warn() { printf '%s !!%s %s\n' "$PBC_YL" "$PBC_N" "$*"; }
pbc_err()  { printf '%sFEHLER:%s %s\n' "$PBC_RD" "$PBC_N" "$*" >&2; }
pbc_head() { printf '\n%s────────────────────────────────────────────────────────%s\n%s%s%s\n' \
                    "$PBC_B" "$PBC_N" "$PBC_B" "$*" "$PBC_N"; }

# ---------------------------------------------------------------------------
# Werkzeuge prüfen — mit Angabe, wofür sie gebraucht werden und wie man sie
# installiert. Die Paketnamen unterscheiden sich je Distribution, deshalb wird
# sie erkannt und nur der passende Befehl gezeigt.
# ---------------------------------------------------------------------------
pbc_pkg_for() {   # <werkzeug> <apt|dnf|pacman|brew>
    case "$1:$2" in
        cmake:apt) echo cmake ;;    cmake:dnf) echo cmake ;;
        cmake:pacman) echo cmake ;; cmake:brew) echo cmake ;;
        ninja:apt) echo ninja-build ;;  ninja:dnf) echo ninja-build ;;
        ninja:pacman) echo ninja ;;     ninja:brew) echo ninja ;;
        arm-none-eabi-gcc:apt) echo gcc-arm-none-eabi ;;
        arm-none-eabi-gcc:dnf) echo arm-none-eabi-gcc-cs ;;
        arm-none-eabi-gcc:pacman) echo arm-none-eabi-gcc ;;
        arm-none-eabi-gcc:brew) echo --cask\ gcc-arm-embedded ;;
        gcc:apt) echo build-essential ;; gcc:dnf) echo gcc\ gcc-c++\ make ;;
        gcc:pacman) echo base-devel ;;   gcc:brew) echo gcc ;;
        make:apt) echo build-essential ;; make:dnf) echo make ;;
        make:pacman) echo base-devel ;;   make:brew) echo make ;;
        git:*) echo git ;;
        python3:apt) echo python3 ;; python3:dnf) echo python3 ;;
        python3:pacman) echo python ;; python3:brew) echo python3 ;;
        *) echo "$1" ;;
    esac
}

pbc_why() {   # wofür wird das Werkzeug gebraucht
    case "$1" in
        cmake)             echo "Bausystem aller Pico-SDK-Projekte" ;;
        ninja)             echo "schnellerer Generator; ohne ihn wird Make benutzt" ;;
        arm-none-eabi-gcc) echo "Übersetzer für den RP2350 (ARM Cortex-M33)" ;;
        gcc)               echo "Übersetzer fürs eigene System (picotool, Hilfswerkzeuge)" ;;
        make)              echo "wird vom MicroPython-Port benutzt" ;;
        git)               echo "holt Pico SDK, pico-extras und MicroPython" ;;
        python3)           echo "Hilfsskripte des SDK und der Datenpacker" ;;
        pio)               echo "PlatformIO — nur für jumpnbump und i2collection" ;;
        *)                 echo "" ;;
    esac
}

pbc_distro() {
    command -v apt-get >/dev/null && { echo apt; return; }
    command -v dnf     >/dev/null && { echo dnf; return; }
    command -v pacman  >/dev/null && { echo pacman; return; }
    command -v brew    >/dev/null && { echo brew; return; }
    echo unbekannt
}

# pbc_need <werkzeug>...  — bricht ab, wenn eines fehlt, und erklärt was zu tun ist
pbc_need() {
    local missing=() t
    for t in "$@"; do
        if [ "$t" = gcc ]; then
            command -v cc >/dev/null || command -v gcc >/dev/null || missing+=(gcc)
        else
            command -v "$t" >/dev/null || missing+=("$t")
        fi
    done
    [ ${#missing[@]} -eq 0 ] && return 0

    local d; d="$(pbc_distro)"
    pbc_err "Zum Bauen fehlen ${#missing[@]} Werkzeug(e):"
    echo >&2
    for t in "${missing[@]}"; do
        printf '    %s%-20s%s %s\n' "$PBC_B" "$t" "$PBC_N" "$(pbc_why "$t")" >&2
    done
    echo >&2
    local pkgs=() t2
    for t in "${missing[@]}"; do pkgs+=("$(pbc_pkg_for "$t" "$d")"); done
    # doppelte Paketnamen (build-essential zweimal) zusammenfassen
    mapfile -t pkgs < <(printf '%s\n' "${pkgs[@]}" | awk '!seen[$0]++')
    case "$d" in
        apt)    echo "  So installierst du sie:" >&2
                echo "      sudo apt-get install -y ${pkgs[*]}" >&2 ;;
        dnf)    echo "  So installierst du sie:" >&2
                echo "      sudo dnf install -y ${pkgs[*]}" >&2 ;;
        pacman) echo "  So installierst du sie:" >&2
                echo "      sudo pacman -S --needed ${pkgs[*]}" >&2 ;;
        brew)   echo "  So installierst du sie:" >&2
                echo "      brew install ${pkgs[*]}" >&2 ;;
        *)      echo "  Bitte über die Paketverwaltung deines Systems nachinstallieren:" >&2
                echo "      ${pkgs[*]}" >&2 ;;
    esac
    echo >&2
    echo "  Mehr dazu steht in docs/BAUEN.md." >&2
    return 1
}

# ---------------------------------------------------------------------------
# Generator
# ---------------------------------------------------------------------------
PBC_GEN=()
pbc_pick_generator() {
    if command -v ninja >/dev/null; then PBC_GEN=(-G Ninja)
    else pbc_warn "kein ninja gefunden — es wird mit Make gebaut (langsamer, funktioniert aber)"; fi
    # CMake 4 lehnt cmake_minimum_required(<3.5) ab. Das Pico SDK 2.1.0 und
    # einige mitgelieferte Fremdbibliotheken stammen aus der Zeit davor; diese
    # Variable stellt die alte Verträglichkeit her, ohne fremde Quellen anzufassen.
    case "$(cmake --version 2>/dev/null | head -1 | awk '{print $3}')" in
        4.*) export CMAKE_POLICY_VERSION_MINIMUM=3.5 ;;
    esac
}

# ---------------------------------------------------------------------------
# Fremdquellen holen
# ---------------------------------------------------------------------------
# pbc_clone <url> <tag/branch> <ziel> <klartextname> [--ohne-submodule]
#
# --ohne-submodule ist fuer MicroPython gedacht: dessen Untermodule decken alle
# Ports ab (ESP32, STM32, Alif, ...) und waeren zusammen mehrere Gigabyte. Der
# rp2-Port holt sich ueber `make submodules` genau die, die er braucht.
pbc_clone() {
    local url="$1" ref="$2" dst="$3" name="$4" ohne="${5:-}"
    if [ -f "${dst}/.git/HEAD" ] || [ -f "${dst}/CMakeLists.txt" ]; then
        pbc_ok "${name} vorhanden: ${dst}"; return 0
    fi
    pbc_info "Hole ${name} (${ref}) nach ${dst} — das passiert nur einmal"
    rm -rf "${dst}"
    if ! git clone --branch "${ref}" --depth 1 "${url}" "${dst}" >/dev/null 2>&1; then
        if ! git clone "${url}" "${dst}" >/dev/null 2>&1; then
            pbc_err "${name} konnte nicht geholt werden."
            echo "  Versucht wurde: git clone --branch ${ref} ${url}" >&2
            echo "  Ohne Netzzugang kannst du es selbst bereitstellen und den Pfad setzen," >&2
            echo "  siehe docs/BAUEN.md, Abschnitt «Ohne Internet bauen»." >&2
            return 1
        fi
    fi
    [ "${ohne}" = --ohne-submodule ] \
        || git -C "${dst}" submodule update --init --recursive --depth 1 >/dev/null 2>&1 || true
    pbc_ok "${name} geholt"
}

# pioasm (der PIO-Assembler des SDK, fürs eigene System gebaut) benutzt
# uint8_t/uint32_t ohne <cstdint>. Bis GCC 14 kam das über andere Header mit
# herein, GCC 15 räumt die Kette auf — seitdem bricht der Bau mit
# «'uint8_t' does not name a type». Betrifft nur Projekte mit PIO-Programmen.
pbc_fix_pioasm() {
    local d="${PICO_SDK_PATH}/tools/pioasm" f n=0
    [ -d "${d}" ] || return 0
    for f in "${d}"/*.h; do
        grep -q '#include <cstdint>' "${f}" 2>/dev/null && continue
        grep -qE 'uint(8|16|32|64)_t' "${f}" 2>/dev/null || continue
        sed -i '0,/^#define _.*_H$/s//&\n\n#include <cstdint>/' "${f}" && n=$((n+1))
    done
    [ "${n}" -gt 0 ] && pbc_warn "pioasm: <cstdint> in ${n} SDK-Header nachgetragen (GCC-15-Verträglichkeit)"
    return 0
}

# pbc_sdk [extras]  — sorgt für PICO_SDK_PATH (und auf Wunsch PICO_EXTRAS_PATH)
pbc_sdk() {
    mkdir -p "${PBC_SDK_CACHE}"
    if [ -n "${PICO_SDK_PATH:-}" ] && [ -f "${PICO_SDK_PATH}/pico_sdk_init.cmake" ]; then
        pbc_ok "benutze vorgegebenes PICO_SDK_PATH=${PICO_SDK_PATH}"
    else
        PICO_SDK_PATH="${PBC_SDK_CACHE}/pico-sdk"
        pbc_clone https://github.com/raspberrypi/pico-sdk.git "${PBC_SDK_TAG}" \
                  "${PICO_SDK_PATH}" "Pico SDK ${PBC_SDK_TAG}" || return 1
    fi
    export PICO_SDK_PATH
    pbc_fix_pioasm

    if [ "${1:-}" = extras ]; then
        if [ -n "${PICO_EXTRAS_PATH:-}" ] && [ -f "${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake" ]; then
            pbc_ok "benutze vorgegebenes PICO_EXTRAS_PATH=${PICO_EXTRAS_PATH}"
        else
            PICO_EXTRAS_PATH="${PBC_SDK_CACHE}/pico-extras"
            pbc_clone https://github.com/raspberrypi/pico-extras.git "${PBC_EXTRAS_BRANCH}" \
                      "${PICO_EXTRAS_PATH}" "pico-extras" || return 1
        fi
        export PICO_EXTRAS_PATH
    fi

    # picotool hängt auf dem RP2350 die Signatur an die UF2. Das SDK baut es
    # selbst, wenn man ihm einen Ablageort nennt.
    export PICOTOOL_FETCH_FROM_GIT_PATH="${PICOTOOL_FETCH_FROM_GIT_PATH:-${HOME}/.cache/pbc_plus_picotool}"
    mkdir -p "${PICOTOOL_FETCH_FROM_GIT_PATH}"
    pbc_pick_generator
}

# ---------------------------------------------------------------------------
# Bauen
# ---------------------------------------------------------------------------
# PBC_TARGET (optional): nur dieses Target bauen statt aller.
pbc_cmake_build() {   # <quelle> <bauordner> [-D...]
    local src="$1" bdir="$2"; shift 2
    mkdir -p "$(dirname "${bdir}")"
    if ! cmake -S "${src}" -B "${bdir}" "${PBC_GEN[@]}" \
               -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_SDK_PATH="${PICO_SDK_PATH}" "$@" \
               >"${bdir}.cmake.log" 2>&1; then
        pbc_err "cmake konnte das Projekt nicht einrichten."
        echo "  Vollständige Ausgabe: ${bdir}.cmake.log" >&2; echo >&2
        tail -20 "${bdir}.cmake.log" >&2
        return 1
    fi
    local targ=(); [ -n "${PBC_TARGET:-}" ] && targ=(--target "${PBC_TARGET}")
    if ! cmake --build "${bdir}" "${targ[@]}" -j"${PBC_JOBS}" >"${bdir}.build.log" 2>&1; then
        pbc_err "Das Übersetzen ist fehlgeschlagen."
        echo "  Vollständige Ausgabe: ${bdir}.build.log" >&2; echo >&2
        tail -25 "${bdir}.build.log" >&2
        return 1
    fi
    return 0
}

# pbc_collect <quelldatei> <zielname> [unterordner in dist]
pbc_collect() {
    local src="$1" name="$2" sub="${3:-}"
    if [ ! -f "${src}" ]; then
        pbc_err "Der Bau lief durch, aber die erwartete Datei fehlt: ${src}"
        return 1
    fi
    mkdir -p "${PBC_DIST}${sub:+/$sub}"
    cp -f "${src}" "${PBC_DIST}${sub:+/$sub}/${name}"
    pbc_ok "$(printf '%-38s %s' "${name}" "$(du -h "${PBC_DIST}${sub:+/$sub}/${name}" | cut -f1)")"
}

# ---------------------------------------------------------------------------
# UF2 prüfen — fängt einen still falsch gebauten Ordner ab. Ein Projekt, dessen
# CMakeLists Board und Plattform nicht selbst setzt, fällt sonst auf die
# SDK-Vorgabe «pico» (RP2040) zurück: die Datei sieht gültig aus, startet aber
# auf dem Gerät nicht.
# ---------------------------------------------------------------------------
pbc_check_uf2() {   # <datei>...
    python3 - "$@" <<'PY'
import struct, sys, os, collections
RP2040, ABSOLUTE, DATA = 0xe48bff56, 0xe48bff57, 0xe48bff58
RP2350 = {0xe48bff59, 0xe48bff5a, 0xe48bff5b}
NAMES = {RP2040:'RP2040', ABSOLUTE:'ABSOLUTE', DATA:'DATA', 0xe48bff59:'RP2350_ARM_S',
         0xe48bff5a:'RP2350_RISCV', 0xe48bff5b:'RP2350_ARM_NS'}
bad = 0
for f in sys.argv[1:]:
    name = os.path.basename(f)
    try: d = open(f, 'rb').read()
    except OSError as e: print(f"  FEHLER {name}: {e}"); bad += 1; continue
    if not d or len(d) % 512:
        print(f"  FEHLER {name}: Länge ist kein Vielfaches von 512 — keine gültige UF2"); bad += 1; continue
    fam = collections.Counter(); ok = True
    for i in range(len(d)//512):
        b = d[i*512:(i+1)*512]
        if struct.unpack('<II', b[:8]) != (0x0A324655, 0x9E5D5157): ok = False; break
        flags = struct.unpack('<I', b[8:12])[0]
        fam[struct.unpack('<I', b[28:32])[0] if flags & 0x2000 else None] += 1
    if not ok:
        print(f"  FEHLER {name}: UF2-Kennung stimmt nicht"); bad += 1; continue
    if RP2040 in fam:
        print(f"  FEHLER {name}: enthält {fam[RP2040]} RP2040-Blöcke. Das Projekt wurde für"
              f"\n         den falschen Chip gebaut und würde auf dem Gerät nicht starten."
              f"\n         Abhilfe: -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2 mitgeben.")
        bad += 1; continue
    if not (set(fam) & RP2350):
        print(f"  FEHLER {name}: keine RP2350-Blöcke enthalten"); bad += 1; continue
    print(f"  ok     {name:<38} " + ", ".join(
        f"{NAMES.get(k,'?') if k else 'ohne Family'}×{v}" for k, v in sorted(fam.items(), key=lambda x: -x[1])))
sys.exit(1 if bad else 0)
PY
}

# Am Ende eines Projekt-build.sh aufrufen: prüft, was gerade eingesammelt wurde.
pbc_verify_dist() {
    local files=(); local f
    for f in "$@"; do [ -f "${PBC_DIST}/${f}" ] && files+=("${PBC_DIST}/${f}"); done
    [ ${#files[@]} -eq 0 ] && return 0
    pbc_check_uf2 "${files[@]}"
}

pbc_hint_flash() {
    echo
    echo "  Flashen: BOOTSEL gedrückt halten, USB anstecken, die .uf2 aus"
    echo "           dist/ auf das erscheinende Laufwerk kopieren."
}
