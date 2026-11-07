#!/usr/bin/env bash
# ab.sh TREE OUT: hostcmp for the Cortex-M33 with the firmware's flags and
# the VDP's ARM assembly, to be run by armrun.py. Objects in ./aobj_OUT.
set -euo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
M="$(cd "$1" && pwd)"; OUT="$2"; shift 2
C="$M/core"; COMMONDIR="$(cd "$H/../../../../common" && pwd)"
B="$(pwd)/aobj_$(basename "$OUT")"; rm -rf "$B"; mkdir -p "$B"
printf '#include "%s/src/ym_core1.c"\n#ifdef OUT_CHUNK\nint hostcmp_ring_empty(void) { return rd == wr && out_n < 0; }\n#else\nint hostcmp_ring_empty(void) { return rd == wr; }\n#endif\n' "$M" > "$B/wrap_ym_core1.c"
ARCH=(-mcpu=cortex-m33 -mthumb -march=armv8-m.main+dsp -mfloat-abi=soft)
CF=("${ARCH[@]}" -O3 -g -w -ffunction-sections -fdata-sections -DNDEBUG -DARMBENCH
    -DROM_SWAP=0 -DBUS_DISABLE_LOGGING=1 -DVDP_GFX_DISABLE_LOGGING=1 -DENABLE_LOGGING=0
    -DLSB_FIRST=1 -DGW_TARGET=1 -DPBC_YM_CORE1=1 -DZ80_STATIC -include stdint.h -include pico/platform.h
    -I"$H/fake" -I"$M/tools/host/fake" -I"$M/src" -I"$C" -I"$C/bus" -I"$C/io" -I"$C/vdp"
    -I"$C/sound" -I"$C/cpus/musashi" -I"$C/cpus/redcode" -I"$C/savestate" -I"$COMMONDIR")
CC=arm-none-eabi-gcc
REDEF=(-DZ80_WITH_LOCAL_HEADER -DZ80_PBC_FETCH8K=Z80_RAM -DZ80_PBC_READ=z80_read -DZ80_PBC_WRITE=z80_write
       -DZ80_PBC_SWITCH -DZ80_PBC_RAM_TABLES -DZ80_PBC_SWITCH_XY -Dz80_run=redcode_z80_run -Dz80_execute=redcode_z80_execute)
pids=()
$CC "${CF[@]}" "${REDEF[@]}" ${Z80OPT:--O2} -c "$C/cpus/redcode/Z80.c" -o "$B/redcode_z80.o" & pids+=($!)
for s in "$C/cpus/musashi/m68kcpu.c" "$C/cpus/musashi/m68kops.c"; do
  $CC "${CF[@]}" -O3 -ffast-math -funroll-loops -finline-functions -fno-partial-inlining -c "$s" -o "$B/$(basename "$s" .c).o" & pids+=($!)
done
for s in "$H/hostcmp.c" "$H/hstubs.c" "$H/armglue.c" "$C/vdp/gwenesis_vdp_gfx.c" "$C/bus/gwenesis_bus.c" \
         "$C/io/gwenesis_io.c" "$C/vdp/gwenesis_vdp_mem.c" "$C/sound/gwenesis_sn76489.c" \
         "$C/sound/ym2612.c" "$C/sound/z80inst.c" "$C/sound/z80_benchmark.c" "$B/wrap_ym_core1.c" \
         "$M/src/ym2612_synth.c" "$M/src/video.c"; do
  $CC "${CF[@]}" -c "$s" -o "$B/$(basename "$s" .c).o" & pids+=($!)
done
for s in "$C"/vdp/*.S; do $CC "${ARCH[@]}" -c "$s" -o "$B/$(basename "$s" .S)_S.o" & pids+=($!); done
for p in "${pids[@]}"; do wait $p; done
$CC "${ARCH[@]}" -nostartfiles --specs=nano.specs -T "$H/arm.ld" -Wl,--gc-sections -o "$OUT" "$B"/*.o -lc -lgcc
echo "OK $OUT"
