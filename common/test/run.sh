#!/usr/bin/env bash
# Links the protocols of common/ twice (a_, b_) with the fake link and runs them.
set -eu
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

twice() {   # source, then its public symbols
    local src="$1"; shift
    gcc -O2 -Wall -DLS_HOST -I"$HIER/.." -c "$HIER/../$src" -o "$OUT/x.o"
    for p in a_ b_; do
        local args=()
        for s in "$@"; do args+=(--redefine-sym "$s=$p$s"); done
        objcopy "${args[@]}" "$OUT/x.o" "$OUT/$p${src%.c}.o"
    done
}

twice pbc_lockstep.c ls_begin ls_end ls_connect ls_frame ls_out_of_sync
gcc -O2 -Wall -pthread -I"$HIER/.." "$HIER/lockstep_test.c" \
    "$OUT/a_pbc_lockstep.o" "$OUT/b_pbc_lockstep.o" -o "$OUT/lockstep"
"$OUT/lockstep"

twice pbc_lobby.c lobby_begin lobby_end lobby_tick lobby_partner lobby_offer \
    lobby_offer_state lobby_offer_cancel lobby_incoming lobby_answer lobby_went \
    lobby_send lobby_send_state lobby_send_cancel lobby_incoming_send lobby_foreign_reply \
    lobby_pause lobby_resume
gcc -O2 -Wall -pthread -I"$HIER/.." "$HIER/lobby_test.c" \
    "$OUT/a_pbc_lobby.o" "$OUT/b_pbc_lobby.o" -o "$OUT/lobby"
"$OUT/lobby"

gcc -O2 -Wall -DLS_HOST -I"$HIER/.." -c "$HIER/../pbc_xfer.c" -o "$OUT/pbc_xfer.o"
gcc -O2 -Wall -pthread -I"$HIER/.." "$HIER/xfer_test.c" "$OUT/pbc_xfer.o" -o "$OUT/xfer"
"$OUT/xfer"
