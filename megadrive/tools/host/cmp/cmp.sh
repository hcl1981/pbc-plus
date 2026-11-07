#!/usr/bin/env bash
# cmp.sh BASE [FRAMES]: this megadrive tree against BASE (a copy of an older
# state), on the PC: the test ROMs with input, the VDP and YM2612 fuzzers,
# the scaler. Every line must say SAME. ROMS=dir with the three games.
set -uo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
NEW="$(cd "$H/../../.." && pwd)"
BASE="$(cd "$1" && pwd)"
N="${2:-6000}"
R="${ROMS:-/home/hcl/Desktop/Spielesamlung}"
S2="$R/Sonic The Hedgehog 2 (World) (Rev A).md"
SOR="$R/Streets of Rage 2 (USA).md"
GUN="$R/vom gerät/Gunstar Heroes (Europe).md"
mkdir -p out
"$H/hb.sh" "$NEW" ./hc_new > build_new.log 2>&1 || { tail -30 build_new.log; exit 1; }
"$H/hb.sh" "$BASE" ./hc_base > build_base.log 2>&1 || { tail -30 build_base.log; exit 1; }
run() {   # name args...
    local name="$1"; shift
    ./hc_base "$@" 2>/dev/null > "out/$name.base" &
    ./hc_new "$@" 2>/dev/null > "out/$name.new" &
    wait
    if cmp -s "out/$name.base" "out/$name.new" && [ -s "out/$name.base" ]; then
        echo "SAME  $name ($(grep -c '^[0-9]' "out/$name.base") lines)"
    else
        echo "DIFF  $name"; diff "out/$name.base" "out/$name.new" | head -4
    fi
}
run s2       "$S2" $N &
run s2zone   "$S2" $N -z 0x0D01 &
run s2crop   "$S2" $N -m crop -r 7 &
run sor2     "$SOR" $N &
run sor2b    "$SOR" $N -r 3 -m crop &
run gunstar  "$GUN" $N &
run gunstarb "$GUN" $N -r 5 &
run vdpfuzz  "$SOR" 1 -F 200000 &
run linefuzz "$SOR" 1 -L 300 &
wait
# the scaler in every mode on random pictures, and core 1's YM2612 on random writes
CF=(-O2 -w -DPBC_YM_CORE1=1 -include stdint.h -include pico/platform.h -I"$H/fake")
COMMONDIR="$(cd "$H/../../../../common" && pwd)"
for t in base new; do
    M="$BASE"; [ $t = new ] && M="$NEW"
    I=(-I"$M/tools/host/fake" -I"$M/src" -I"$M/core" -I"$M/core/sound" -I"$M/core/bus" -I"$M/core/savestate" -I"$M/core/cpus/musashi" -I"$COMMONDIR")
    gcc "${CF[@]}" "${I[@]}" "$H/vtest.c" -o "./vt_$t"
    gcc "${CF[@]}" "${I[@]}" "$H/ymfuzz.c" "$H/hstubs.c" -o "./ymf_$t"
done
./vt_base 400 > out/vt.base; ./vt_new 400 > out/vt.new
cmp -s out/vt.base out/vt.new && echo "SAME  scaler" || echo "DIFF  scaler"
for seed in 1 2 3 4; do
    ./ymf_base 3000 $seed > out/ymf.base & ./ymf_new 3000 $seed > out/ymf.new & wait
    cmp -s out/ymf.base out/ymf.new && echo "SAME  ymfuzz $seed" || echo "DIFF  ymfuzz $seed"
done
