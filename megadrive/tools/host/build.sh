#!/usr/bin/env bash
# Baut mdhost: den Mega-Drive-Kern als PC-Programm (gcc, Linux/macOS).
#   ./build.sh                  -> ./mdhost
#   CPU=old OUT=mdhost-old ./build.sh   mit dem früheren 68000-Kern in
#                                       core/cpus/M68K (aus der Git-Geschichte
#                                       zurückholen), zum Vergleich Befehl für
#                                       Befehl (MDHOST_TRACE, siehe host.c)
set -euo pipefail
HIER="$(cd "$(dirname "$0")" && pwd)"
M="${HIER}/../.."
C="${M}/core"
B="${HIER}/build"
CPU="${CPU:-musashi}"
OUT="${OUT:-mdhost}"
mkdir -p "${B}"
# Die VDP-Zeilenroutinen ohne ARM-Assembler (deren C-Fassung steckt im Kern)
sed 's/^#define USE_ASM_VDP 1/#define USE_ASM_VDP 0/; s/^#define USE_ASM_LINE 1/#define USE_ASM_LINE 0/' \
    "${C}/vdp/gwenesis_vdp_gfx.c" > "${B}/vdp_gfx_host.c"

if [ "${CPU}" = old ]; then
    CPUINC="${C}/cpus/M68K"
    CPUSRC=("${C}/cpus/M68K/m68kcpu.c")
    CPUDEF=(-DOLD_M68K -DHOOK_CPU)
else
    CPUINC="${C}/cpus/musashi"
    CPUSRC=("${C}/cpus/musashi/m68kcpu.c" "${C}/cpus/musashi/m68kops.c")
    CPUDEF=("-DM68K_TRACE_HOOK=m68k_trace_hook")
fi

COMMON=(-O2 -g -w
    -DROM_SWAP=0 -DBUS_DISABLE_LOGGING=1 -DVDP_GFX_DISABLE_LOGGING=1 -DENABLE_LOGGING=0
    -DLSB_FIRST=1 -DGW_TARGET=1 -include stdint.h -include pico/platform.h
    -I"${HIER}/fake" -I"${M}/src" -I"${C}" -I"${C}/bus" -I"${C}/io" -I"${C}/vdp"
    -I"${C}/sound" -I"${CPUINC}" -I"${C}/cpus/redcode" -I"${C}/savestate" -I"${M}/../common")

# Z80: redcode/Z80 mit denselben Schaltern wie in CMakeLists.txt
gcc "${COMMON[@]}" -DZ80_WITH_LOCAL_HEADER -DZ80_PBC_FETCH8K=Z80_RAM -DZ80_PBC_READ=z80_read \
    -DZ80_PBC_WRITE=z80_write -DZ80_PBC_SWITCH -DZ80_PBC_RAM_TABLES \
    -Dz80_run=redcode_z80_run -Dz80_execute=redcode_z80_execute \
    -c "${C}/cpus/redcode/Z80.c" -o "${B}/redcode_z80.o"

gcc "${COMMON[@]}" "${CPUDEF[@]}" \
    -o "${HIER}/${OUT}" "${HIER}/host.c" "${HIER}/stubs.c" "${B}/vdp_gfx_host.c" \
    "${C}/bus/gwenesis_bus.c" "${C}/io/gwenesis_io.c" "${C}/vdp/gwenesis_vdp_mem.c" \
    "${C}/sound/gwenesis_sn76489.c" "${C}/sound/ym2612.c" "${C}/sound/z80inst.c" \
    "${C}/sound/z80_benchmark.c" "${CPUSRC[@]}" "${B}/redcode_z80.o" -lm \
    -Wl,--wrap=YM2612Write
echo "OK ${HIER}/${OUT}"
