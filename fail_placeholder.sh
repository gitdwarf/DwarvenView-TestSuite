#!/bin/bash
# fail_placeholder.sh BIN -- a file that is not an image shows the DV placeholder graphic plus
# "Not a valid image file", whatever the loading splash settings are (on, off, or delayed).
# Measured as the number of distinct grey levels in the viewport: text on black is ~190, graphic ~245.
# needs: Xvfb, import (ImageMagick), python3 PIL
. "$(dirname "$0")/uilib.sh"
BIN=$1
D=/tmp/failph; rm -rf $D; mkdir -p $D
printf 'this is not an image\n' > $D/bad.jpg
levels() {   # levels SPLASH DELAY -> distinct grey levels in the viewport after a failed open
    export XDG_CONFIG_HOME=$D/cfg_$1_$2; mkdir -p $XDG_CONFIG_HOME/dwarvenview
    printf '[dwarvenview]\nshow_loading_splash=%s\nloading_splash_delay=%s\n' $1 $2 > $XDG_CONFIG_HOME/dwarvenview/dwarvenview.conf
    start_xvfb 1280x720
    $BIN $D/bad.jpg >/dev/null 2>&1 & APP=$!
    sleep 3; import -window root $D/s.png
    kill $APP 2>/dev/null; pkill Xvfb; sleep 0.5
    python3 -W ignore -c "
from PIL import Image
print(len(Image.open('$D/s.png').convert('L').crop((0,64,1024,698)).getcolors(1<<20)))"
}
FAILS=0
for c in "true 0" "false 0" "true 1.5"; do set -- $c
    n=$(levels $1 $2)
    chk "failed open shows the placeholder graphic (splash=$1 delay=$2, $n levels)" "$([ "$n" -gt 220 ] && echo 1 || echo 0)"
done
echo "FAILURES: $FAILS"; [ $FAILS = 0 ]
