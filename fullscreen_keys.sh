#!/bin/bash
# fullscreen_keys.sh BIN -- fullscreen from the keyboard, and the "image only" option.
# F11 toggles; Escape leaves fullscreen once there is no zoom left to undo (first Escape steps the zoom back);
# with fullscreen_hide_ui set, fullscreen shows no menu, toolbar or status bar and leaving brings them back.
# Needs a window manager to honour fullscreen: uses openbox. needs: Xvfb, xdotool, openbox, import, python3 PIL, ffmpeg
. "$(dirname "$0")/uilib.sh"
BIN=$1
D=/tmp/fskeys; rm -rf $D; mkdir -p $D
ffmpeg -loglevel error -y -f lavfi -i color=c=red:size=640x480 -frames:v 1 $D/a.png
export XDG_CONFIG_HOME=$D/cfg; mkdir -p $XDG_CONFIG_HOME/dwarvenview
CONF=$XDG_CONFIG_HOME/dwarvenview/dwarvenview.conf

shot_corner() { # mean brightness 0..255 of the top-left 100x60 of the screen
  import -window root $D/s.png
  python3 -W ignore -c "
from PIL import Image, ImageStat
print(int(ImageStat.Stat(Image.open('$D/s.png').convert('L').crop((0,0,100,60))).mean[0]))"; }
width() { xdotool getwindowgeometry $W | awk '/Geometry/{print $2}' | cut -dx -f1; }
is_fs() { [ "$(width)" = 1280 ]; }

run() { # $1 = fullscreen_hide_ui true/false
  printf '[dwarvenview]\nfullscreen_hide_ui=%s\n' $1 > $CONF
  start_xvfb 1280x720; openbox >/dev/null 2>&1 & sleep 1
  start_app $BIN $D/a.png
}
stop() { kill $APP 2>/dev/null; pkill openbox; pkill Xvfb; sleep 0.5; }

run false
chk "starts windowed" "$(is_fs && echo 0 || echo 1)"
xdotool key F11; sleep 1.5
chk "F11 enters fullscreen" "$(is_fs && echo 1 || echo 0)"
chk "default: controls stay visible in fullscreen" "$([ "$(shot_corner)" -gt 150 ] && echo 1 || echo 0)"
xdotool key Escape; sleep 1.5
chk "Escape leaves fullscreen" "$(is_fs && echo 0 || echo 1)"
xdotool key ctrl+plus; sleep 0.5; xdotool key F11; sleep 1.5
xdotool key Escape; sleep 1.2
chk "Escape with a zoom to undo steps the zoom back first (still fullscreen)" "$(is_fs && echo 1 || echo 0)"
xdotool key Escape; sleep 1.5
chk "the next Escape leaves fullscreen" "$(is_fs && echo 0 || echo 1)"
xdotool key F11; sleep 1.5; xdotool key F11; sleep 1.5
chk "F11 leaves fullscreen too" "$(is_fs && echo 0 || echo 1)"
stop

run true
xdotool key F11; sleep 1.5
chk "image only: fullscreen hides the menu and toolbar" "$([ "$(shot_corner)" -lt 30 ] && echo 1 || echo 0)"
xdotool key Escape; sleep 1.5
chk "image only: Escape leaves fullscreen" "$(is_fs && echo 0 || echo 1)"
chk "image only: leaving brings the controls back" "$([ "$(shot_corner)" -gt 150 ] && echo 1 || echo 0)"
finish
