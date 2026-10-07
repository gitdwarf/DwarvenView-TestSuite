#!/bin/bash
# Runtime key test: real binary, real X server (Xvfb), real synthetic keystrokes.
# Usage: keytest.sh /path/to/dwarvenview KEY [KEY...]
export DISPLAY=:99 GDK_BACKEND=x11 GSK_RENDERER=cairo NO_AT_BRIDGE=1

BIN="$1"; shift
D=/tmp/rt
rm -rf "$D"; mkdir -p "$D"

pkill Xvfb 2>/dev/null; sleep 0.5
Xvfb :99 -screen 0 1280x800x24 >"$D/xvfb.log" 2>&1 &
sleep 2

ffmpeg -loglevel error -y -f lavfi -i "testsrc=size=480x160:rate=1" -frames:v 1 "$D/wide.png"

"$BIN" "$D/wide.png" >"$D/app.log" 2>&1 &
APP=$!

WID=""
for i in $(seq 1 30); do
    WID=$(xdotool search --onlyvisible --pid "$APP" 2>/dev/null | head -1)
    [ -n "$WID" ] && break
    sleep 0.5
done
echo "app pid=$APP window=[$WID]"
if [ -z "$WID" ]; then echo "NO WINDOW"; head -5 "$D/app.log"; kill "$APP" 2>/dev/null; pkill Xvfb; exit 1; fi

xdotool windowfocus "$WID"; sleep 1.5

shot()   { import -window root "$D/$1.png"; }
diffpx() { compare -metric AE "$D/$1.png" "$D/$2.png" null: 2>&1 | awk '{print $1}'; }

shot base
xdotool key --window "$WID" ctrl+plus; sleep 0.8; shot ctrlplus
xdotool key --window "$WID" ctrl+0;    sleep 0.8; shot reset
echo "CONTROL ctrl+plus vs base : $(diffpx base ctrlplus) px differ (expect >0: handled key)"
echo "CONTROL ctrl+0 vs base    : $(diffpx base reset) px differ (expect ~0: reset restores)"

for k in "$@"; do
    xdotool key --window "$WID" "$k"; sleep 0.8
    shot "k_$k"
    echo "KEY '$k' vs reset : $(diffpx reset "k_$k") px differ (>0 means the key did something)"
    # put state back for the next key: undo via same key sequence is not generic,
    # so restore with ctrl+0 (resets zoom) and leave rotation state to the caller
done

kill "$APP" 2>/dev/null; pkill Xvfb
