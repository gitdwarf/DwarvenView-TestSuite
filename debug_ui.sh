#!/bin/bash
export DISPLAY=:99 GDK_BACKEND=x11 GSK_RENDERER=cairo NO_AT_BRIDGE=1
BIN=$1; rm -rf /tmp/debugcfg /tmp/debugcache; export XDG_CONFIG_HOME=/tmp/debugcfg XDG_CACHE_HOME=/tmp/debugcache
pkill Xvfb 2>/dev/null; sleep 0.5; Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 2
FAILS=0
chk() { if [ "$2" = 1 ]; then echo "PASS  $1"; else echo "FAIL  $1"; FAILS=$((FAILS+1)); fi; }
open_about() {
  xdotool mousemove 262 12; sleep 0.3; xdotool click 1; sleep 1
  xdotool mousemove 262 75; sleep 0.4; xdotool click 1; sleep 2
}
LOG=/tmp/debugcache/dwarvenview/debug.log
CFG=/tmp/debugcfg/dwarvenview/dwarvenview.conf

$BIN /tmp/props/photo.jpg >/dev/null 2>&1 & APP=$!
for i in $(seq 1 30); do W=$(xdotool search --onlyvisible --pid $APP 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.5; done
xdotool windowfocus $W; sleep 2
chk "no log before enabling" $([ ! -f "$LOG" ] && echo 1 || echo 0)
open_about
AW=$(xdotool search --onlyvisible --pid $APP | grep -v "^$W$" | head -1)
xdotool windowfocus $AW; sleep 0.3
xdotool mousemove --window $AW 27 196; sleep 0.3; xdotool click 1; sleep 1
xdotool mousemove --window $AW 417 235; sleep 0.3; xdotool click 1; sleep 1   # Close button
chk "config now says debug enabled=true" $(grep -qi "enabled=true" "$CFG" 2>/dev/null && echo 1 || echo 0)
kill $APP 2>/dev/null; sleep 0.5

$BIN /tmp/props/anim.gif >/dev/null 2>&1 & APP=$!
for i in $(seq 1 30); do W=$(xdotool search --onlyvisible --pid $APP 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.5; done
xdotool windowfocus $W; sleep 3
chk "log file created by real decode of anim.gif" $([ -f "$LOG" ] && echo 1 || echo 0)
chk "log mentions load_thread_func" $(grep -q "load_thread_func" "$LOG" 2>/dev/null && echo 1 || echo 0)
chk "log mentions frame decode loop" $(grep -q "entering frame decode loop" "$LOG" 2>/dev/null && echo 1 || echo 0)
chk "log mentions avformat_open_input" $(grep -q "avformat_open_input" "$LOG" 2>/dev/null && echo 1 || echo 0)
LC=$(wc -l < "$LOG" 2>/dev/null || echo 0)
echo "   log line count after one GIF load: $LC"
kill $APP 2>/dev/null; sleep 0.5

$BIN /tmp/props/photo.jpg >/dev/null 2>&1 & APP=$!
for i in $(seq 1 30); do W=$(xdotool search --onlyvisible --pid $APP 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.5; done
xdotool windowfocus $W; sleep 2
open_about
AW=$(xdotool search --onlyvisible --pid $APP | grep -v "^$W$" | head -1)
xdotool windowfocus $AW; sleep 0.3
xdotool mousemove --window $AW 27 196; sleep 0.3; xdotool click 1; sleep 1    # uncheck
chk "log file deleted the moment box is unchecked" $([ ! -f "$LOG" ] && echo 1 || echo 0)
chk "config now says debug enabled=false" $(grep -qi "enabled=false" "$CFG" 2>/dev/null && echo 1 || echo 0)
kill $APP 2>/dev/null; pkill Xvfb
echo "FAILURES: $FAILS"
