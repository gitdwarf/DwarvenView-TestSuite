#!/bin/bash
# prefs_ui.sh BIN -- the Preferences dialog in the real app, on a small (1280x720) screen:
# it must fit without scrolling, and ticking an option and pressing Save must persist it.
# Coordinates are for the default 1024x768 window and the dialog placed at 0,0 (no window manager).
# needs: Xvfb, xdotool, ffmpeg
. "$(dirname "$0")/uilib.sh"
BIN=$1
D=/tmp/pfuitest; rm -rf $D; mkdir -p $D
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=320x240:rate=1 -frames:v 1 $D/a.jpg
export XDG_CONFIG_HOME=$D/cfg
CONF=$XDG_CONFIG_HOME/dwarvenview/dwarvenview.conf
start_xvfb 1280x720
start_app $BIN $D/a.jpg

xdotool mousemove 964 44 click 1; sleep 2                       # the toolbar's Preferences button
P=$(xdotool search --name "DwarvenView Preferences" | head -1)
chk "the Preferences window opened" "$([ -n "$P" ] && echo 1 || echo 0)"
GEO=$(xdotool getwindowgeometry $P | awk '/Geometry/{print $2}'); PW=${GEO%x*}; PH=${GEO#*x}
chk "it fits a 720p screen without scrolling (${GEO})" "$([ "${PH:-9999}" -le 480 ] && [ "${PW:-9999}" -le 520 ] && echo 1 || echo 0)"
chk "nothing is saved until Save is pressed" "$([ ! -f $CONF ] && echo 1 || echo 0)"

xdotool mousemove 148 34 click 1; sleep 1                       # Browsing tab
xdotool mousemove 42 104 click 1; sleep 1                       # "Also browse audio and video files"
xdotool mousemove 391 326 click 1; sleep 1.5                    # Save
chk "ticking the option and pressing Save writes browse_media=true" "$(grep -q '^browse_media=true' $CONF 2>/dev/null && echo 1 || echo 0)"
finish
