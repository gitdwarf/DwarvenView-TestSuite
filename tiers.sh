#!/bin/bash
export DISPLAY=:99 GDK_BACKEND=x11 GSK_RENDERER=cairo NO_AT_BRIDGE=1
BIN=$1; IMG=$2; ORIG=$PATH; mkdir -p /tmp/pui
tier() { # $1=name $2=csv
  local d=/tmp/tier_$1; rm -rf $d; mkdir -p $d
  for t in $(echo $2 | tr ',' ' '); do ln -s "$(PATH=$ORIG command -v $t)" $d/$t; done
  pkill Xvfb 2>/dev/null; sleep 0.5; Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 2
  PATH=$d $BIN $IMG >/dev/null 2>&1 & APP=$!
  for i in $(seq 1 30); do W=$(xdotool search --onlyvisible --pid $APP 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.5; done
  xdotool windowfocus $W; sleep 2
  xdotool key --window $W ctrl+i; sleep 4
  PW=$(xdotool search --onlyvisible --pid $APP | grep -v "^$W$" | head -1)
  import -window $PW /tmp/pui/tier_$1.png 2>/dev/null || import -window root -crop 620x580+0+0 /tmp/pui/tier_$1.png
  echo "$1: window '$(xdotool getwindowname $PW)'"
  kill $APP; pkill Xvfb; sleep 1
}
tier exiftool  exiftool
tier exiv2     exiv2
tier mediainfo mediainfo
tier ffprobe   ffprobe
tier none      ""
