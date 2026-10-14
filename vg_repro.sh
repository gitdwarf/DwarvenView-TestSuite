#!/bin/bash
# Run the real app under valgrind inside Xvfb, then send keys and report liveness.
# usage: BIN=/path/to/dwarvenview [OUT=/tmp/vg] vg_repro.sh TOOL IMAGE [KEY...]
#   e.g. BIN=build/dwarvenview vg_repro.sh memcheck a.png Next
# Use a debug build (meson setup build-dbg --buildtype=debug -Doptimization=0) for readable traces.
export DISPLAY=:99 GDK_BACKEND=x11 GSK_RENDERER=cairo NO_AT_BRIDGE=1
: "${BIN:?set BIN to the dwarvenview binary}"
OUT=${OUT:-/tmp/vg}; mkdir -p "$OUT"
TOOL=$1; IMG=$2; shift 2
pkill -x Xvfb 2>/dev/null; sleep 0.5; Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 2
rm -f "$OUT/vg.$TOOL.log"
valgrind --tool=$TOOL --log-file="$OUT/vg.$TOOL.log" --error-limit=no --num-callers=25 "$BIN" "$IMG" >"$OUT/app.$TOOL.out" 2>&1 & APP=$!
for i in $(seq 1 120); do W=$(xdotool search --onlyvisible --name "$(basename "$IMG")" 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 1; done
echo "window=$W after ${i}s"; sleep 8
for k in "$@"; do xdotool key --window "$W" "$k"; sleep 12; echo "after $k: alive=$(kill -0 $APP 2>/dev/null && echo yes || echo NO)"; done
kill $APP 2>/dev/null; sleep 3; pkill -x Xvfb
grep -c "Invalid write" "$OUT/vg.$TOOL.log" | sed 's/^/invalid writes: /'; grep "ERROR SUMMARY" "$OUT/vg.$TOOL.log" | tail -1
