#!/usr/bin/env bash
# Baut mdhost: den Mega-Drive-Kern als PC-Programm (gcc, Linux/macOS).
#   ./build.sh            -> ./mdhost
set -euo pipefail
HIER="$(cd "$(dirname "$0")" && pwd)"
M="${HIER}/../.."
C="${M}/core"
B="${HIER}/build"
mkdir -p "${B}"
# Die VDP-Zeilenroutinen ohne ARM-Assembler (deren C-Fassung steckt im Kern)
sed 's/^#define USE_ASM_VDP 1/#define USE_ASM_VDP 0/; s/^#define USE_ASM_LINE 1/#define USE_ASM_LINE 0/' \
    "${C}/vdp/gwenesis_vdp_gfx.c" > "${B}/vdp_gfx_host.c"
gcc -O2 -g -w \
    -DROM_SWAP=0 -DBUS_DISABLE_LOGGING=1 -DVDP_GFX_DISABLE_LOGGING=1 -DENABLE_LOGGING=0 \
    -DLSB_FIRST=1 -DGW_TARGET=1 -include stdint.h -include pico/platform.h \
    -I"${HIER}/fake" -I"${M}/src" -I"${C}" -I"${C}/bus" -I"${C}/io" -I"${C}/vdp" \
    -I"${C}/sound" -I"${C}/cpus/M68K" -I"${C}/cpus/Z80" -I"${C}/savestate" \
    -o "${HIER}/mdhost" "${HIER}/host.c" "${HIER}/stubs.c" "${B}/vdp_gfx_host.c" \
    "${C}/bus/gwenesis_bus.c" "${C}/io/gwenesis_io.c" "${C}/vdp/gwenesis_vdp_mem.c" \
    "${C}/sound/gwenesis_sn76489.c" "${C}/sound/ym2612.c" "${C}/sound/z80inst.c" \
    "${C}/sound/z80_benchmark.c" "${C}/cpus/Z80/Z80.c" "${C}/cpus/M68K/m68kcpu.c" -lm
echo "OK ${HIER}/mdhost"
