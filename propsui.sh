#!/bin/bash
# usage: propsui.sh BIN IMAGE   -- Ctrl+I behaviour on the real binary, with controls
export DISPLAY=:99 GDK_BACKEND=x11 GSK_RENDERER=cairo NO_AT_BRIDGE=1
BIN=$1; IMG=$2; D=/tmp/pui; rm -rf $D; mkdir -p $D
pkill Xvfb 2>/dev/null; sleep 0.5; Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 2
FAILS=0
chk() { if [ "$2" = 1 ]; then echo "PASS  $1"; else echo "FAIL  $1"; FAILS=$((FAILS+1)); fi; }
launch() { $BIN "$@" >$D/app.log 2>&1 & APP=$!
  for i in $(seq 1 30); do W=$(xdotool search --onlyvisible --pid $APP 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.5; done
  xdotool windowfocus $W; sleep 2; }
nwin() { xdotool search --onlyvisible --pid $APP 2>/dev/null | wc -l; }

echo "=== with an image loaded"
launch "$IMG"
N0=$(nwin); echo "windows at start: $N0"
xdotool key --window $W i; sleep 1;            N1=$(nwin)
chk "plain 'i' does nothing (control)" $([ "$N1" = "$N0" ] && echo 1 || echo 0)
xdotool key --window $W ctrl+i; sleep 3;       N2=$(nwin)
chk "Ctrl+I opens the Properties window" $([ "$N2" -gt "$N0" ] && echo 1 || echo 0)
import -window root $D/props.png
other() { xdotool search --onlyvisible --pid $APP 2>/dev/null | grep -v "^$W$" | head -1; }
PW=$(other)
chk "a second window exists and is titled 'Properties: ...'" $([ -n "$PW" ] && [[ "$(xdotool getwindowname $PW)" == Properties:* ]] && echo 1 || echo 0)
echo "   title: $(xdotool getwindowname $PW 2>/dev/null)"
xdotool windowfocus $W 2>/dev/null; xdotool key --window $W ctrl+i; sleep 3; N3=$(nwin)
chk "Ctrl+I again replaces it (window count unchanged)" $([ "$N3" = "$N2" ] && echo 1 || echo 0)
PW=$(other)
xdotool windowfocus $PW; sleep 0.5; xdotool key Escape; sleep 1.5; N4=$(nwin)
chk "Escape closes it" $([ "$N4" = "$N0" ] && echo 1 || echo 0)
kill $APP 2>/dev/null; sleep 1

echo "=== no image loaded (negative control)"
launch
M0=$(nwin)
xdotool key --window $W ctrl+i; sleep 2; M1=$(nwin)
chk "Ctrl+I with no image opens nothing" $([ "$M1" = "$M0" ] && echo 1 || echo 0)
kill $APP 2>/dev/null; pkill Xvfb
echo "FAILURES: $FAILS"
