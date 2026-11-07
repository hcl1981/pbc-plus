#!/usr/bin/env bash
# hb.sh TREE OUT [gcc flags...]: hostcmp for the PC from megadrive tree TREE
# (this one or a copy of an older state). Objects in ./obj_OUT.
set -euo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
M="$(cd "$1" && pwd)"; OUT="$2"; shift 2
C="$M/core"
COMMONDIR="$(cd "$H/../../../../common" && pwd)"
B="$(pwd)/obj_$(basename "$OUT")"
rm -rf "$B"; mkdir -p "$B"
OPT="${OPT:--O2}"
# the VDP line routines without ARM assembly (their C version is in the core)
sed 's/^#define USE_ASM_VDP 1/#define USE_ASM_VDP 0/; s/^#define USE_ASM_LINE 1/#define USE_ASM_LINE 0/' \
    "$C/vdp/gwenesis_vdp_gfx.c" > "$B/vdp_gfx_host.c"
# core 1's sound loop as is, plus a look at its ring
printf '#include "%s/src/ym_core1.c"\n#ifdef OUT_CHUNK\nint hostcmp_ring_empty(void) { return rd == wr && out_n < 0; }\n#else\nint hostcmp_ring_empty(void) { return rd == wr; }\n#endif\n' "$M" > "$B/wrap_ym_core1.c"
CF=($OPT -g -w -fno-strict-aliasing
    -DROM_SWAP=0 -DBUS_DISABLE_LOGGING=1 -DVDP_GFX_DISABLE_LOGGING=1 -DENABLE_LOGGING=0
    -DLSB_FIRST=1 -DGW_TARGET=1 -DPBC_YM_CORE1=1 -include stdint.h -include pico/platform.h
    -I"$H/fake" -I"$M/tools/host/fake" -I"$M/src" -I"$C" -I"$C/bus" -I"$C/io" -I"$C/vdp"
    -I"$C/sound" -I"$C/cpus/musashi" -I"$C/cpus/redcode" -I"$C/savestate" -I"$COMMONDIR" "$@")
gcc "${CF[@]}" -DZ80_WITH_LOCAL_HEADER -DZ80_PBC_FETCH8K=Z80_RAM -DZ80_PBC_READ=z80_read \
    -DZ80_PBC_WRITE=z80_write -DZ80_PBC_SWITCH -DZ80_PBC_RAM_TABLES -DZ80_PBC_SWITCH_XY \
    -Dz80_run=redcode_z80_run -Dz80_execute=redcode_z80_execute \
    -c "$C/cpus/redcode/Z80.c" -o "$B/redcode_z80.o"
pids=()
for s in "$H/hostcmp.c" "$H/hstubs.c" "$B/vdp_gfx_host.c" "$C/bus/gwenesis_bus.c" \
         "$C/io/gwenesis_io.c" "$C/vdp/gwenesis_vdp_mem.c" "$C/sound/gwenesis_sn76489.c" \
         "$C/sound/ym2612.c" "$C/sound/z80inst.c" "$C/sound/z80_benchmark.c" \
         "$C/cpus/musashi/m68kcpu.c" "$C/cpus/musashi/m68kops.c" "$B/wrap_ym_core1.c" \
         "$M/src/ym2612_synth.c" "$M/src/video.c"; do
    gcc "${CF[@]}" -c "$s" -o "$B/$(basename "$s" .c).o" & pids+=($!)
done
for p in "${pids[@]}"; do wait $p; done
gcc -o "$OUT" "$B"/*.o -lm ${LDX:-}
echo "OK $OUT"
