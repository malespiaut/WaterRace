#!/bin/sh
# Syntax-check every game source file against Pomme and the port stubs.
# port/excluded.txt lists files skipped; port/permissive.txt lists files built with -fpermissive.
# Usage: port/check-compile.sh [OUTDIR]
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-${SCRATCH:-/tmp}/compile}
mkdir -p "$OUT"
find "$ROOT/Source" -type d | while IFS= read -r d; do printf -- '-I%s\n' "$d"; done > "$OUT/includes.txt"
grep -v '^#' "$ROOT/port/excluded.txt" | grep . | sed "s|^|$ROOT/Source/|" > "$OUT/excluded-abs.txt"
grep -v '^#' "$ROOT/port/permissive.txt" | grep . | sed "s|^|$ROOT/Source/|" > "$OUT/permissive-abs.txt"
find "$ROOT/Source" \( -name "*.cp" -o -name "*.c" -o -name "*.cpp" \) | sort | grep -v -x -F -f "$OUT/excluded-abs.txt" |
while IFS= read -r f; do
  flags=""; grep -qx -F "$f" "$OUT/permissive-abs.txt" && flags="-fpermissive"
  name=$(echo "$f" | sed "s|$ROOT/Source/||; s|/|__|g; s| |_|g")
  xargs -a "$OUT/includes.txt" -d '\n' \
    g++ -std=gnu++17 -fsyntax-only -w -DPOMME_NO_GRAPHICS -DPOMME_NO_SOUND_MIXER -DPOMME_NO_VIDEO -DPOMME_NO_QD3D -I"$ROOT/port/stubs" -I"$ROOT/Pomme/src" -include MacHeadersPPC++ -x c++ $flags "$f" \
    > "$OUT/$name.log" 2>&1 && echo "OK   $f" || echo "FAIL $f ($(grep -c 'error:' "$OUT/$name.log") errors)"
done
