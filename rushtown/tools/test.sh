#!/usr/bin/env bash
# Plays through what can be checked without a person: the nine jobs, being
# caught, shopping, the whole story, the ferry, every build without one of
# its parts, and three minutes of traffic. Needs gcc.
set -eu
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$HIER/tools/host.sh"
H="$HIER/build-host/host"
fail=0
check() {   # name, wanted text, output
    if echo "$3" | grep -q -- "$2"; then echo "ok    $1"; else echo "FAIL  $1: no '$2' in: $3"; fail=1; fi
}
start="w10 +A w2 -A w5 +A w2 -A w160"   # the second press puts the opening picture away
# job numbers: 1 courier 2 taxi 3 checkpoints 4 round trip 5 hot car 6 fragile 7 heat 8 getaway 9 shadow
check "courier"      "jobs 1 "        "$($H $start J1 w2 g w2 g w2 p)"
check "taxi"         "jobs 1 "        "$($H $start J2 w2 c g w4 g w4 p)"
check "taxi on foot" "job 2/0"        "$($H $start J2 w2 g w4 p)"
check "checkpoints"  "jobs 1 "        "$($H $start J3 w3 c g w2 g w2 g w2 g w2 g w2 g w2 p)"
check "round trip"   "jobs 1 "        "$($H $start J4 w2 g w2 g w2 g w2 g w2 p)"
check "hot car"      "jobs 1 "        "$($H $start J5 w2 g w3 +C w3 -C w5 g w30 p)"
check "its stars"    "stars 2"        "$($H $start J5 w2 g w3 +C w3 -C w5 p)"
check "fragile load" "jobs 1 "        "$($H $start J6 w2 g w3 +C w3 -C w5 g w30 p)"
check "bail"         "cash 750 .*lives 5 " "$($H $start '$1000' J7 w2 U w300 p)"
ko="H w140"
check "a life gone"  "lives 4 "       "$($H $start $ko p)"
check "game over"    "state 4 "       "$($H $start $ko $ko $ko $ko $ko p)"
check "and again"    "state 0 .*cash 0 .*story 6 .*lives 5 " "$($H $start S8 '$500' $ko $ko $ko $ko $ko +A w2 -A w3 p)"
check "new chapter"  "lives 5 "       "$($H $start S5 $ko $ko Q p)"
check "the car's radio" "radio 0 "    "$($H M1 $start c w3 +C w3 -C w30 +C w3 -C w5 p | sed 's/.*/&/')"
check "arrest costs pay" "cash 225 "  "$($H $start S8 G w3 +A w2 -A w5 U w300 Q p)"
check "siren stops"  "siren 0 "       "$($H $start J7 w2 U w300 p)"
check "heat starts"  "stars 3"        "$($H $start J7 w2 p)"
check "getaway"      "jobs 1 stars 0" "$($H $start J8 w2 c g w4 g w4 p)"
check "getaway heat" "stars 3"        "$($H $start J8 w2 c g w4 p)"
check "shadow found" "job 9/1"        "$($H $start J9 w2 g w3 p)"
check "out of time"  "jobs 0 .*job 0/0" "$($H $start J1 w2 w2500 p)"
check "caught"       "jobs 0 stars 0 heat 0 job 0/0" "$($H $start J7 w2 w3000 p)"
where="$($H W)"
spot() { echo "$where" | sed -n "s/.*$1 \([0-9]*,[0-9]*\).*/\1/p"; }
check "garage"       "owns 1"         "$($H $start '$2000' T$(spot garage) w3 p)"
check "too poor"     "owns 0"         "$($H $start '$100' T$(spot garage) w3 p)"
check "tower"        "owns 2"         "$($H $start '$20000' T$(spot tower) w3 p)"
check "ammo"         "ammo 24"        "$($H $start '$500' T$(spot ammo) w3 p)"
check "ranks"        "rank 2"         "$($H $start '$1600' p)"
check "shooting"     "heat [1-9][0-9]* .*ammo 2[0-3]" "$($H $start m +R w30 -R +A w40 -A p)"
# the story: all twenty missions in a row; the ferry opens with the tenth, the bridge with the fourteenth
q="Q Q Q Q Q Q Q Q Q Q"
story="$($H $start $q p)"
check "story 1-10"   "story 10 "      "$story"
check "ferry opens"  "owns 8 "        "$story"
story="$($H $start $q $q p)"
check "story 11-20"  "story 20 "      "$story"
check "bridge opens" "owns 12 "       "$story"
check "its end"      "THE END"        "$story"
check "auf deutsch"  "ENDE. VORERST"  "$($H D $start $q $q p)"
check "free game"    "owns 12 .*story 0" "$($H M1 $start p)"
pier="$($H W | sed -n 's/.*ferry \([0-9]*,[0-9]*\).*/\1/p')"
check "ferry closed" "AUSSER BETRIEB\|OUT OF ORDER" "$($H $start T$pier w3 p)"
check "ferry"        "pos [0-9]*,1[6-9][0-9][0-9] " "$($H M1 $start '$50' T$pier w120 p)"
# every part can be left out
for part in STORY ISLANDS MUSIC SPEECH ART; do
    if RT_FLAGS="-DRT_$part=0" "$HIER/tools/host.sh" 2>&1 | grep -q .; then echo "FAIL  without $part"; fail=1; else echo "ok    builds without $part"; fi
done
check "plain game"   "jobs 1 "        "$(RT_FLAGS='-DRT_STORY=0 -DRT_ISLANDS=0 -DRT_MUSIC=0 -DRT_SPEECH=0 -DRT_ART=0' "$HIER/tools/host.sh" && $H $start J1 w2 g w2 g w2 p)"
"$HIER/tools/host.sh"
out="$($H w3000 x w3000 x w3000 x)"
check "traffic moves" "[2-9] moving\|1[0-9] moving" "$(echo "$out" | tail -1)"
if echo "$out" | grep -q "NaN\|[4-9] blocked\|[2-9] wrecked"; then echo "FAIL  traffic: $out"; fail=1; else echo "ok    traffic stays whole"; fi
exit $fail
