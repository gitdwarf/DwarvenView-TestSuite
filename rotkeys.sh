#!/bin/bash
# usage: rotkeys.sh BIN   -- behavioural test of R / Shift+R / L / Shift+L / H / V under Xvfb
export DISPLAY=:99 GDK_BACKEND=x11 GSK_RENDERER=cairo NO_AT_BRIDGE=1
BIN=$1; D=/tmp/rk; rm -rf $D; mkdir -p $D
pkill Xvfb 2>/dev/null; sleep 0.5; Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 2
ffmpeg -loglevel error -y -f lavfi -i "testsrc=size=480x160:rate=1" -frames:v 1 $D/wide.png

launch() { $BIN "$@" >$D/app.log 2>&1 & APP=$!
  for i in $(seq 1 30); do W=$(xdotool search --onlyvisible --pid $APP 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.5; done
  xdotool windowfocus $W; sleep 1.5; }
key()  { xdotool key --window $W "$@"; sleep 0.6; }
shot() { import -window root $D/$1.png; }
px()   { compare -metric AE $D/$1.png $D/$2.png null: 2>&1 | awk '{print $1}'; }
chk()  { # label expr(0/1)
  if [ "$2" = "1" ]; then echo "PASS  $1"; else echo "FAIL  $1"; FAILS=$((FAILS+1)); fi; }
FAILS=0

launch $D/wide.png
shot base
key r;           shot cw          # CW
key l;           shot back1       # CCW  -> back to base
key l;           shot ccw         # CCW
key r;           shot back2       # CW   -> back to base
key shift+r;     shot sr          # CCW via Shift+R
key shift+l;     shot back3       # CW   -> back to base
key shift+l;     shot sl          # CW via Shift+L
key shift+r;     shot back4       # CCW  -> base
key h;           shot fh
key h;           shot fh2
key v;           shot fv
key v;           shot fv2
key ctrl+r;      shot ctrlr

chk "R rotates (differs from base)"                 $([ "$(px base cw)" -gt 1000 ] && echo 1 || echo 0)
chk "CW and CCW differ from each other"             $([ "$(px cw ccw)" -gt 1000 ] && echo 1 || echo 0)
chk "R then L returns exactly to base"              $([ "$(px base back1)" = 0 ] && echo 1 || echo 0)
chk "L then R returns exactly to base"              $([ "$(px base back2)" = 0 ] && echo 1 || echo 0)
chk "Shift+R == L  (both counter-clockwise)"        $([ "$(px sr ccw)" = 0 ] && echo 1 || echo 0)
chk "Shift+L == R  (both clockwise)"                $([ "$(px sl cw)" = 0 ] && echo 1 || echo 0)
chk "Shift+L undoes Shift+R"                        $([ "$(px base back3)" = 0 ] && echo 1 || echo 0)
chk "H flips (differs from base)"                   $([ "$(px base fh)" -gt 1000 ] && echo 1 || echo 0)
chk "H twice returns to base"                       $([ "$(px base fh2)" = 0 ] && echo 1 || echo 0)
chk "V flips (differs from base)"                   $([ "$(px base fv)" -gt 1000 ] && echo 1 || echo 0)
chk "V twice returns to base"                       $([ "$(px base fv2)" = 0 ] && echo 1 || echo 0)
chk "Ctrl+R does NOT rotate"                        $([ "$(px base ctrlr)" = 0 ] && echo 1 || echo 0)
kill $APP 2>/dev/null; sleep 1

# no image loaded: keys must do nothing
launch; sleep 2
key shift+x                 # warm-up: first Shift key makes GTK draw a focus ring (pre-existing, unrelated)
shot empty0
key r; key l; key shift+r; key shift+l; key h; key v; shot empty1
chk "no image loaded: R L Shift+R Shift+L H V change nothing" $([ "$(px empty0 empty1)" = 0 ] && echo 1 || echo 0)
kill $APP 2>/dev/null; pkill Xvfb
echo "FAILURES: $FAILS"
