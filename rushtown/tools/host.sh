#!/usr/bin/env bash
# Builds tools/host (the game without hardware) into build-host/.
set -eu
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$HIER/build-host"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -DRT_HOST -fno-strict-aliasing -fsanitize=address,undefined -I"$HIER/src" \
    ${RT_FLAGS:-} "$HIER"/src/{gfx,art,world,snd,music,speech_data,game,draw}.c "$HIER/src/pics.S" "$HIER/tools/host.c" -lm -o "$HIER/build-host/host"
