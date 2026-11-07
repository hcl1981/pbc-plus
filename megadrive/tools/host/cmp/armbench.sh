#!/usr/bin/env bash
# armbench.sh BIN... (from ab.sh): instructions per frame on the emulated
# Cortex-M33, by part, and a hash over all frames (must match between two
# states). Needs Python with unicorn and pyelftools (PY=...).
H="$(cd "$(dirname "$0")" && pwd)"; R="${ROMS:-/home/hcl/Desktop/Spielesamlung}"
PY="${PY:-python3}"
for b in "$@"; do
  $PY "$H/armrun.py" "$b" "$R/Sonic The Hedgehog 2 (World) (Rev A).md" "${S2F:-1330} -q -z 0x0D01" ${S2S:-1300} 2>/dev/null | sed "s|^|$b s2     |" &
  $PY "$H/armrun.py" "$b" "$R/Streets of Rage 2 (USA).md" "${SRF:-1830} -q" ${SRS:-1800} 2>/dev/null | sed "s|^|$b sor2   |" &
  $PY "$H/armrun.py" "$b" "$R/vom gerät/Gunstar Heroes (Europe).md" "${GHF:-1830} -q" ${GHS:-1800} 2>/dev/null | sed "s|^|$b gunst  |" &
done
wait
