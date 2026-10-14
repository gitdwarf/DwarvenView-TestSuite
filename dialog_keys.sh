#!/bin/bash
# dialog_keys.sh BIN -- one keyboard rule for every dialog: Enter accepts, Escape cancels.
# About, Help, Properties: both close. Preferences: Enter saves, Escape discards. Also in About, Space toggles the
# debug-log checkbox (focus starts on it).
# Coordinates are for the default 1024x768 window: Help menu at 244,12; its items at 300,45 (Help) and 300,75 (About).
# needs: Xvfb, xdotool
. "$(dirname "$0")/uilib.sh"
BIN=$1
D=/tmp/aboutkeys; rm -rf $D; mkdir -p $D
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=320x240:rate=1 -frames:v 1 $D/a.jpg
export XDG_CONFIG_HOME=$D/cfg XDG_CACHE_HOME=$D/cache
CONF=$XDG_CONFIG_HOME/dwarvenview/dwarvenview.conf
mkdir -p $XDG_CONFIG_HOME
start_xvfb 1280x720
start_app $BIN $D/a.jpg

win() { xdotool search --onlyvisible --name "$1" 2>/dev/null | head -1; }
open_from_menu() { # $1 = item y
  xdotool mousemove 244 12 click 1; sleep 0.7; xdotool mousemove 300 $1 click 1; sleep 1; }

for item in "75 About DwarvenView" "45 DwarvenView Help"; do
  set -- $item; y=$1; shift; name="$*"
  for key in Escape Return KP_Enter; do
    open_from_menu $y
    chk "$name opens" "$([ -n "$(win "$name")" ] && echo 1 || echo 0)"
    xdotool key $key; sleep 0.7
    chk "$name closes on $key" "$([ -z "$(win "$name")" ] && echo 1 || echo 0)"
  done
done

open_from_menu 75
before=$(grep -c "=true" $CONF 2>/dev/null)
xdotool key space; sleep 0.7
on=$(grep -c "=true" $CONF 2>/dev/null)
chk "About: Space turns the debug log on" "$([ "${on:-0}" -gt "${before:-0}" ] && echo 1 || echo 0)"
xdotool key space; sleep 0.7
off=$(grep -c "=true" $CONF 2>/dev/null)
chk "About: Space turns it off again" "$([ "${off:-0}" -eq "${before:-0}" ] && echo 1 || echo 0)"
xdotool key Escape; sleep 0.5

# Properties: Ctrl+I; Escape and Enter both close it
for key in Escape Return; do
  xdotool windowfocus $W; xdotool key ctrl+i; sleep 2
  PW=$(xdotool search --onlyvisible --name "Properties" | head -1)
  chk "Properties opens" "$([ -n "$PW" ] && echo 1 || echo 0)"
  xdotool key $key; sleep 0.7
  chk "Properties closes on $key" "$([ -z "$(xdotool search --onlyvisible --name Properties | head -1)" ] && echo 1 || echo 0)"
done

# Preferences: tick the media option, then Escape must discard and Enter must save
rm -f $CONF.bak; grep -v "browse_media" $CONF > /dev/null 2>&1
prefs_tick() { xdotool windowfocus $W; xdotool mousemove 964 44 click 1; sleep 2
  xdotool mousemove 148 34 click 1; sleep 1; xdotool mousemove 42 104 click 1; sleep 1; }
prefs_tick; xdotool key Escape; sleep 1
chk "Preferences: Escape closes it" "$([ -z "$(xdotool search --onlyvisible --name 'DwarvenView Preferences' | head -1)" ] && echo 1 || echo 0)"
chk "Preferences: Escape does not save the ticked option" "$(grep -q '^browse_media=true' $CONF 2>/dev/null && echo 0 || echo 1)"
prefs_tick; xdotool key Return; sleep 1.5
chk "Preferences: Enter closes it" "$([ -z "$(xdotool search --onlyvisible --name 'DwarvenView Preferences' | head -1)" ] && echo 1 || echo 0)"
chk "Preferences: Enter saves the ticked option" "$(grep -q '^browse_media=true' $CONF 2>/dev/null && echo 1 || echo 0)"
finish
