#!/bin/bash
. "$(dirname "$0")/uilib.sh"
BIN=$1
# own fixtures: three files, so the last step ends somewhere other than where the first began
mkdir -p /tmp/navtest; rm -f /tmp/navtest/*
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=320x240:rate=1 -frames:v 1 /tmp/navtest/a_photo.jpg
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=64x64:rate=5 -t 1 /tmp/navtest/b_anim.gif
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=200x100:rate=1 -frames:v 1 /tmp/navtest/c_wide.png
start_xvfb
start_app $BIN /tmp/navtest/a_photo.jpg
xdotool key --window $W ctrl+i; sleep 3
PW=$(xdotool search --onlyvisible --pid $APP | grep -v "^$W$" | head -1)
chk "Properties opened, titled a_photo.jpg" "$(wait_title $PW 'Properties: a_photo.jpg' 5 && echo 1 || echo 0)"
nav_key Page_Down b_anim.gif
chk "title followed to b_anim.gif after Page_Down" "$(wait_title $PW 'Properties: b_anim.gif' 5 && echo 1 || echo 0)"
nav_key Page_Down c_wide.png
chk "title followed to c_wide.png after Page_Down" "$(wait_title $PW 'Properties: c_wide.png' 5 && echo 1 || echo 0)"
nav_key Page_Up b_anim.gif
chk "title followed back to b_anim.gif after Page_Up" "$(wait_title $PW 'Properties: b_anim.gif' 5 && echo 1 || echo 0)"
finish
