#!/usr/bin/env bash
#
# Baut das whd_gen-Konverter-Binary fuer die verfuegbaren Plattformen nach bin/.
#
#   Linux   : nativer  g++                     -> bin/whd_gen-linux-x86_64
#   Windows : x86_64-w64-mingw32-g++ (mingw)   -> bin/whd_gen-windows-x86_64.exe
#   macOS   : nativer  clang++ (auf einem Mac) -> bin/whd_gen-macos-<arch>
#
# Es wird nur gebaut, was der jeweilige Compiler hergibt. Fehlt einer, wird
# das Ziel uebersprungen (mit Hinweis). Auf einem Mac ausgefuehrt entsteht die
# macOS-Binary; unter Linux mit mingw-w64 die Windows-.exe.
#
# mingw installieren (Debian/Ubuntu):  sudo apt-get install -y mingw-w64
#
set -u

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/../picoboy-doom/src"
OUT="$ROOT/bin"
mkdir -p "$OUT"

if [ ! -d "$SRC/whd_gen" ]; then
    echo "FEHLER: Quellen nicht gefunden unter $SRC/whd_gen" >&2
    echo "Erwartet wird der Quellbaum unter ../picoboy-doom/ — also das" >&2
    echo "Skript aus doom/wad2uf2/ heraus aufrufen." >&2
    exit 1
fi

CPPSRC="$SRC/whd_gen/whd_gen.cpp $SRC/whd_gen/mus2seq.cpp $SRC/whd_gen/huff.cpp \
$SRC/whd_gen/lodepng.cpp $SRC/whd_gen/compress_mus.cpp $SRC/whd_gen/wad.cpp \
$SRC/whd_gen/win_gui.cpp \
$SRC/tiny_huff.c $SRC/musx_decoder.c $SRC/image_decoder.c"
ADPCM="$SRC/adpcm-xq/adpcm-lib.c"
INC="-I$SRC/whd_gen -I$SRC -I$SRC/doom -I$SRC/adpcm-xq"
CXXFLAGS="-O2 -w -fpermissive -std=gnu++14 -include cstdint -include cstdio -include $ROOT/whd_gen_compat.h -DIS_WHD_GEN=1 $INC"

build() {   # $1=label  $2=CC  $3=CXX  $4=outfile  $5=extra-ldflags
    local label="$1" CC="$2" CXX="$3" out="$4" extra="$5"
    if ! command -v "$CXX" >/dev/null 2>&1; then
        echo "[skip] $label  ($CXX nicht gefunden)"
        return
    fi
    echo "[build] $label -> $(basename "$out")"
    local tmpo; tmpo="$(mktemp --suffix=.o)"
    "$CC" -O2 -w -c "$ADPCM" -o "$tmpo" || { echo "  adpcm-lib fehlgeschlagen"; rm -f "$tmpo"; return; }
    if "$CXX" $CXXFLAGS $CPPSRC "$tmpo" $extra -o "$out"; then
        echo "  OK: $out"
    else
        echo "  FEHLER beim Linken von $label"
    fi
    rm -f "$tmpo"
}

# --- Linux (nativ) ---
build "Linux x86_64"   gcc  g++ \
      "$OUT/whd_gen-linux-x86_64"  "-static -static-libgcc -static-libstdc++"

# --- Windows (mingw-w64 cross) --- comdlg32/user32 = eingebauter Datei-Dialog
build "Windows x86_64" x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++ \
      "$OUT/whd_gen-windows-x86_64.exe" \
      "-static -static-libgcc -static-libstdc++ -lcomdlg32 -luser32"

# --- macOS (nur auf einem Mac; native arch) ---
if [ "$(uname -s)" = "Darwin" ]; then
    ARCH="$(uname -m)"   # arm64 oder x86_64
    build "macOS $ARCH" clang clang++ "$OUT/whd_gen-macos-$ARCH" ""
else
    echo "[info] macOS-Binary nur auf einem Mac baubar (dort dieses Skript ausfuehren)."
fi

echo ""
echo "Fertig. Inhalt von bin/:"
ls -la "$OUT"
